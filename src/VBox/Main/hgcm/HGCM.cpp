/** @file
 *
 * HGCM (Host-Guest Communication Manager)
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*
 * NOT FOR REVIEWING YET. A LOT OF TODO/MISSED/INCOMPLETE/SKETCH CODE INSIDE!
 */

#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_HGCM
#include "Logging.h"

#include <string.h>

#include "hgcm/HGCM.h"
#include "hgcm/HGCMThread.h"

#include <VBox/err.h>
#include <VBox/hgcmsvc.h>

#include <iprt/alloc.h>
#include <iprt/avl.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>
#include <iprt/ldr.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>

#include <VBox/VBoxGuest.h>

/**
 *
 * Service location types:
 *
 *  LOCAL SERVICE
 *      service DLL is loaded by the VM process,
 *      and directly called by the VM HGCM instance.
 */

/**
 * A service gets one thread, which synchronously delivers messages to
 * the service. This is good for serialization.
 *
 * Some services may want to process messages asynchronously, and will want
 * a next message to be delivered, while a previous message is still being
 * processed.
 *
 * The dedicated service thread delivers a next message when service
 * returns after fetching a previous one. The service will call a message
 * completion callback when message is actually processed. So returning
 * from the service call means only that the service is processing message.
 *
 * 'Message processed' condition is indicated by service, which call the
 * callback, even if the callback is called synchronously in the dedicated
 * thread.
 *
 * This message completion callback is only valid for Call requests.
 * Connect and Disconnect are processed sznchronously by service.
 *
 */

/** @todo services registration, only registered service dll can be loaded */

/** @todo a registered LOCAL service must also get a thread, for now
 * a thread per service (later may be there will be an option to
 * have a thread per client for a service, however I do not think it's
 * really necessary).
 * The requests will be queued and executed by the service thread,
 * an IRQ notification will be ussued when a request is completed.
 *
 * May be other services (like VRDP acceleration) should still use
 * the EMT thread, because they have their own threads for long
 * operations.
 * So we have to distinguish those services during
 * registration process (external/internal registration).
 * External dlls will always have its own thread,
 * internal (trusted) services will choose between having executed
 * on EMT or on a separate thread.
 *
 */


/** Internal helper service object. HGCM code would use it to
 *  hold information about services and communicate with services.
 *  The HGCMService is an (in future) abstract class that implements
 *  common functionality. There will be derived classes for specific
 *  service types (Local, etc).
 */

/** @todo should be HGCMObject */
class HGCMService
{
    private:
        VBOXHGCMSVCHELPERS m_svcHelpers;

        static HGCMService *sm_pSvcListHead;
        static HGCMService *sm_pSvcListTail;


        HGCMTHREADHANDLE m_thread;
        friend DECLCALLBACK(void) hgcmServiceThread (HGCMTHREADHANDLE ThreadHandle, void *pvUser);

        uint32_t volatile m_u32RefCnt;

        HGCMService *m_pSvcNext;
        HGCMService *m_pSvcPrev;

        char *m_pszSvcName;
        char *m_pszSvcLibrary;
        
        PPDMIHGCMPORT m_pHGCMPort;

        RTLDRMOD m_hLdrMod;
        PFNVBOXHGCMSVCLOAD m_pfnLoad;

        VBOXHGCMSVCFNTABLE m_fntable;


        int loadServiceDLL (void);
        void unloadServiceDLL (void);

        int InstanceCreate (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort);
        void InstanceDestroy (void);

        HGCMService ();
        ~HGCMService () {};

        bool EqualToLoc (HGCMServiceLocation *loc);

        static DECLCALLBACK(void) svcHlpCallComplete (VBOXHGCMCALLHANDLE callHandle, int32_t rc);
        
    public:

        static int FindService (HGCMService **ppsvc, HGCMServiceLocation *loc);
        static HGCMService *FindServiceByName (const char *pszServiceName);
        static int LoadService (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort);
        void ReleaseService (void);

        uint32_t SizeOfClient (void) { return m_fntable.cbClient; };

        int Connect (uint32_t u32ClientID);
        int Disconnect (uint32_t u32ClientID);
        int GuestCall (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[], bool fBlock);
        int HostCall (PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[]);
};


class HGCMClient: public HGCMObject
{
    public:
        ~HGCMClient ();

        int Init (HGCMService *pSvc);

        /** Service that the client is connected to. */
        HGCMService *pService;

        /** Client specific data. */
        void *pvData;
};

HGCMClient::~HGCMClient ()
{
    RTMemFree (pvData);
}

int HGCMClient::Init (HGCMService *pSvc)
{
    pService = pSvc;

    if (pService->SizeOfClient () > 0)
    {
        pvData = RTMemAllocZ (pService->SizeOfClient ());

        if (!pvData)
        {
           return VERR_NO_MEMORY;
        }
    }

    return VINF_SUCCESS;
}


