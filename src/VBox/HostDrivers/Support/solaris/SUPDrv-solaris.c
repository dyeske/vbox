/** @file
 * VBox host drivers - Ring-0 support drivers - Solaris host:
 * Solaris driver C code
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/modctl.h>
#include <sys/open.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/file.h>
#undef u /* /usr/include/sys/user.h:249:1 is where this is defined to (curproc->p_user). very cool. */

#include "SUPDRV.h"
#include <iprt/spinlock.h>
#include <iprt/process.h>
#include <iprt/initterm.h>
#include <iprt/alloc.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The module name. */
#define DEVICE_NAME              "vboxdrv"
#define DEVICE_DESC              "VirtualBox Driver"
#define DEVICE_MAXINSTANCES      16


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VBoxDrvSolarisOpen(dev_t* pDev, int fFlag, int fType, cred_t* pCred);
static int VBoxDrvSolarisClose(dev_t Dev, int fFlag, int fType, cred_t* pCred);
static int VBoxDrvSolarisRead(dev_t Dev, struct uio* pUio, cred_t* pCred);
static int VBoxDrvSolarisWrite(dev_t Dev, struct uio* pUio, cred_t* pCred);
static int VBoxDrvSolarisIOCtl (dev_t Dev, int Cmd, intptr_t pArgs, int mode, cred_t* pCred, int* pVal);

static int VBoxDrvSolarisAttach(dev_info_t* pDip, ddi_attach_cmd_t Cmd);
static int VBoxDrvSolarisDetach(dev_info_t* pDip, ddi_detach_cmd_t Cmd);

static int VBoxSupDrvErr2SolarisErr(int rc);
static int VBoxDrvSolarisIOCtlSlow(PSUPDRVSESSION pSession, int Cmd, int Mode, intptr_t pArgs);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * cb_ops: for drivers that support char/block entry points
 */
static struct cb_ops g_VBoxDrvSolarisCbOps =
{
    VBoxDrvSolarisOpen,
    VBoxDrvSolarisClose,
    nodev,                  /* b strategy */
    nodev,                  /* b dump */
    nodev,                  /* b print */
    VBoxDrvSolarisRead,
    VBoxDrvSolarisWrite,
    VBoxDrvSolarisIOCtl,
    nodev,                  /* c devmap */
    nodev,                  /* c mmap */
    nodev,                  /* c segmap */
    nochpoll,               /* c poll */
    ddi_prop_op,            /* property ops */
    NULL,                   /* streamtab  */
    D_NEW | D_MP,           /* compat. flag */
    CB_REV                  /* revision */
};

/**
 * dev_ops: for driver device operations
 */
static struct dev_ops g_VBoxDrvSolarisDevOps =
{
    DEVO_REV,               /* driver build revision */
    0,                      /* ref count */
    nulldev,                /* get info */
    nulldev,                /* identify */
    nulldev,                /* probe */
    VBoxDrvSolarisAttach,
    VBoxDrvSolarisDetach,
    nodev,                  /* reset */
    &g_VBoxDrvSolarisCbOps,
    (struct bus_ops *)0,
    nodev                   /* power */
};

/**
 * modldrv: export driver specifics to the kernel
 */
static struct modldrv g_VBoxDrvSolarisModule =
{
    &mod_driverops,         /* extern from kernel */
    DEVICE_DESC,
    &g_VBoxDrvSolarisDevOps
};

/**
 * modlinkage: export install/remove/info to the kernel
 */
static struct modlinkage g_VBoxDrvSolarisModLinkage =
{
    MODREV_1,               /* loadable module system revision */
    &g_VBoxDrvSolarisModule,
    NULL                    /* terminate array of linkage structures */
};

/** State info. each our kernel module instances */
typedef struct
{
    dev_info_t*     pDip;   /* Device handle */
} vbox_devstate_t;

/** Opaque pointer to state */
static void* g_pVBoxDrvSolarisState;

/** Device extention & session data association structure */
static SUPDRVDEVEXT         g_DevExt;

