/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VirtualBoxClientImpl.h"

#include "AutoCaller.h"
#include "VBoxEvents.h"
#include "Logging.h"
#include "VBox/com/ErrorInfo.h"

#include <iprt/asm.h>
#include <iprt/thread.h>
#include <iprt/critsect.h>
#include <iprt/semaphore.h>
#include <iprt/cpp/utils.h>


/** Waiting time between probing whether VBoxSVC is alive. */
#define VBOXCLIENT_DEFAULT_INTERVAL 30000


/** Initialize instance counter class variable */
uint32_t VirtualBoxClient::g_cInstances = 0;


// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HRESULT VirtualBoxClient::FinalConstruct()
{
    HRESULT rc = init();
    BaseFinalConstruct();
    return rc;
}

void VirtualBoxClient::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the VirtualBoxClient object.
 *
 * @returns COM result indicator
 */
HRESULT VirtualBoxClient::init()
{
    LogFlowThisFuncEnter();

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Important: DO NOT USE any kind of "early return" (except the single
     * one above, checking the init span success) in this method. It is vital
     * for correct error handling that it has only one point of return, which
     * does all the magic on COM to signal object creation success and
     * reporting the error later for every API method. COM translates any
     * unsuccessful object creation to REGDB_E_CLASSNOTREG errors or similar
     * unhelpful ones which cause us a lot of grief with troubleshooting. */

    HRESULT rc = S_OK;
    try
    {
        if (ASMAtomicIncU32(&g_cInstances) != 1)
            AssertFailedStmt(throw setError(E_FAIL,
                                            tr("Attempted to create more than one VirtualBoxClient instance")));

        mData.m_ThreadWatcher = NIL_RTTHREAD;
        mData.m_SemEvWatcher = NIL_RTSEMEVENT;

        rc = mData.m_pVirtualBox.createLocalObject(CLSID_VirtualBox);
        if (FAILED(rc))
            throw rc;

        /* VirtualBox error return is postponed to method calls, fetch it. */
        ULONG rev;
        rc = mData.m_pVirtualBox->COMGETTER(Revision)(&rev);
        if (FAILED(rc))
            throw rc;

        rc = unconst(mData.m_pEventSource).createObject();
        AssertComRCThrow(rc, setError(rc,
                                      tr("Could not create EventSource for VirtualBoxClient")));
        rc = mData.m_pEventSource->init();
        AssertComRCThrow(rc, setError(rc,
                                      tr("Could not initialize EventSource for VirtualBoxClient")));

        /* Setting up the VBoxSVC watcher thread. If anything goes wrong here it
         * is not considered important enough to cause any sort of visible
         * failure. The monitoring will not be done, but that's all. */
        int vrc = RTSemEventCreate(&mData.m_SemEvWatcher);
        if (RT_FAILURE(vrc))
        {
            mData.m_SemEvWatcher = NIL_RTSEMEVENT;
            AssertRCStmt(vrc, throw setError(VBOX_E_IPRT_ERROR,
                                             tr("Failed to create semaphore (rc=%Rrc)"),
                                             vrc));
        }

        vrc = RTThreadCreate(&mData.m_ThreadWatcher, SVCWatcherThread, this, 0,
                             RTTHREADTYPE_INFREQUENT_POLLER, RTTHREADFLAGS_WAITABLE, "VBoxSVCWatcher");
        if (RT_FAILURE(vrc))
        {
            RTSemEventDestroy(mData.m_SemEvWatcher);
            mData.m_SemEvWatcher = NIL_RTSEMEVENT;
            AssertRCStmt(vrc, throw setError(VBOX_E_IPRT_ERROR,
                                             tr("Failed to create watcher thread (rc=%Rrc)"),
                                             vrc));
        }
    }
    catch (HRESULT err)
    {
        /* we assume that error info is set by the thrower */
        rc = err;
    }
    catch (...)
    {
        rc = VirtualBoxBase::handleUnexpectedExceptions(this, RT_SRC_POS);
    }

    /* Confirm a successful initialization when it's the case. Must be last,
     * as on failure it will uninitialize the object. */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();
    else
        autoInitSpan.setFailed(rc);

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();
    /* Unconditionally return success, because the error return is delayed to
     * the attribute/method calls through the InitFailed object state. */
    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void VirtualBoxClient::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    if (mData.m_ThreadWatcher != NIL_RTTHREAD)
    {
        /* Signal the event semaphore and wait for the thread to terminate.
         * if it hangs for some reason exit anyway, this can cause a crash
         * though as the object will no longer be available. */
        RTSemEventSignal(mData.m_SemEvWatcher);
        RTThreadWait(mData.m_ThreadWatcher, 30000, NULL);
        mData.m_ThreadWatcher = NIL_RTTHREAD;
        RTSemEventDestroy(mData.m_SemEvWatcher);
        mData.m_SemEvWatcher = NIL_RTSEMEVENT;
    }

    mData.m_pVirtualBox.setNull();

    ASMAtomicDecU32(&g_cInstances);
}