#define HGCM_CLIENT_DATA(pService, pClient) (pClient->pvData)


/*
 * Messages processed by worker threads.
 */

#define HGCMMSGID_SVC_LOAD       (0)
#define HGCMMSGID_SVC_UNLOAD     (1)
#define HGCMMSGID_SVC_CONNECT    (2)
#define HGCMMSGID_SVC_DISCONNECT (3)
#define HGCMMSGID_GUESTCALL      (4)

class HGCMMsgSvcLoad: public HGCMMsgCore
{
};

class HGCMMsgSvcUnload: public HGCMMsgCore
{
};

class HGCMMsgSvcConnect: public HGCMMsgCore
{
    public:
        /* client identifier */
        uint32_t u32ClientID;
};

class HGCMMsgSvcDisconnect: public HGCMMsgCore
{
    public:
        /* client identifier */
        uint32_t u32ClientID;
};

class HGCMMsgHeader: public HGCMMsgCore
{
    public:
        HGCMMsgHeader () : pCmd (NULL), pHGCMPort (NULL) {};
        
        /* Command pointer/identifier. */
        PVBOXHGCMCMD pCmd;

        /* Port to be informed on message completion. */
        PPDMIHGCMPORT pHGCMPort;
};


class HGCMMsgCall: public HGCMMsgHeader
{
    public:
        /* client identifier */
        uint32_t u32ClientID;

        /* function number */
        uint32_t u32Function;

        /* number of parameters */
        uint32_t cParms;

        VBOXHGCMSVCPARM *paParms;
};


/*
 * Messages processed by main HGCM thread.
 */

#define HGCMMSGID_CONNECT    (10)
#define HGCMMSGID_DISCONNECT (11)
#define HGCMMSGID_LOAD       (12)

#define HGCMMSGID_HOSTCALL   (13)

class HGCMMsgConnect: public HGCMMsgHeader
{
    public:
        /* service location */
        HGCMSERVICELOCATION *pLoc;

        /* client identifier */
        uint32_t *pu32ClientID;

};

class HGCMMsgDisconnect: public HGCMMsgHeader
{
    public:
        /* client identifier */
        uint32_t u32ClientID;
};

class HGCMMsgLoad: public HGCMMsgHeader
{
    public:
        virtual ~HGCMMsgLoad ()
        {
            RTStrFree (pszServiceLibrary);
            RTStrFree (pszServiceName);
        }
        
        int Init (const char *pszName, const char *pszLibrary)
        {
            pszServiceName = RTStrDup (pszName);
            pszServiceLibrary = RTStrDup (pszLibrary);
            
            if (!pszServiceName || !pszServiceLibrary)
            {
                RTStrFree (pszServiceLibrary);
                RTStrFree (pszServiceName);
                pszServiceLibrary = NULL;
                pszServiceName = NULL;
                return VERR_NO_MEMORY;
            }
            
            return VINF_SUCCESS;
        }
        
        char *pszServiceName;
        char *pszServiceLibrary;
};

class HGCMMsgHostCall: public HGCMMsgHeader
{
    public:
        char *pszServiceName;
        
        /* function number */
        uint32_t u32Function;

        /* number of parameters */
        uint32_t cParms;

        VBOXHGCMSVCPARM *paParms;
};

/* static */ DECLCALLBACK(void) HGCMService::svcHlpCallComplete (VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
   HGCMMsgCore *pMsgCore = (HGCMMsgCore *)callHandle;

   if (   pMsgCore->MsgId () == HGCMMSGID_GUESTCALL
       || pMsgCore->MsgId () == HGCMMSGID_HOSTCALL)
   {
       /* Only call the completion for these messages. The helper 
        * is called by the service, and the service does not get
        * any other messages.
        */
       hgcmMsgComplete (pMsgCore, rc);
   }
   else
   {
       AssertFailed ();
   }
}

HGCMService *HGCMService::sm_pSvcListHead = NULL;
HGCMService *HGCMService::sm_pSvcListTail = NULL;

HGCMService::HGCMService ()
    :
    m_thread     (0),
    m_u32RefCnt  (0),
    m_pSvcNext   (NULL),
    m_pSvcPrev   (NULL),
    m_pszSvcName (NULL),
    m_pszSvcLibrary (NULL),
    m_hLdrMod    (NIL_RTLDRMOD),
    m_pfnLoad    (NULL)
{
    memset (&m_fntable, 0, sizeof (m_fntable));
}