/* GCC C++ hack. */
unsigned __gxx_personality_v0 = 0xcccccccc;

/** Hash table */
static PSUPDRVSESSION       g_apSessionHashTab[19];
/** Spinlock protecting g_apSessionHashTab. */
static RTSPINLOCK           g_Spinlock = NIL_RTSPINLOCK;
/** Calculates bucket index into g_apSessionHashTab.*/
#define SESSION_HASH(sfn) ((sfn) % RT_ELEMENTS(g_apSessionHashTab))

/**
 * Kernel entry points
 */
int _init (void)
{
    cmn_err(CE_CONT, "VBoxDrvSolaris _init");

    int e = ddi_soft_state_init(&g_pVBoxDrvSolarisState, sizeof (vbox_devstate_t), 1);
    if (e != 0)
        return e;

    e = mod_install(&g_VBoxDrvSolarisModLinkage);
    if (e != 0)
        ddi_soft_state_fini(&g_pVBoxDrvSolarisState);

    return e;
}

int _fini (void)
{
    cmn_err(CE_CONT, "VBoxDrvSolaris _fini");

    int e = mod_remove(&g_VBoxDrvSolarisModLinkage);
    if (e != 0)
        return e;

    ddi_soft_state_fini(&g_pVBoxDrvSolarisState);
    return e;
}

int _info (struct modinfo *pModInfo)
{
    cmn_err(CE_CONT, "VBoxDrvSolaris _info");
    return mod_info (&g_VBoxDrvSolarisModLinkage, pModInfo);
}

/**
 * User context entry points
 */
static int VBoxDrvSolarisOpen(dev_t *pDev, int fFlag, int fType, cred_t *pCred)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisOpen");

    int                 rc;
    PSUPDRVSESSION      pSession;

    /*
     * Create a new session.
     * Sessions in Solaris driver are mostly useless. It's however needed
     * in VBoxDrvSolarisIOCtlSlow() while calling supdrvIOCtl()
     */
    rc = supdrvCreateSession(&g_DevExt, &pSession);
    if (RT_SUCCESS(rc))
    {
        cmn_err(CE_NOTE,"supdrvCreateSession success");
        RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
        unsigned        iHash;

        pSession->Uid       = crgetuid(pCred);
        pSession->Gid       = crgetgid(pCred);
        pSession->Process   = RTProcSelf();
        pSession->R0Process = RTR0ProcHandleSelf();

        /*
         * Insert it into the hash table.
         */
        iHash = SESSION_HASH(pSession->Process);
        RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
        pSession->pNextHash = g_apSessionHashTab[iHash];
        g_apSessionHashTab[iHash] = pSession;
        RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
        cmn_err(CE_NOTE,"VBoxDrvSolarisOpen success");
    }

    int instance;
    for (instance = 0; instance < DEVICE_MAXINSTANCES; instance++)
    {
        vbox_devstate_t *pState = ddi_get_soft_state (g_pVBoxDrvSolarisState, instance);
        if (pState)
            break;
    }
    
    if (instance >= DEVICE_MAXINSTANCES)
    {
        cmn_err(CE_NOTE, "All instances exhausted\n");
        return ENXIO;
    }
    
    *pDev = makedevice(getmajor(*pDev), instance);
        
    return VBoxSupDrvErr2SolarisErr(rc);
}

static int VBoxDrvSolarisClose(dev_t pDev, int flag, int otyp, cred_t *cred)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisClose");

    RTSPINLOCKTMP   Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS Process = RTProcSelf();
    const unsigned  iHash = SESSION_HASH(Process);
    PSUPDRVSESSION  pSession;

    /*
     * Remove from the hash table.
     */
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession)
    {
        if (pSession->Process == Process)
        {
            g_apSessionHashTab[iHash] = pSession->pNextHash;
            pSession->pNextHash = NULL;
        }
        else
        {
            PSUPDRVSESSION pPrev = pSession;
            pSession = pSession->pNextHash;
            while (pSession)
            {
                if (pSession->Process == Process)
                {
                    pPrev->pNextHash = pSession->pNextHash;
                    pSession->pNextHash = NULL;
                    break;
                }

                /* next */
                pPrev = pSession;
                pSession = pSession->pNextHash;
            }
        }
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        OSDBGPRINT(("VBoxDrvSolarisClose: WHAT?!? pSession == NULL! This must be a mistake... pid=%d (close)\n", 
                    (int)Process));
        return DDI_FAILURE;
    }

    /*
     * Close the session.
     */
    supdrvCloseSession(&g_DevExt, pSession);
    return DDI_SUCCESS;
}