// IVirtualBoxClient properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns a reference to the VirtualBox object.
 *
 * @returns COM status code
 * @param   aVirtualBox Address of result variable.
 */
HRESULT VirtualBoxClient::getVirtualBox(ComPtr<IVirtualBox> &aVirtualBox)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    aVirtualBox = mData.m_pVirtualBox;
    return S_OK;
}

/**
 * Create a new Session object and return a reference to it.
 *
 * @returns COM status code
 * @param   aSession    Address of result variable.
 */
HRESULT VirtualBoxClient::getSession(ComPtr<ISession> &aSession)
{
    /* this is not stored in this object, no need to lock */
    ComPtr<ISession> pSession;
    HRESULT rc = pSession.createInprocObject(CLSID_Session);
    if (SUCCEEDED(rc))
        aSession = pSession;
    return rc;
}

/**
 * Return reference to the EventSource associated with this object.
 *
 * @returns COM status code
 * @param   aEventSource    Address of result variable.
 */
HRESULT VirtualBoxClient::getEventSource(ComPtr<IEventSource> &aEventSource)
{
    /* this is const, no need to lock */
    aEventSource = mData.m_pEventSource;
    return aEventSource.isNull() ? E_FAIL : S_OK;
}

// IVirtualBoxClient methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Checks a Machine object for any pending errors.
 *
 * @returns COM status code
 * @param   aMachine    Machine object to check.
 */
HRESULT VirtualBoxClient::checkMachineError(const ComPtr<IMachine> &aMachine)
{
    BOOL fAccessible = FALSE;
    HRESULT rc = aMachine->COMGETTER(Accessible)(&fAccessible);
    if (FAILED(rc))
        return setError(rc, tr("Could not check the accessibility status of the VM"));
    else if (!fAccessible)
    {
        ComPtr<IVirtualBoxErrorInfo> pAccessError;
        rc = aMachine->COMGETTER(AccessError)(pAccessError.asOutParam());
        if (FAILED(rc))
            return setError(rc, tr("Could not get the access error message of the VM"));
        else
        {
            ErrorInfo info(pAccessError);
            ErrorInfoKeeper eik(info);
            return info.getResultCode();
        }
    }
    return S_OK;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/*static*/
DECLCALLBACK(int) VirtualBoxClient::SVCWatcherThread(RTTHREAD ThreadSelf,
                                                     void *pvUser)
{
    NOREF(ThreadSelf);
    Assert(pvUser);
    VirtualBoxClient *pThis = (VirtualBoxClient *)pvUser;
    RTSEMEVENT sem = pThis->mData.m_SemEvWatcher;
    RTMSINTERVAL cMillies = VBOXCLIENT_DEFAULT_INTERVAL;
    int vrc;

    /* The likelihood of early crashes are high, so start with a short wait. */
    vrc = RTSemEventWait(sem, cMillies / 2);

    /* As long as the waiting times out keep retrying the wait. */
    while (RT_FAILURE(vrc))
    {
        {
            HRESULT rc = S_OK;
            ComPtr<IVirtualBox> pV;
            {
                AutoReadLock alock(pThis COMMA_LOCKVAL_SRC_POS);
                pV = pThis->mData.m_pVirtualBox;
            }
            if (!pV.isNull())
            {
                ULONG rev;
                rc = pV->COMGETTER(Revision)(&rev);
                if (FAILED_DEAD_INTERFACE(rc))
                {
                    LogRel(("VirtualBoxClient: detected unresponsive VBoxSVC (rc=%Rhrc)\n", rc));
                    {
                        AutoWriteLock alock(pThis COMMA_LOCKVAL_SRC_POS);
                        /* Throw away the VirtualBox reference, it's no longer
                         * usable as VBoxSVC terminated in the mean time. */
                        pThis->mData.m_pVirtualBox.setNull();
                    }
                    fireVBoxSVCAvailabilityChangedEvent(pThis->mData.m_pEventSource, FALSE);
                }
            }
            else
            {
                /* Try to get a new VirtualBox reference straight away, and if
                 * this fails use an increased waiting time as very frequent
                 * restart attempts in some wedged config can cause high CPU
                 * and disk load. */
                ComPtr<IVirtualBox> pVirtualBox;
                rc = pVirtualBox.createLocalObject(CLSID_VirtualBox);
                if (FAILED(rc))
                    cMillies = 3 * VBOXCLIENT_DEFAULT_INTERVAL;
                else
                {
                    LogRel(("VirtualBoxClient: detected working VBoxSVC (rc=%Rhrc)\n", rc));
                    {
                        AutoWriteLock alock(pThis COMMA_LOCKVAL_SRC_POS);
                        /* Update the VirtualBox reference, there's a working
                         * VBoxSVC again from now on. */
                        pThis->mData.m_pVirtualBox = pVirtualBox;
                    }
                    fireVBoxSVCAvailabilityChangedEvent(pThis->mData.m_pEventSource, TRUE);
                    cMillies = VBOXCLIENT_DEFAULT_INTERVAL;
                }
            }
        }
        vrc = RTSemEventWait(sem, cMillies);
    }
    return 0;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