HGCMMsgCore *hgcmMessageAlloc (uint32_t u32MsgId)
{
    switch (u32MsgId)
    {
        case HGCMMSGID_SVC_LOAD:        return new HGCMMsgSvcLoad ();
        case HGCMMSGID_SVC_UNLOAD:      return new HGCMMsgSvcUnload ();
        case HGCMMSGID_SVC_CONNECT:     return new HGCMMsgSvcConnect ();
        case HGCMMSGID_SVC_DISCONNECT:  return new HGCMMsgSvcDisconnect ();
        case HGCMMSGID_GUESTCALL:       return new HGCMMsgCall ();

        case HGCMMSGID_CONNECT:         return new HGCMMsgConnect ();
        case HGCMMSGID_DISCONNECT:      return new HGCMMsgDisconnect ();
        case HGCMMSGID_LOAD:            return new HGCMMsgLoad ();
        case HGCMMSGID_HOSTCALL:        return new HGCMMsgHostCall ();
        default:
            Log(("hgcmMessageAlloc::Unsupported message number %08X\n", u32MsgId));
    }

    return NULL;
}


static DECLCALLBACK(void) hgcmMsgCompletionCallback (int32_t result, HGCMMsgCore *pMsgCore)
{
    /* Call the VMMDev port interface to issue IRQ notification. */
    HGCMMsgHeader *pMsgHdr = (HGCMMsgHeader *)pMsgCore;

    LogFlow(("MAIN::hgcmMsgCompletionCallback: message %p\n", pMsgCore));

    if (pMsgHdr->pHGCMPort)
    {
        pMsgHdr->pHGCMPort->pfnCompleted (pMsgHdr->pHGCMPort, result, pMsgHdr->pCmd);
    }

    return;
}


DECLCALLBACK(void) hgcmServiceThread (HGCMTHREADHANDLE ThreadHandle, void *pvUser)
{
    int rc = VINF_SUCCESS;

    HGCMService *pSvc = (HGCMService *)pvUser;

    AssertRelease(pSvc != NULL);

    HGCMMsgCore *pMsgCore = NULL;

    bool bUnloaded = false;

    while (!bUnloaded)
    {
        rc = hgcmMsgGet (ThreadHandle, &pMsgCore);

        if (VBOX_FAILURE (rc))
        {
            Log(("hgcmServiceThread: message get failed, rc = %Vrc\n", rc));

            RTThreadSleep(100);

            continue;
        }

        switch (pMsgCore->MsgId ())
        {
            case HGCMMSGID_SVC_LOAD:
            {
                LogFlow(("HGCMMSGID_SVC_LOAD\n"));
                rc = pSvc->loadServiceDLL ();
            } break;

            case HGCMMSGID_SVC_UNLOAD:
            {
                LogFlow(("HGCMMSGID_SVC_UNLOAD\n"));
                pSvc->unloadServiceDLL ();
                bUnloaded = true;
            } break;

            case HGCMMSGID_SVC_CONNECT:
            {
                LogFlow(("HGCMMSGID_SVC_CONNECT\n"));

                HGCMMsgSvcConnect *pMsg = (HGCMMsgSvcConnect *)pMsgCore;

                rc = VINF_SUCCESS;

                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID);

                if (pClient)
                {
                    rc = pSvc->m_fntable.pfnConnect (pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient));

                    hgcmObjDereference (pClient);
                }
            } break;

            case HGCMMSGID_SVC_DISCONNECT:
            {
                LogFlow(("HGCMMSGID_SVC_DISCONNECT\n"));

                HGCMMsgSvcDisconnect *pMsg = (HGCMMsgSvcDisconnect *)pMsgCore;

                rc = VINF_SUCCESS;

                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID);

                if (pClient)
                {
                    rc = pSvc->m_fntable.pfnDisconnect (pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient));

                    hgcmObjDereference (pClient);
                }
            } break;

            case HGCMMSGID_GUESTCALL:
            {
                LogFlow(("HGCMMSGID_GUESTCALL\n"));

                HGCMMsgCall *pMsg = (HGCMMsgCall *)pMsgCore;

                rc = VINF_SUCCESS;

                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID);

                if (pClient)
                {
                    pSvc->m_fntable.pfnCall ((VBOXHGCMCALLHANDLE)pMsg, pMsg->u32ClientID, HGCM_CLIENT_DATA(pSvc, pClient), pMsg->u32Function, pMsg->cParms, pMsg->paParms);

                    hgcmObjDereference (pClient);
                }
                else
                {
                    rc = VERR_INVALID_HANDLE;
                }
            } break;

            case HGCMMSGID_HOSTCALL:
            {
                LogFlow(("HGCMMSGID_HOSTCALL\n"));

                HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)pMsgCore;

                pSvc->m_fntable.pfnHostCall ((VBOXHGCMCALLHANDLE)pMsg, 0, NULL, pMsg->u32Function, pMsg->cParms, pMsg->paParms);

                rc = VINF_SUCCESS;
            } break;

            default:
            {
                Log(("hgcmServiceThread::Unsupported message number %08X\n", pMsgCore->MsgId ()));
                rc = VERR_NOT_SUPPORTED;
            } break;
        }

        if (   pMsgCore->MsgId () != HGCMMSGID_GUESTCALL
            && pMsgCore->MsgId () != HGCMMSGID_HOSTCALL)
        {
            /* For HGCMMSGID_GUESTCALL & HGCMMSGID_HOSTCALL the service
             * calls the completion helper. Other messages have to be
             * completed here.
             */
            hgcmMsgComplete (pMsgCore, rc);
        }
    }

    return;
}