static int VBoxDrvSolarisRead(dev_t dev, struct uio* pUio, cred_t* credp)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisRead");
    return DDI_SUCCESS;
}

static int VBoxDrvSolarisWrite(dev_t dev, struct uio* pUio, cred_t* credp)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisWrite");
    return DDI_SUCCESS;
}

/**
 * Attach entry point, to attach a device to the system or resume it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
static int VBoxDrvSolarisAttach(dev_info_t* pDip, ddi_attach_cmd_t enmCmd)
{
    cmn_err(CE_CONT, "VBoxDrvSolarisAttach");
    int rc = VINF_SUCCESS;
    int instance = 0;
    vbox_devstate_t* pState;
    
    switch (enmCmd)
    {
        case DDI_ATTACH:
        {
            instance = ddi_get_instance (pDip);

            if (ddi_soft_state_zalloc(g_pVBoxDrvSolarisState, instance) != DDI_SUCCESS)
            {
                cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: state alloc failed");
                return DDI_FAILURE;
            }
            
            pState = ddi_get_soft_state(g_pVBoxDrvSolarisState, instance);
            
            /*
             * Initialize IPRT R0 driver, which internally calls OS-specific r0 init.
             */
            rc = RTR0Init(0);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Initialize the device extension
                 */
                rc = supdrvInitDevExt(&g_DevExt);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Initialize the session hash table.
                     */
                    memset(g_apSessionHashTab, 0, sizeof(g_apSessionHashTab));
                    rc = RTSpinlockCreate(&g_Spinlock);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Register ourselves as a character device, pseudo-driver
                         */
                        if (ddi_create_minor_node(pDip, "0", S_IFCHR,
                                instance, DDI_PSEUDO, 0) == DDI_SUCCESS)
                        {
                            pState->pDip = pDip;
                            ddi_report_dev(pDip);
                            return DDI_SUCCESS;
                        }

                        /* Is this really necessary? */
                        ddi_remove_minor_node(pDip, NULL);
                        cmn_err(CE_NOTE,"VBoxDrvSolarisAttach: ddi_create_minor_node failed.");

                        RTSpinlockDestroy(g_Spinlock);
                        g_Spinlock = NIL_RTSPINLOCK;
                    }
                    else
                        cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: RTSpinlockCreate failed");
                    supdrvDeleteDevExt(&g_DevExt);
                }
                else
                    cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: supdrvInitDevExt failed");
                RTR0Term ();
            }
            else
                cmn_err(CE_NOTE, "VBoxDrvSolarisAttach: failed to init R0Drv");
            memset(&g_DevExt, 0, sizeof(g_DevExt));
            break;
        }

        default:
            return DDI_FAILURE;
    }

    return DDI_FAILURE;
}

/**
 * Detach entry point, to detach a device to the system or suspend it.
 *
 * @param   pDip            The module structure instance.
 * @param   enmCmd          Attach type (ddi_attach_cmd_t)
 *
 * @return  corresponding solaris error code.
 */
