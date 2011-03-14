/* $Id$ */
/** @file
 * Implementation of INetworkAdaptor in VBoxSVC.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "NetworkAdapterImpl.h"
#include "NATEngineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"
#include "MachineImpl.h"
#include "GuestOSTypeImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>

#include "AutoStateDep.h"

// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

NetworkAdapter::NetworkAdapter()
    : mParent(NULL)
{
}

NetworkAdapter::~NetworkAdapter()
{
}

HRESULT NetworkAdapter::FinalConstruct()
{

    return BaseFinalConstruct();
}

void NetworkAdapter::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initializes the network adapter object.
 *
 *  @param aParent  Handle of the parent object.
 */
HRESULT NetworkAdapter::init(Machine *aParent, ULONG aSlot)
{
    LogFlowThisFunc(("aParent=%p, aSlot=%d\n", aParent, aSlot));

    ComAssertRet(aParent, E_INVALIDARG);
    ComAssertRet(aSlot < SchemaDefs::NetworkAdapterCount, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    unconst(mNATEngine).createObject();
    mNATEngine->init(aParent, this);
    /* mPeer is left null */

    m_fModified = false;

    mData.allocate();

    /* initialize data */
    mData->mSlot = aSlot;

    /* default to Am79C973 */
    mData->mAdapterType = NetworkAdapterType_Am79C973;

    /* generate the MAC address early to guarantee it is the same both after
     * changing some other property (i.e. after mData.backup()) and after the
     * subsequent mData.rollback(). */
    generateMACAddress();

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the network adapter object given another network adapter object
 *  (a kind of copy constructor). This object shares data with
 *  the object passed as an argument.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT NetworkAdapter::init(Machine *aParent, NetworkAdapter *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    unconst(mPeer) = aThat;
    unconst(mNATEngine).createObject();
    mNATEngine->init(aParent, this, aThat->mNATEngine);

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.share (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Initializes the guest object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 *
 *  @note Locks @a aThat object for reading.
 */
HRESULT NetworkAdapter::initCopy(Machine *aParent, NetworkAdapter *aThat)
{
    LogFlowThisFunc(("aParent=%p, aThat=%p\n", aParent, aThat));

    ComAssertRet(aParent && aThat, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;
    /* mPeer is left null */

    unconst(mNATEngine).createObject();
    mNATEngine->initCopy(aParent, this, aThat->mNATEngine);

    AutoCaller thatCaller (aThat);
    AssertComRCReturnRC(thatCaller.rc());

    AutoReadLock thatLock(aThat COMMA_LOCKVAL_SRC_POS);
    mData.attachCopy (aThat->mData);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void NetworkAdapter::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mData.free();

    unconst(mNATEngine).setNull();
    unconst(mPeer) = NULL;
    unconst(mParent) = NULL;
}

// INetworkAdapter properties
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP NetworkAdapter::COMGETTER(AdapterType) (NetworkAdapterType_T *aAdapterType)
{
    CheckComArgOutPointerValid(aAdapterType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAdapterType = mData->mAdapterType;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(AdapterType) (NetworkAdapterType_T aAdapterType)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* make sure the value is allowed */
    switch (aAdapterType)
    {
        case NetworkAdapterType_Am79C970A:
        case NetworkAdapterType_Am79C973:
#ifdef VBOX_WITH_E1000
        case NetworkAdapterType_I82540EM:
        case NetworkAdapterType_I82543GC:
        case NetworkAdapterType_I82545EM:
#endif
#ifdef VBOX_WITH_VIRTIO
        case NetworkAdapterType_Virtio:
#endif /* VBOX_WITH_VIRTIO */
            break;
        default:
            return setError(E_FAIL,
                            tr("Invalid network adapter type '%d'"),
                            aAdapterType);
    }

    if (mData->mAdapterType != aAdapterType)
    {
        mData.backup();
        mData->mAdapterType = aAdapterType;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Changing the network adapter type during runtime is not allowed,
         * therefore no immediate change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(Slot) (ULONG *aSlot)
{
    CheckComArgOutPointerValid(aSlot);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSlot = mData->mSlot;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mData->mEnabled;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mEnabled != aEnabled)
    {
        mData.backup();
        mData->mEnabled = aEnabled;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Disabling the network adapter during runtime is not allowed
         * therefore no immediate change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(MACAddress)(BSTR *aMACAddress)
{
    CheckComArgOutPointerValid(aMACAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComAssertRet(!mData->mMACAddress.isEmpty(), E_FAIL);

    mData->mMACAddress.cloneTo(aMACAddress);

    return S_OK;
}

HRESULT NetworkAdapter::updateMacAddress(Utf8Str aMACAddress)
{
    HRESULT rc = S_OK;

    /*
     * Are we supposed to generate a MAC?
     */
    if (aMACAddress.isEmpty())
        generateMACAddress();
    else
    {
        if (mData->mMACAddress != aMACAddress)
        {
            /*
             * Verify given MAC address
             */
            char *macAddressStr = aMACAddress.mutableRaw();
            int i = 0;
            while ((i < 13) && macAddressStr && *macAddressStr && (rc == S_OK))
            {
                char c = *macAddressStr;
                /* canonicalize hex digits to capital letters */
                if (c >= 'a' && c <= 'f')
                {
                    /** @todo the runtime lacks an ascii lower/upper conv */
                    c &= 0xdf;
                    *macAddressStr = c;
                }
                /* we only accept capital letters */
                if (((c < '0') || (c > '9')) &&
                    ((c < 'A') || (c > 'F')))
                    rc = setError(E_INVALIDARG, tr("Invalid MAC address format"));
                /* the second digit must have even value for unicast addresses */
                if ((i == 1) && (!!(c & 1) == (c >= '0' && c <= '9')))
                    rc = setError(E_INVALIDARG, tr("Invalid MAC address format"));

                macAddressStr++;
                i++;
            }
            /* we must have parsed exactly 12 characters */
            if (i != 12)
                rc = setError(E_INVALIDARG, tr("Invalid MAC address format"));

            if (SUCCEEDED(rc))
                mData->mMACAddress = aMACAddress;
        }
    }

    return rc;
}

STDMETHODIMP NetworkAdapter::COMSETTER(MACAddress)(IN_BSTR aMACAddress)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();


    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    mData.backup();

    HRESULT rc = updateMacAddress(aMACAddress);
    if (SUCCEEDED(rc))
    {
        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Changing the MAC via the Main API during runtime is not allowed,
         * therefore no immediate change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return rc;
}

STDMETHODIMP NetworkAdapter::COMGETTER(AttachmentType)(
    NetworkAttachmentType_T *aAttachmentType)
{
    CheckComArgOutPointerValid(aAttachmentType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aAttachmentType = mData->mAttachmentType;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(HostInterface)(BSTR *aHostInterface)
{
    CheckComArgOutPointerValid(aHostInterface);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mHostInterface.cloneTo(aHostInterface);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(HostInterface)(IN_BSTR aHostInterface)
{
    Bstr bstrEmpty("");
    if (!aHostInterface)
        aHostInterface = bstrEmpty.raw();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mHostInterface != aHostInterface)
    {
        mData.backup();
        mData->mHostInterface = aHostInterface;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* When changing the host adapter, adapt the CFGM logic to make this
         * change immediately effect and to notify the guest that the network
         * might have changed, therefore changeAdapter=TRUE. */
        mParent->onNetworkAdapterChange(this, TRUE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(InternalNetwork) (BSTR *aInternalNetwork)
{
    CheckComArgOutPointerValid(aInternalNetwork);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mInternalNetwork.cloneTo(aInternalNetwork);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(InternalNetwork) (IN_BSTR aInternalNetwork)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mInternalNetwork != aInternalNetwork)
    {
        /* if an empty/null string is to be set, internal networking must be
         * turned off */
        if (   (aInternalNetwork == NULL || *aInternalNetwork == '\0')
            && mData->mAttachmentType == NetworkAttachmentType_Internal)
        {
            return setError(E_FAIL,
                            tr("Empty or null internal network name is not valid"));
        }

        mData.backup();
        mData->mInternalNetwork = aInternalNetwork;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* When changing the internal network, adapt the CFGM logic to make this
         * change immediately effect and to notify the guest that the network
         * might have changed, therefore changeAdapter=TRUE. */
        mParent->onNetworkAdapterChange(this, TRUE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(NATNetwork) (BSTR *aNATNetwork)
{
    CheckComArgOutPointerValid(aNATNetwork);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mNATNetwork.cloneTo(aNATNetwork);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(NATNetwork) (IN_BSTR aNATNetwork)
{
    Bstr bstrEmpty("");
    if (!aNATNetwork)
        aNATNetwork = bstrEmpty.raw();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mNATNetwork != aNATNetwork)
    {
        mData.backup();
        mData->mNATNetwork = aNATNetwork;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Changing the NAT network isn't allowed during runtime, therefore
         * no immediate replug in CFGM logic => changeAdapter=FALSE */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(VDENetwork) (BSTR *aVDENetwork)
{
#if defined(VBOX_WITH_VDE)
    CheckComArgOutPointerValid(aVDENetwork);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mVDENetwork.cloneTo(aVDENetwork);

    return S_OK;
#else
    NOREF(aVDENetwork);
    return E_NOTIMPL;
#endif
}

STDMETHODIMP NetworkAdapter::COMSETTER(VDENetwork) (IN_BSTR aVDENetwork)
{
#if defined(VBOX_WITH_VDE)
    Bstr bstrEmpty("");
    if (!aVDENetwork)
        aVDENetwork = bstrEmpty.raw();

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep (mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mVDENetwork != aVDENetwork)
    {
        mData.backup();
        mData->mVDENetwork = aVDENetwork;

        /* leave the lock before informing callbacks */
        alock.release();

        mParent->onNetworkAdapterChange (this, FALSE);
    }

    return S_OK;
#else
    NOREF(aVDENetwork);
    return E_NOTIMPL;
#endif
}

STDMETHODIMP NetworkAdapter::COMGETTER(CableConnected) (BOOL *aConnected)
{
    CheckComArgOutPointerValid(aConnected);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aConnected = mData->mCableConnected;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(CableConnected) (BOOL aConnected)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aConnected != mData->mCableConnected)
    {
        mData.backup();
        mData->mCableConnected = aConnected;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* No change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(LineSpeed) (ULONG *aSpeed)
{
    CheckComArgOutPointerValid(aSpeed);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSpeed = mData->mLineSpeed;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(LineSpeed) (ULONG aSpeed)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aSpeed != mData->mLineSpeed)
    {
        mData.backup();
        mData->mLineSpeed = aSpeed;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* No change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}


STDMETHODIMP NetworkAdapter::COMGETTER(PromiscModePolicy)(NetworkAdapterPromiscModePolicy_T *aPromiscModePolicy)
{
    CheckComArgOutPointerValid(aPromiscModePolicy);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        *aPromiscModePolicy = mData->mPromiscModePolicy;
    }
    return hrc;
}

STDMETHODIMP NetworkAdapter::COMSETTER(PromiscModePolicy)(NetworkAdapterPromiscModePolicy_T aPromiscModePolicy)
{
    switch (aPromiscModePolicy)
    {
        case NetworkAdapterPromiscModePolicy_Deny:
        case NetworkAdapterPromiscModePolicy_AllowNetwork:
        case NetworkAdapterPromiscModePolicy_AllowAll:
            break;
        default:
            return setError(E_INVALIDARG, tr("Invalid promiscuous mode policy (%d)"), aPromiscModePolicy);
    }

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();

    if (SUCCEEDED(hrc))
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (aPromiscModePolicy != mData->mPromiscModePolicy)
        {
            mData.backup();
            mData->mPromiscModePolicy = aPromiscModePolicy;
            m_fModified = true;

            alock.release();
            mParent->setModifiedLock(Machine::IsModified_NetworkAdapters);
            mParent->onNetworkAdapterChange(this, TRUE);
        }
    }

    return hrc;
}

STDMETHODIMP NetworkAdapter::COMGETTER(TraceEnabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aEnabled = mData->mTraceEnabled;
    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(TraceEnabled) (BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aEnabled != mData->mTraceEnabled)
    {
        mData.backup();
        mData->mTraceEnabled = aEnabled;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Adapt the CFGM logic changeAdapter=TRUE */
        mParent->onNetworkAdapterChange(this, TRUE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(TraceFile) (BSTR *aTraceFile)
{
    CheckComArgOutPointerValid(aTraceFile);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mTraceFile.cloneTo(aTraceFile);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(TraceFile) (IN_BSTR aTraceFile)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mTraceFile != aTraceFile)
    {
        mData.backup();
        mData->mTraceFile = aTraceFile;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* No change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(NatDriver) (INATEngine **aNatDriver)
{
    CheckComArgOutPointerValid(aNatDriver);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mNATEngine.queryInterfaceTo(aNatDriver);

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMGETTER(BootPriority) (ULONG *aBootPriority)
{
    CheckComArgOutPointerValid(aBootPriority);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aBootPriority = mData->mBootPriority;

    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(BootPriority) (ULONG aBootPriority)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aBootPriority != mData->mBootPriority)
    {
        mData.backup();
        mData->mBootPriority = aBootPriority;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* No change in CFGM logic => changeAdapter=FALSE. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    return S_OK;
}

// INetworkAdapter methods
////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP NetworkAdapter::AttachToNAT()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mAttachmentType != NetworkAttachmentType_NAT)
    {
        mData.backup();

        // Commented this for now as it resets the parameter mData->mNATNetwork
        // which is essential while changing the Attachment dynamically.
        //detach();

        mData->mAttachmentType = NetworkAttachmentType_NAT;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Adapt the CFGM logic and notify the guest => changeAdapter=TRUE. */
        HRESULT rc = mParent->onNetworkAdapterChange(this, TRUE);
        if (FAILED(rc))
        {
            /* If changing the attachment failed then we can't assume
             * that the previous attachment will attach correctly
             * and thus return error along with detaching all
             * attachments.
             */
            Detach();
            return rc;
        }
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToBridgedInterface()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* don't do anything if we're already host interface attached */
    if (mData->mAttachmentType != NetworkAttachmentType_Bridged)
    {
        mData.backup();

        /* first detach the current attachment */
        // Commented this for now as it reset the parameter mData->mHostInterface
        // which is essential while changing the Attachment dynamically.
        //detach();

        mData->mAttachmentType = NetworkAttachmentType_Bridged;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Adapt the CFGM logic and notify the guest => changeAdapter=TRUE. */
        HRESULT rc = mParent->onNetworkAdapterChange(this, TRUE);
        if (FAILED(rc))
        {
            /* If changing the attachment failed then we can't assume that the
             * previous attachment will attach correctly and thus return error
             * along with detaching all attachments.
             */
            Detach();
            return rc;
        }
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToInternalNetwork()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* don't do anything if we're already internal network attached */
    if (mData->mAttachmentType != NetworkAttachmentType_Internal)
    {
        mData.backup();

        /* first detach the current attachment */
        // Commented this for now as it reset the parameter mData->mInternalNetwork
        // which is essential while changing the Attachment dynamically.
        //detach();

        /* there must an internal network name */
        if (mData->mInternalNetwork.isEmpty())
        {
            LogRel (("Internal network name not defined, "
                     "setting to default \"intnet\"\n"));
            mData->mInternalNetwork = "intnet";
        }

        mData->mAttachmentType = NetworkAttachmentType_Internal;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Adapt the CFGM logic and notify the guest => changeAdapter=TRUE. */
        HRESULT rc = mParent->onNetworkAdapterChange(this, TRUE);
        if (FAILED(rc))
        {
            /* If changing the attachment failed then we can't assume
             * that the previous attachment will attach correctly
             * and thus return error along with detaching all
             * attachments.
             */
            Detach();
            return rc;
        }
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToHostOnlyInterface()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* don't do anything if we're already host interface attached */
    if (mData->mAttachmentType != NetworkAttachmentType_HostOnly)
    {
        mData.backup();

        /* first detach the current attachment */
        // Commented this for now as it reset the parameter mData->mHostInterface
        // which is essential while changing the Attachment dynamically.
        //detach();

        mData->mAttachmentType = NetworkAttachmentType_HostOnly;

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* Adapt the CFGM logic and notify the guest => changeAdpater=TRUE. */
        HRESULT rc = mParent->onNetworkAdapterChange(this, TRUE);
        if (FAILED(rc))
        {
            /* If changing the attachment failed then we can't assume
             * that the previous attachment will attach correctly
             * and thus return error along with detaching all
             * attachments.
             */
            Detach();
            return rc;
        }
    }

    return S_OK;
}

STDMETHODIMP NetworkAdapter::AttachToVDE()
{
#if defined(VBOX_WITH_VDE)
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep (mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* don't do anything if we're already host interface attached */
    if (mData->mAttachmentType != NetworkAttachmentType_VDE)
    {
        mData.backup();

        /* first detach the current attachment */
        // Commented this for now as it reset the parameter mData->mHostInterface
        // which is essential while changing the Attachment dynamically.
        //detach();

        mData->mAttachmentType = NetworkAttachmentType_VDE;

        /* leave the lock before informing callbacks */
        alock.release();

        HRESULT rc = mParent->onNetworkAdapterChange (this, TRUE);
        if (FAILED (rc))
        {
            /* If changing the attachment failed then we can't assume
             * that the previous attachment will attach correctly
             * and thus return error along with detaching all
             * attachments.
             */
            Detach();
            return rc;
        }
    }

    return S_OK;
#else /* !VBOX_WITH_VDE */
    return E_NOTIMPL;
#endif
}

STDMETHODIMP NetworkAdapter::Detach()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (mData->mAttachmentType != NetworkAttachmentType_Null)
    {
        mData.backup();

        detach();

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);       // mParent is const, no need to lock
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* adapt the CFGM logic and notify the guest => changeAdapter=TRUE. */
        mParent->onNetworkAdapterChange(this, TRUE);
    }

    return S_OK;
}

// public methods only for internal purposes
////////////////////////////////////////////////////////////////////////////////

/**
 *  Loads settings from the given adapter node.
 *  May be called once right after this object creation.
 *
 *  @param aAdapterNode <Adapter> node.
 *
 *  @note Locks this object for writing.
 */
HRESULT NetworkAdapter::loadSettings(BandwidthControl *bwctl,
                                     const settings::NetworkAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Note: we assume that the default values for attributes of optional
     * nodes are assigned in the Data::Data() constructor and don't do it
     * here. It implies that this method may only be called after constructing
     * a new BIOSSettings object while all its data fields are in the default
     * values. Exceptions are fields whose creation time defaults don't match
     * values that should be applied when these fields are not explicitly set
     * in the settings file (for backwards compatibility reasons). This takes
     * place when a setting of a newly created object must default to A while
     * the same setting of an object loaded from the old settings file must
     * default to B. */

    HRESULT rc = S_OK;

    mData->mAdapterType = data.type;
    mData->mEnabled = data.fEnabled;
    /* MAC address (can be null) */
    rc = updateMacAddress(data.strMACAddress);
    if (FAILED(rc)) return rc;
    /* cable (required) */
    mData->mCableConnected = data.fCableConnected;
    /* line speed (defaults to 100 Mbps) */
    mData->mLineSpeed = data.ulLineSpeed;
    mData->mPromiscModePolicy = data.enmPromiscModePolicy;
    /* tracing (defaults to false) */
    mData->mTraceEnabled = data.fTraceEnabled;
    mData->mTraceFile = data.strTraceFile;
    /* boot priority (defaults to 0, i.e. lowest) */
    mData->mBootPriority = data.ulBootPriority;
    /* bandwidth group */
    if (data.strBandwidthGroup.isEmpty())
        updateBandwidthGroup(NULL);
    else
    {
        ComObjPtr<BandwidthGroup> group;
        rc = bwctl->getBandwidthGroupByName(data.strBandwidthGroup, group, true); // TODO: set error?
        if (FAILED(rc)) return rc;
    }
    
    // leave the lock before attaching
    alock.release();

    switch (data.mode)
    {
        case NetworkAttachmentType_NAT:
            mNATEngine->loadSettings(data.nat);
            rc = AttachToNAT();
            if (FAILED(rc)) return rc;
        break;

        case NetworkAttachmentType_Bridged:
            rc = COMSETTER(HostInterface)(Bstr(data.strName).raw());
            if (FAILED(rc)) return rc;
            rc = AttachToBridgedInterface();
            if (FAILED(rc)) return rc;
        break;

        case NetworkAttachmentType_Internal:
            mData->mInternalNetwork = data.strName;
            Assert(!mData->mInternalNetwork.isEmpty());

            rc = AttachToInternalNetwork();
            if (FAILED(rc)) return rc;
        break;

        case NetworkAttachmentType_HostOnly:
#if defined(VBOX_WITH_NETFLT)
            rc = COMSETTER(HostInterface)(Bstr(data.strName).raw());
            if (FAILED(rc)) return rc;
#endif
            rc = AttachToHostOnlyInterface();
            if (FAILED(rc)) return rc;
        break;

#if defined(VBOX_WITH_VDE)
        case NetworkAttachmentType_VDE:
            mData->mVDENetwork = data.strName;
            rc = AttachToVDE();
            if (FAILED(rc)) return rc;
        break;
#endif

        case NetworkAttachmentType_Null:
            rc = Detach();
            if (FAILED(rc)) return rc;
        break;
    }
    if (data.fHasDisabledNAT)
        mNATEngine->loadSettings(data.nat);

    // after loading settings, we are no longer different from the XML on disk
    m_fModified = false;

    return S_OK;
}

/**
 *  Saves settings to the given adapter node.
 *
 *  Note that the given Adapter node is completely empty on input.
 *
 *  @param aAdapterNode <Adapter> node.
 *
 *  @note Locks this object for reading.
 */
HRESULT NetworkAdapter::saveSettings(settings::NetworkAdapter &data)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnRC(autoCaller.rc());

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.fEnabled = !!mData->mEnabled;
    data.strMACAddress = mData->mMACAddress;
    data.fCableConnected = !!mData->mCableConnected;

    data.enmPromiscModePolicy = mData->mPromiscModePolicy;
    data.ulLineSpeed = mData->mLineSpeed;

    data.fTraceEnabled = !!mData->mTraceEnabled;

    data.strTraceFile = mData->mTraceFile;

    data.ulBootPriority = mData->mBootPriority;

    if (mData->mBandwidthGroup.isNull())
        data.strBandwidthGroup = "";
    else
        data.strBandwidthGroup = mData->mBandwidthGroup->getName();

    data.type = mData->mAdapterType;

    switch (data.mode = mData->mAttachmentType)
    {
        case NetworkAttachmentType_Null:
            data.strName.setNull();
        break;

        case NetworkAttachmentType_NAT:
            data.fHasDisabledNAT = 0;
            mNATEngine->commit();
            mNATEngine->saveSettings(data.nat);
        break;

        case NetworkAttachmentType_Bridged:
            data.strName = mData->mHostInterface;
        break;

        case NetworkAttachmentType_Internal:
            data.strName = mData->mInternalNetwork;
        break;

        case NetworkAttachmentType_HostOnly:
            data.strName = mData->mHostInterface;
        break;

#if defined(VBOX_WITH_VDE)
        case NetworkAttachmentType_VDE:
            data.strName = mData->mVDENetwork;
        break;
#endif
    }

    if (data.mode != NetworkAttachmentType_NAT)
    {
        data.fHasDisabledNAT = 1; /* ??? */
        mNATEngine->commit();
        mNATEngine->saveSettings(data.nat);
    }

    // after saving settings, we are no longer different from the XML on disk
    m_fModified = false;

    return S_OK;
}

/**
 * Returns true if any setter method has modified settings of this instance.
 * @return
 */
bool NetworkAdapter::isModified() {
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    bool fChanged = m_fModified;
    fChanged |= (mData->mAdapterType == NetworkAttachmentType_NAT? mNATEngine->isModified() : false);
    return fChanged;
}

/**
 *  @note Locks this object for writing.
 */
void NetworkAdapter::rollback()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void NetworkAdapter::commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller (mPeer);
    AssertComRCReturnVoid (peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(mPeer, this COMMA_LOCKVAL_SRC_POS);

    if (mData.isBackedUp())
    {
        mData.commit();
        if (mPeer)
        {
            /* attach new data to the peer and reshare it */
            mPeer->mData.attach (mData);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void NetworkAdapter::copyFrom (NetworkAdapter *aThat)
{
    AssertReturnVoid (aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller (aThat);
    AssertComRCReturnVoid (thatCaller.rc());

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    mData.assignCopy (aThat->mData);
}

void NetworkAdapter::applyDefaults (GuestOSType *aOsType)
{
    AssertReturnVoid (aOsType != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid (autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    bool e1000enabled = false;
#ifdef VBOX_WITH_E1000
    e1000enabled = true;
#endif // VBOX_WITH_E1000

    NetworkAdapterType_T defaultType = aOsType->networkAdapterType();

    /* Set default network adapter for this OS type */
    if (defaultType == NetworkAdapterType_I82540EM ||
        defaultType == NetworkAdapterType_I82543GC ||
        defaultType == NetworkAdapterType_I82545EM)
    {
        if (e1000enabled) mData->mAdapterType = defaultType;
    }
    else mData->mAdapterType = defaultType;

    /* Enable and connect the first one adapter to the NAT */
    if (mData->mSlot == 0)
    {
        mData->mEnabled = true;
        mData->mAttachmentType = NetworkAttachmentType_NAT;
        mData->mCableConnected = true;
    }
}

// private methods
////////////////////////////////////////////////////////////////////////////////

/**
 *  Worker routine for detach handling. No locking, no notifications.

 *  @note Must be called from under the object's write lock.
 */
void NetworkAdapter::detach()
{
    AssertReturnVoid (isWriteLockOnCurrentThread());

    switch (mData->mAttachmentType)
    {
        case NetworkAttachmentType_Null:
        {
            /* nothing to do here */
            break;
        }
        case NetworkAttachmentType_NAT:
        {
            break;
        }
        case NetworkAttachmentType_Bridged:
        {
            /* reset handle and device name */
            mData->mHostInterface = "";
            break;
        }
        case NetworkAttachmentType_Internal:
        {
            mData->mInternalNetwork.setNull();
            break;
        }
        case NetworkAttachmentType_HostOnly:
        {
#if defined(VBOX_WITH_NETFLT)
            /* reset handle and device name */
            mData->mHostInterface = "";
#endif
            break;
        }
    }

    mData->mAttachmentType = NetworkAttachmentType_Null;
}

/**
 *  Generates a new unique MAC address based on our vendor ID and
 *  parts of a GUID.
 *
 *  @note Must be called from under the object's write lock or within the init
 *  span.
 */
void NetworkAdapter::generateMACAddress()
{
    /*
     * Our strategy is as follows: the first three bytes are our fixed
     * vendor ID (080027). The remaining 3 bytes will be taken from the
     * start of a GUID. This is a fairly safe algorithm.
     */
    char strMAC[13];
    Guid guid;
    guid.create();
    RTStrPrintf (strMAC, sizeof(strMAC), "080027%02X%02X%02X",
                 guid.raw()->au8[0], guid.raw()->au8[1], guid.raw()->au8[2]);
    LogFlowThisFunc(("generated MAC: '%s'\n", strMAC));
    mData->mMACAddress = strMAC;
}

STDMETHODIMP NetworkAdapter::COMGETTER(BandwidthGroup) (IBandwidthGroup **aBwGroup)
{
    LogFlowThisFuncEnter();
    CheckComArgOutPointerValid(aBwGroup);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->mBandwidthGroup.queryInterfaceTo(aBwGroup);

    LogFlowThisFuncLeave();
    return S_OK;
}

STDMETHODIMP NetworkAdapter::COMSETTER(BandwidthGroup) (IBandwidthGroup *aBwGroup)
{
    LogFlowThisFuncEnter();
    CheckComArgOutPointerValid(aBwGroup);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(mParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    //HRESULT hrc = S_OK;

    if (mData->mBandwidthGroup != aBwGroup)
    {
        mData.backup();

        updateBandwidthGroup(static_cast<BandwidthGroup*>(aBwGroup));

        m_fModified = true;
        // leave the lock before informing callbacks
        alock.release();

        AutoWriteLock mlock(mParent COMMA_LOCKVAL_SRC_POS);
        mParent->setModified(Machine::IsModified_NetworkAdapters);
        mlock.release();

        /* TODO: changeAdapter=???. */
        mParent->onNetworkAdapterChange(this, FALSE);
    }

    LogFlowThisFuncLeave();
    return S_OK;
}

void NetworkAdapter::updateBandwidthGroup(BandwidthGroup *aBwGroup)
{
    LogFlowThisFuncEnter();
    Assert(isWriteLockOnCurrentThread());

    mData.backup();
    if (!mData->mBandwidthGroup.isNull())
    {
        mData->mBandwidthGroup->release();
        mData->mBandwidthGroup.setNull();
    }

    if (aBwGroup)
    {
        mData->mBandwidthGroup = aBwGroup;
        mData->mBandwidthGroup->reference();
    }

    LogFlowThisFuncLeave();
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