int HGCMService::InstanceCreate (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort)
{
    int rc = VINF_SUCCESS;

    LogFlow(("HGCMService::InstanceCreate: name %s, lib %s\n", pszServiceName, pszServiceLibrary));

    char achThreadName[14];

    RTStrPrintf (achThreadName, sizeof (achThreadName), "HGCM%08X", this);

    /* @todo do a proper fix 0x12345678 -> sizeof */
    rc = hgcmThreadCreate (&m_thread, achThreadName, hgcmServiceThread, this, 0x12345678 /* sizeof (HGCMMsgCall) */);

    if (VBOX_SUCCESS(rc))
    {
        m_pszSvcName    = RTStrDup (pszServiceName);
        m_pszSvcLibrary = RTStrDup (pszServiceLibrary);

        if (!m_pszSvcName || !m_pszSvcLibrary)
        {
            RTStrFree (m_pszSvcLibrary);
            m_pszSvcLibrary = NULL;
            
            RTStrFree (m_pszSvcName);
            m_pszSvcName = NULL;
            
            rc = VERR_NO_MEMORY;
        }
        else
        {
            m_pHGCMPort = pHGCMPort;
            
            m_svcHelpers.pfnCallComplete = svcHlpCallComplete;
            m_svcHelpers.pvInstance      = this;
            
            /* Execute the load request on the service thread. */
            HGCMMSGHANDLE hMsg;

            rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_LOAD, sizeof (HGCMMsgSvcLoad), hgcmMessageAlloc);

            if (VBOX_SUCCESS(rc))
            {
                rc = hgcmMsgSend (hMsg);
            }
        }
    }
    else
    {
        Log(("HGCMService::InstanceCreate: FAILURE: service thread creation\n"));
    }

    if (VBOX_FAILURE(rc))
    {
        InstanceDestroy ();
    }

    LogFlow(("HGCMService::InstanceCreate rc = %Vrc\n", rc));

    return rc;
}

void HGCMService::InstanceDestroy (void)
{
    HGCMMSGHANDLE hMsg;

    LogFlow(("HGCMService::InstanceDestroy\n"));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_UNLOAD, sizeof (HGCMMsgSvcUnload), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        rc = hgcmMsgSend (hMsg);
    }

    RTStrFree (m_pszSvcLibrary);
    m_pszSvcLibrary = NULL;
    RTStrFree (m_pszSvcName);
    m_pszSvcName = NULL;

    LogFlow(("HGCMService::InstanceDestroy completed\n"));
}

bool HGCMService::EqualToLoc (HGCMServiceLocation *loc)
{
    if (!loc || (loc->type != VMMDevHGCMLoc_LocalHost && loc->type != VMMDevHGCMLoc_LocalHost_Existing))
    {
        return false;
    }

    if (strcmp (m_pszSvcName, loc->u.host.achName) != 0)
    {
        return false;
    }

    return true;
}

/** Services are searched by FindService function which is called
 *  by the main HGCM thread during CONNECT message processing.
 *  Reference count of the service is increased.
 *  Services are loaded by the LoadService function.
 *  Note: both methods are executed by the main HGCM thread.
 */
/* static */ int HGCMService::FindService (HGCMService **ppsvc, HGCMServiceLocation *loc)
{
    HGCMService *psvc = NULL;

    LogFlow(("HGCMService::FindService: loc = %p\n", loc));

    if (!loc || (loc->type != VMMDevHGCMLoc_LocalHost && loc->type != VMMDevHGCMLoc_LocalHost_Existing))
    {
        return VERR_INVALID_PARAMETER;
    }

    LogFlow(("HGCMService::FindService: name %s\n", loc->u.host.achName));

    /* Look at already loaded services. */
    psvc = sm_pSvcListHead;

    while (psvc)
    {
        if (psvc->EqualToLoc (loc))
        {
            break;
        }

        psvc = psvc->m_pSvcNext;
    }

    LogFlow(("HGCMService::FindService: lookup in the list is %p\n", psvc));
    
    if (psvc)
    {
        ASMAtomicIncU32 (&psvc->m_u32RefCnt);
        
        *ppsvc = psvc;
        
        return VINF_SUCCESS;
    }
    
    return VERR_ACCESS_DENIED;
}

/* static */ HGCMService *HGCMService::FindServiceByName (const char *pszServiceName)
{
    HGCMService *psvc = sm_pSvcListHead;

    while (psvc)
    {
        if (strcmp (psvc->m_pszSvcName, pszServiceName) == 0)
        {
            break;
        }

        psvc = psvc->m_pSvcNext;
    }
    
    return psvc;
}