static int VBoxDrvSolarisDetach(dev_info_t *pDip, ddi_detach_cmd_t enmCmd)
{
    int rc = VINF_SUCCESS;
    int instance;
    register vbox_devstate_t* pState;

    cmn_err(CE_CONT, "VBoxDrvSolarisDetach");
    switch (enmCmd)
    {
        case DDI_DETACH:
        {
            instance = ddi_get_instance(pDip);
            pState = ddi_get_soft_state(g_pVBoxDrvSolarisState, instance);

            ddi_remove_minor_node(pDip, NULL);
            ddi_soft_state_free(g_pVBoxDrvSolarisState, instance);
            
            rc = supdrvDeleteDevExt(&g_DevExt);
            AssertRC(rc);

            rc = RTSpinlockDestroy(g_Spinlock);
            AssertRC(rc);
            g_Spinlock = NIL_RTSPINLOCK;

            RTR0Term();

            memset(&g_DevExt, 0, sizeof(g_DevExt));
            cmn_err(CE_CONT, "VBoxDrvSolarisDetach: Clean Up Done.");
            return DDI_SUCCESS;
        }

        default:
            return DDI_FAILURE;
    }
}

/**
 * Driver ioctl, an alternate entry point for this character driver.
 *
 * @param   Dev             Device number
 * @param   Cmd             Operation identifier
 * @param   pArg            Arguments from user to driver
 * @param   Mode            Information bitfield (read/write, address space etc.)
 * @param   pCred           User credentials
 * @param   pVal            Return value for calling process.
 *
 * @return  corresponding solaris error code.
 */
static int VBoxDrvSolarisIOCtl (dev_t Dev, int Cmd, intptr_t pArgs, int Mode, cred_t* pCred, int* pVal)
{
    RTSPINLOCKTMP       Tmp = RTSPINLOCKTMP_INITIALIZER;
    const RTPROCESS     Process = RTProcSelf();
    const unsigned      iHash = SESSION_HASH(Process);
    PSUPDRVSESSION      pSession;

    cmn_err(CE_CONT, "VBoxDrvSolarisIOCtl\n");
    /*
     * Find the session.
     */
    RTSpinlockAcquireNoInts(g_Spinlock, &Tmp);
    pSession = g_apSessionHashTab[iHash];
    if (pSession && pSession->Process != Process)
    {
        do pSession = pSession->pNextHash;
        while (pSession && pSession->Process != Process);
    }
    RTSpinlockReleaseNoInts(g_Spinlock, &Tmp);
    if (!pSession)
    {
        OSDBGPRINT(("VBoxSupDrvIOCtl: WHAT?!? pSession == NULL! This must be a mistake... pid=%d iCmd=%#x\n", 
                    (int)Process, Cmd));
        return EINVAL;
    }

    /*
     * Deal with the two high-speed IOCtl that takes it's arguments from
     * the session and iCmd, and only returns a VBox status code.
     */
#ifdef VBOX_WITHOUT_IDT_PATCHING
    if (    Cmd == SUP_IOCTL_FAST_DO_RAW_RUN
        ||  Cmd == SUP_IOCTL_FAST_DO_HWACC_RUN
        ||  Cmd == SUP_IOCTL_FAST_DO_NOP)
        return supdrvIOCtlFast(Cmd, &g_DevExt, pSession);
#endif

    return VBoxDrvSolarisIOCtlSlow(pSession, Cmd, Mode, pArgs);
}

/**
 * Worker for VBoxSupDrvIOCtl that takes the slow IOCtl functions.
 *
 * @returns Solaris errno.
 *
 * @param pSession  The session.
 * @param Cmd       The IOCtl command.
 * @param Mode      Information bitfield (for specifying ownership of data)
 * @param pArgs     Pointer to the kernel copy of the SUPDRVIOCTLDATA buffer.
 */