/* static */ int HGCMService::LoadService (const char *pszServiceLibrary, const char *pszServiceName, PPDMIHGCMPORT pHGCMPort)
{
    int rc = VINF_SUCCESS;

    HGCMService *psvc = NULL;

    LogFlow(("HGCMService::LoadService: name = %s, lib %s\n", pszServiceName, pszServiceLibrary));
    
    /* Look at already loaded services to avoid double loading. */
    psvc = FindServiceByName (pszServiceName);

    if (psvc)
    {
        LogFlow(("HGCMService::LoadService: Service already exists %p!!!\n", psvc));
    }
    else
    {
        psvc = new HGCMService ();

        if (!psvc)
        {
            Log(("HGCMService::Load: memory allocation for the service failed!!!\n"));
            rc = VERR_NO_MEMORY;
        }

        if (VBOX_SUCCESS(rc))
        {
            rc = psvc->InstanceCreate (pszServiceLibrary, pszServiceName, pHGCMPort);

            if (VBOX_SUCCESS(rc))
            {
                /* Insert the just created service to list for future references. */
                psvc->m_pSvcNext = sm_pSvcListHead;
                psvc->m_pSvcPrev = NULL;
                
                if (sm_pSvcListHead)
                {
                    sm_pSvcListHead->m_pSvcPrev = psvc;
                }
                else
                {
                    sm_pSvcListTail = psvc;
                }

                sm_pSvcListHead = psvc;
                
                LogFlow(("HGCMService::LoadService: service %p\n", psvc));
            }
        }
    }

    return rc;
}

void HGCMService::ReleaseService (void)
{
    uint32_t u32RefCnt = ASMAtomicDecU32 (&m_u32RefCnt);

    AssertRelease(u32RefCnt != ~0U);

    if (u32RefCnt == 0)
    {
        /** @todo We do not unload services. Cache them all. Unloading will be later. HGCMObject! */

        LogFlow(("HGCMService::ReleaseService: no more references.\n"));

//        InstanceDestroy ();

//        delete this;
    }
}

/** Helper function to load a local service DLL.
 *
 *  @return VBox code
 */
int HGCMService::loadServiceDLL (void)
{
    LogFlow(("HGCMService::loadServiceDLL: m_pszSvcLibrary = %s\n", m_pszSvcLibrary));

    if (m_pszSvcLibrary == NULL)
    {
        return VERR_INVALID_PARAMETER;
    }

    int rc = VINF_SUCCESS;

    rc = RTLdrLoad (m_pszSvcLibrary, &m_hLdrMod);

    if (VBOX_SUCCESS(rc))
    {
        LogFlow(("HGCMService::loadServiceDLL: successfully loaded the library.\n"));

        m_pfnLoad = NULL;

        rc = RTLdrGetSymbol (m_hLdrMod, VBOX_HGCM_SVCLOAD_NAME, (void**)&m_pfnLoad);

        if (VBOX_FAILURE (rc) || !m_pfnLoad)
        {
            Log(("HGCMService::loadServiceDLL: Error resolving the service entry point %s, rc = %d, m_pfnLoad = %p\n", VBOX_HGCM_SVCLOAD_NAME, rc, m_pfnLoad));

            if (VBOX_SUCCESS(rc))
            {
                /* m_pfnLoad was NULL */
                rc = VERR_SYMBOL_NOT_FOUND;
            }
        }

        if (VBOX_SUCCESS(rc))
        {
            memset (&m_fntable, 0, sizeof (m_fntable));

            m_fntable.cbSize     = sizeof (m_fntable);
            m_fntable.u32Version = VBOX_HGCM_SVC_VERSION;
            m_fntable.pHelpers   = &m_svcHelpers;

            rc = m_pfnLoad (&m_fntable);

            LogFlow(("HGCMService::loadServiceDLL: m_pfnLoad rc = %Vrc\n", rc));

            if (VBOX_SUCCESS (rc))
            {
                if (   m_fntable.pfnUnload == NULL
                    || m_fntable.pfnConnect == NULL
                    || m_fntable.pfnDisconnect == NULL
                    || m_fntable.pfnCall == NULL
                   )
                {
                    Log(("HGCMService::loadServiceDLL: at least one of function pointers is NULL\n"));

                    rc = VERR_INVALID_PARAMETER;

                    if (m_fntable.pfnUnload)
                    {
                        m_fntable.pfnUnload ();
                    }
                }
            }
        }
    }
    else
    {
        LogFlow(("HGCMService::loadServiceDLL: failed to load service library. The service is not available.\n"));
        m_hLdrMod = NIL_RTLDRMOD;
    }

    if (VBOX_FAILURE(rc))
    {
        unloadServiceDLL ();
    }

    return rc;
}