static int VBoxDrvSolarisIOCtlSlow(PSUPDRVSESSION pSession, int Cmd, int Mode, intptr_t pArgs)
{
    int                 rc;
    void               *pvBuf = NULL;
    unsigned long       cbBuf = 0;
    unsigned            cbOut = 0;
    PSUPDRVIOCTLDATA    pArgData = (PSUPDRVIOCTLDATA)pArgs;

   /*
     * Allocate and copy user space input data buffer to kernel space.
     */
    if (pArgData->cbIn > 0 || pArgData->cbOut > 0)
    {
        cbBuf = max(pArgData->cbIn, pArgData->cbOut);
        pvBuf = RTMemTmpAlloc(cbBuf);

        if (pvBuf == NULL)
        {
            OSDBGPRINT(("VBoxDrvSolarisIOCtlSlow: failed to allocate buffer of %d bytes.\n", cbBuf));
            return ENOMEM;
        }
        
        rc = ddi_copyin(pArgData->pvIn, pvBuf, pArgData->cbIn, Mode);
        
        if (rc != 0)
        {
            OSDBGPRINT(("VBoxDrvSolarisIOCtlSlow: ddi_copyin(%p,%d) failed.\n", pArgData->pvIn, pArgData->cbIn));

            RTMemTmpFree(pvBuf);
            return EFAULT;
        }
    }
    
    /*
     * Process the IOCtl.
     */
    rc = supdrvIOCtl(Cmd, &g_DevExt, pSession, pvBuf, pArgData->cbIn, pvBuf, pArgData->cbOut, &cbOut);
    
    /*
     * Copy ioctl data and output buffer back to user space.
     */
    if (rc != 0)
        rc = VBoxSupDrvErr2SolarisErr(rc);
    else if (cbOut > 0)
    {
        if (pvBuf != NULL && cbOut <= cbBuf)
        {
            rc = ddi_copyout(pvBuf, pArgData->pvOut, cbOut, Mode);
            if (rc != 0)
            {
                OSDBGPRINT(("VBoxDrvSolarisIOCtlSlow: ddi_copyout(,%p,%d) failed.\n", pArgData->pvOut, cbBuf));

                /** @todo r=bird: why this extra return? setting rc = EFAULT; should do the trick, shouldn't it? */
                RTMemTmpFree(pvBuf);
                return EFAULT;
            }
        }
        else
        {
            OSDBGPRINT(("WHAT!?! supdrvIOCtl messed up! cbOut=%d cbBuf=%d pvBuf=%p\n", cbOut, cbBuf, pvBuf));
            rc = EPERM;
        }
    }

    if (pvBuf)
        RTMemTmpFree(pvBuf);
    
    OSDBGPRINT(("VBoxDrvSolarisIOCtlSlow: returns %d cbOut=%d\n", rc, cbOut));
    return rc;
}


/**
 * Converts an supdrv error code to a solaris error code.
 *
 * @returns corresponding solaris error code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static int VBoxSupDrvErr2SolarisErr(int rc)
{
    switch (rc)
    {
        case 0:                             return 0;
        case SUPDRV_ERR_GENERAL_FAILURE:    return EACCES;
        case SUPDRV_ERR_INVALID_PARAM:      return EINVAL;
        case SUPDRV_ERR_INVALID_MAGIC:      return EILSEQ;
        case SUPDRV_ERR_INVALID_HANDLE:     return ENXIO;
        case SUPDRV_ERR_INVALID_POINTER:    return EFAULT;
        case SUPDRV_ERR_LOCK_FAILED:        return ENOLCK;
        case SUPDRV_ERR_ALREADY_LOADED:     return EEXIST;
        case SUPDRV_ERR_PERMISSION_DENIED:  return EPERM;
        case SUPDRV_ERR_VERSION_MISMATCH:   return ENOSYS;
    }

    return EPERM;
}

/**
 * Initializes any OS specific object creator fields.
 */
void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
{
    NOREF(pObj);
    NOREF(pSession);
}


/**
 * Checks if the session can access the object.
 *
 * @returns true if a decision has been made.
 * @returns false if the default access policy should be applied.
 *
 * @param   pObj        The object in question.
 * @param   pSession    The session wanting to access the object.
 * @param   pszObjName  The object name, can be NULL.
 * @param   prc         Where to store the result when returning true.
 */
bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}

RTDECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list     args;
    char        szMsg[512];

    va_start(args, pszFormat);
    vsnprintf(szMsg, sizeof(szMsg) - 1, pszFormat, args);
    va_end(args);

    szMsg[sizeof(szMsg) - 1] = '\0';
    printf("%s", szMsg);
    return 0;
}