/** Helper function to free a local service DLL.
 *
 *  @return VBox code
 */
void HGCMService::unloadServiceDLL (void)
{
    if (m_hLdrMod)
    {
        RTLdrClose (m_hLdrMod);
    }

    memset (&m_fntable, 0, sizeof (m_fntable));
    m_pfnLoad = NULL;
    m_hLdrMod = NIL_RTLDRMOD;
}


int HGCMService::Connect (uint32_t u32ClientID)
{
    HGCMMSGHANDLE hMsg;

    LogFlow(("MAIN::HGCMService::Connect: client id = %d\n", u32ClientID));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_CONNECT, sizeof (HGCMMsgSvcConnect), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgSvcConnect *pMsg = (HGCMMsgSvcConnect *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->u32ClientID = u32ClientID;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
    }
    else
    {
        Log(("MAIN::HGCMService::Connect: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

int HGCMService::Disconnect (uint32_t u32ClientID)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::HGCMService::Disconnect: client id = %d\n", u32ClientID));

    HGCMMSGHANDLE hMsg;

    rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_SVC_DISCONNECT, sizeof (HGCMMsgSvcDisconnect), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgSvcDisconnect *pMsg = (HGCMMsgSvcDisconnect *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->u32ClientID = u32ClientID;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
    }
    else
    {
        Log(("MAIN::HGCMService::Disconnect: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

/* Forward the call request to the dedicated service thread.
 */
int HGCMService::GuestCall (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fBlock)
{
    HGCMMSGHANDLE hMsg = 0;

    LogFlow(("MAIN::HGCMService::Call\n"));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_GUESTCALL, sizeof (HGCMMsgCall), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgCall *pMsg = (HGCMMsgCall *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->pCmd      = pCmd;
        pMsg->pHGCMPort = pHGCMPort;

        pMsg->u32ClientID      = u32ClientID;
        pMsg->u32Function      = u32Function;
        pMsg->cParms           = cParms;
        pMsg->paParms = paParms;

        hgcmObjDereference (pMsg);

        if (fBlock)
            rc = hgcmMsgSend (hMsg);
        else
            rc = hgcmMsgPost (hMsg, hgcmMsgCompletionCallback);

        if (!fBlock && VBOX_SUCCESS(rc))
        {
            rc = VINF_HGCM_ASYNC_EXECUTE;
        }
    }
    else
    {
        Log(("MAIN::HGCMService::Call: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

/* Forward the call request to the dedicated service thread.
 */
int HGCMService::HostCall (PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    HGCMMSGHANDLE hMsg = 0;

    LogFlow(("MAIN::HGCMService::HostCall %s\n", m_pszSvcName));

    int rc = hgcmMsgAlloc (m_thread, &hMsg, HGCMMSGID_HOSTCALL, sizeof (HGCMMsgHostCall), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->pCmd      = NULL; /* Not used for host calls. */
        pMsg->pHGCMPort = NULL; /* Not used for host calls. */

        pMsg->u32Function      = u32Function;
        pMsg->cParms           = cParms;
        pMsg->paParms          = paParms;

        hgcmObjDereference (pMsg);

        rc = hgcmMsgSend (hMsg);
    }
    else
    {
        Log(("MAIN::HGCMService::Call: Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}

/* Main HGCM thread that processes CONNECT/DISCONNECT requests. */
static DECLCALLBACK(void) hgcmThread (HGCMTHREADHANDLE ThreadHandle, void *pvUser)
{
    NOREF(pvUser);

    int rc = VINF_SUCCESS;

    HGCMMsgCore *pMsgCore = NULL;

    for (;;)
    {
        LogFlow(("hgcmThread: Going to get a message\n"));

        rc = hgcmMsgGet (ThreadHandle, &pMsgCore);

        if (VBOX_FAILURE (rc))
        {
            Log(("hgcmThread: message get failed, rc = %Vrc\n"));
            RTThreadSleep(100);
            continue;
        }

        switch (pMsgCore->MsgId ())
        {
            case HGCMMSGID_CONNECT:
            {
                LogFlow(("HGCMMSGID_CONNECT\n"));

                HGCMMsgConnect *pMsg = (HGCMMsgConnect *)pMsgCore;

                /* Search if the service exists.
                 * Create an information structure for the client.
                 * Inform the service about the client.
                 * Generate and return the client id.
                 */

                Log(("MAIN::hgcmThread:HGCMMSGID_CONNECT: location type = %d\n", pMsg->pLoc->type));

                HGCMService *pService = NULL;

                rc = HGCMService::FindService (&pService, pMsg->pLoc);

                if (VBOX_SUCCESS (rc))
                {
                    /* Allocate a client information structure */

                    HGCMClient *pClient = new HGCMClient ();

                    if (!pClient)
                    {
                        Log(("hgcmConnect::Could not allocate HGCMClient\n"));
                        rc = VERR_NO_MEMORY;
                    }
                    else
                    {
                        uint32_t handle = hgcmObjGenerateHandle (pClient);

                        AssertRelease(handle);

                        rc = pClient->Init (pService);

                        if (VBOX_SUCCESS(rc))
                        {
                            rc = pService->Connect (handle);
                        }

                        if (VBOX_FAILURE(rc))
                        {
                            hgcmObjDeleteHandle (handle);
                        }
                        else
                        {
                            *pMsg->pu32ClientID = handle;
                        }
                    }
                }

                if (VBOX_FAILURE(rc))
                {
                    LogFlow(("HGCMMSGID_CONNECT: FAILURE rc = %Vrc\n", rc));

                    if (pService)
                    {
                        pService->ReleaseService ();
                    }
                }

            } break;

            case HGCMMSGID_DISCONNECT:
            {
                LogFlow(("HGCMMSGID_DISCONNECT\n"));

                HGCMMsgDisconnect *pMsg = (HGCMMsgDisconnect *)pMsgCore;

                Log(("MAIN::hgcmThread:HGCMMSGID_DISCONNECT: client id = %d\n", pMsg->u32ClientID));

                /* Forward call to the service dedicated HGCM thread. */
                HGCMClient *pClient = (HGCMClient *)hgcmObjReference (pMsg->u32ClientID);

                if (!pClient)
                {
                    Log(("MAIN::hgcmThread:HGCMMSGID_DISCONNECT: FAILURE resolving client id\n"));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    rc = pClient->pService->Disconnect (pMsg->u32ClientID);

                    pClient->pService->ReleaseService ();

                    hgcmObjDereference (pClient);

                    hgcmObjDeleteHandle (pMsg->u32ClientID);
                }

            } break;

            case HGCMMSGID_LOAD:
            {
                LogFlow(("HGCMMSGID_LOAD\n"));

                HGCMMsgLoad *pMsg = (HGCMMsgLoad *)pMsgCore;

                rc = HGCMService::LoadService (pMsg->pszServiceName, pMsg->pszServiceLibrary, pMsg->pHGCMPort);
            } break;


            case HGCMMSGID_HOSTCALL:
            {
                LogFlow(("HGCMMSGID_HOSTCALL at hgcmThread\n"));

                HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)pMsgCore;

                HGCMService *pService = HGCMService::FindServiceByName (pMsg->pszServiceName);

                if (pService)
                {
                    LogFlow(("HGCMMSGID_HOSTCALL found service, forwarding the call.\n"));
                    pService->HostCall (NULL, 0, pMsg->u32Function, pMsg->cParms, pMsg->paParms);
                }
            } break;
            
            default:
            {
                Log(("hgcmThread: Unsupported message number %08X!!!\n", pMsgCore->MsgId ()));
                rc = VERR_NOT_SUPPORTED;
            } break;
        }

        hgcmMsgComplete (pMsgCore, rc);
    }

    return;
}

static HGCMTHREADHANDLE g_hgcmThread = 0;

/*
 * Find a service and inform it about a client connection.
 * Main HGCM thread will process this request for serialization.
 */
int hgcmConnectInternal (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pLoc, uint32_t *pu32ClientID, bool fBlock)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmConnectInternal: pHGCMPort = %p, pCmd = %p, loc = %p, pu32ClientID = %p\n",
             pHGCMPort, pCmd, pLoc, pu32ClientID));

    if (!pHGCMPort || !pCmd || !pLoc || !pu32ClientID)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_CONNECT, sizeof (HGCMMsgConnect), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgConnect *pMsg = (HGCMMsgConnect *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->pCmd      = pCmd;
        pMsg->pHGCMPort = pHGCMPort;

        pMsg->pLoc = pLoc;
        pMsg->pu32ClientID = pu32ClientID;

        if (fBlock)
            rc = hgcmMsgSend (hMsg);
        else
            rc = hgcmMsgPost (hMsg, hgcmMsgCompletionCallback);

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcmConnectInternal: hgcmMsgPost returned %Vrc\n", rc));

        if (!fBlock && VBOX_SUCCESS(rc))
        {
            rc = VINF_HGCM_ASYNC_EXECUTE;
        }
    }
    else
    {
        Log(("MAIN::hgcmConnectInternal:Message allocation failed: %Vrc\n", rc));
    }


    return rc;
}

/*
 * Tell a service that the client is disconnecting.
 * Main HGCM thread will process this request for serialization.
 */
int hgcmDisconnectInternal (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, bool fBlock)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmDisconnectInternal: pHGCMPort = %p, pCmd = %p, u32ClientID = %d\n",
             pHGCMPort, pCmd, u32ClientID));

    if (!pHGCMPort || !pCmd)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_DISCONNECT, sizeof (HGCMMsgDisconnect), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgDisconnect *pMsg = (HGCMMsgDisconnect *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->pCmd      = pCmd;
        pMsg->pHGCMPort = pHGCMPort;

        pMsg->u32ClientID = u32ClientID;

        if (fBlock)
            rc = hgcmMsgSend (hMsg);
        else
            rc = hgcmMsgPost (hMsg, hgcmMsgCompletionCallback);

        hgcmObjDereference (pMsg);

        if (!fBlock && VBOX_SUCCESS(rc))
        {
            rc = VINF_HGCM_ASYNC_EXECUTE;
        }
    }
    else
    {
        Log(("MAIN::hgcmDisconnectInternal: Message allocation failed: %Vrc\n", rc));
    }


    return rc;
}

int hgcmLoadInternal (const char *pszServiceName, const char *pszServiceLibrary)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmLoadInternal: name = %s, lib = %s\n",
             pszServiceName, pszServiceLibrary));

    if (!pszServiceName || !pszServiceLibrary)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_LOAD, sizeof (HGCMMsgLoad), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgLoad *pMsg = (HGCMMsgLoad *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->pHGCMPort = NULL; /* Not used by the call. */
        
        rc = pMsg->Init (pszServiceName, pszServiceLibrary);
        
        if (VBOX_SUCCESS (rc))
        {
            rc = hgcmMsgSend (hMsg);
        }

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcm:LoadInternal: hgcmMsgSend returned %Vrc\n", rc));
    }
    else
    {
        Log(("MAIN::hgcmLoadInternal:Message allocation failed: %Vrc\n", rc));
    }


    return rc;
}

/*
 * Call a service.
 * The service dedicated thread will process this request.
 */
int hgcmGuestCallInternal (PPDMIHGCMPORT pHGCMPort, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[], bool fBlock)
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmCallInternal: pHGCMPort = %p, pCmd = %p, u32ClientID = %d, u32Function = %d, cParms = %d, aParms = %p\n",
             pHGCMPort, pCmd, u32ClientID, u32Function, cParms, aParms));

    if (!pHGCMPort || !pCmd)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMClient *pClient = (HGCMClient *)hgcmObjReference (u32ClientID);

    if (!pClient)
    {
        Log(("MAIN::hgcmCallInternal: FAILURE resolving client id %d\n", u32ClientID));
        return VERR_INVALID_PARAMETER;
    }

    AssertRelease(pClient->pService);

    rc = pClient->pService->GuestCall (pHGCMPort, pCmd, u32ClientID, u32Function, cParms, aParms, fBlock);

    hgcmObjDereference (pClient);

    return rc;
}

/*
 * Call a service.
 * The service dedicated thread will process this request.
 */
int hgcmHostCallInternal (const char *pszServiceName, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM aParms[])
{
    int rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmHostCallInternal: service = %s, u32Function = %d, cParms = %d, aParms = %p\n",
             pszServiceName, u32Function, cParms, aParms));

    if (!pszServiceName)
    {
        return VERR_INVALID_PARAMETER;
    }

    HGCMMSGHANDLE hMsg = 0;

    rc = hgcmMsgAlloc (g_hgcmThread, &hMsg, HGCMMSGID_HOSTCALL, sizeof (HGCMMsgHostCall), hgcmMessageAlloc);

    if (VBOX_SUCCESS(rc))
    {
        HGCMMsgHostCall *pMsg = (HGCMMsgHostCall *)hgcmObjReference (hMsg);

        AssertRelease(pMsg);

        pMsg->pHGCMPort = NULL; /* Not used. */
        
        pMsg->pszServiceName = (char *)pszServiceName;
        pMsg->u32Function = u32Function;
        pMsg->cParms = cParms;
        pMsg->paParms = &aParms[0];
        
        rc = hgcmMsgSend (hMsg);

        hgcmObjDereference (pMsg);

        LogFlow(("MAIN::hgcm:HostCallInternal: hgcmMsgSend returned %Vrc\n", rc));
    }
    else
    {
        Log(("MAIN::hgcmHostCallInternal:Message allocation failed: %Vrc\n", rc));
    }

    return rc;
}


int hgcmInit (void)
{
    int rc = VINF_SUCCESS;

    Log(("MAIN::hgcmInit\n"));

    rc = hgcmThreadInit ();

    if (VBOX_FAILURE(rc))
    {
        Log(("FAILURE: Can't init worker threads.\n"));
    }
    else
    {
        /* Start main HGCM thread that will process Connect/Disconnect requests. */

        /* @todo do a proper fix 0x12345678 -> sizeof */
        rc = hgcmThreadCreate (&g_hgcmThread, "MainHGCMthread", hgcmThread, NULL, 0x12345678 /*sizeof (HGCMMsgConnect)*/);

        if (VBOX_FAILURE (rc))
        {
            Log(("FAILURE: HGCM initialization.\n"));
        }
    }

    LogFlow(("MAIN::hgcmInit: rc = %Vrc\n", rc));

    return rc;
}
