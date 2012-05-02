/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPWddm.h"
#include <iprt/semaphore.h>

/* SHGSMI */
DECLINLINE(void) vboxSHGSMICommandRetain (PVBOXSHGSMIHEADER pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

void vboxSHGSMICommandFree (PVBOXSHGSMI pHeap, PVBOXSHGSMIHEADER pCmd)
{
    VBoxSHGSMIHeapFree(pHeap, pCmd);
}

DECLINLINE(void) vboxSHGSMICommandRelease (PVBOXSHGSMI pHeap, PVBOXSHGSMIHEADER pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if(!cRefs)
        vboxSHGSMICommandFree (pHeap, pCmd);
}

DECLCALLBACK(void) vboxSHGSMICompletionSetEvent(PVBOXSHGSMI pHeap, void *pvCmd, void *pvContext)
{
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

DECLCALLBACK(void) vboxSHGSMICompletionCommandRelease(PVBOXSHGSMI pHeap, void *pvCmd, void *pvContext)
{
    vboxSHGSMICommandRelease (pHeap, VBoxSHGSMIBufferHeader(pvCmd));
}

/* do not wait for completion */
DECLINLINE(const VBOXSHGSMIHEADER*) vboxSHGSMICommandPrepAsynch (PVBOXSHGSMI pHeap, PVBOXSHGSMIHEADER pHeader)
{
    /* ensure the command is not removed until we're processing it */
    vboxSHGSMICommandRetain(pHeader);
    return pHeader;
}

DECLINLINE(void) vboxSHGSMICommandDoneAsynch (PVBOXSHGSMI pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    if(!(ASMAtomicReadU32((volatile uint32_t *)&pHeader->fFlags) & VBOXSHGSMI_FLAG_HG_ASYNCH))
    {
        PFNVBOXSHGSMICMDCOMPLETION pfnCompletion = (PFNVBOXSHGSMICMDCOMPLETION)pHeader->u64Info1;
        if (pfnCompletion)
            pfnCompletion(pHeap, VBoxSHGSMIBufferData (pHeader), (PVOID)pHeader->u64Info2);
    }

    vboxSHGSMICommandRelease(pHeap, (PVBOXSHGSMIHEADER)pHeader);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynchEvent (PVBOXSHGSMI pHeap, PVOID pvBuff, RTSEMEVENT hEventSem)
{
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)vboxSHGSMICompletionSetEvent;
    pHeader->u64Info2 = (uint64_t)hEventSem;
    pHeader->fFlags   = VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ;

    return vboxSHGSMICommandPrepAsynch (pHeap, pHeader);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepSynch (PVBOXSHGSMI pHeap, PVOID pCmd)
{
    RTSEMEVENT hEventSem;
    int rc = RTSemEventCreate(&hEventSem);
    Assert(RT_SUCCESS(rc));
    if (RT_SUCCESS(rc))
    {
        return VBoxSHGSMICommandPrepAsynchEvent (pHeap, pCmd, hEventSem);
    }
    return NULL;
}

void VBoxSHGSMICommandDoneAsynch (PVBOXSHGSMI pHeap, const VBOXSHGSMIHEADER * pHeader)
{
    vboxSHGSMICommandDoneAsynch(pHeap, pHeader);
}

int VBoxSHGSMICommandDoneSynch (PVBOXSHGSMI pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    VBoxSHGSMICommandDoneAsynch (pHeap, pHeader);
    RTSEMEVENT hEventSem = (RTSEMEVENT)pHeader->u64Info2;
    int rc = RTSemEventWait(hEventSem, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        RTSemEventDestroy(hEventSem);
    return rc;
}

void VBoxSHGSMICommandCancelAsynch (PVBOXSHGSMI pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    vboxSHGSMICommandRelease(pHeap, (PVBOXSHGSMIHEADER)pHeader);
}

void VBoxSHGSMICommandCancelSynch (PVBOXSHGSMI pHeap, const VBOXSHGSMIHEADER* pHeader)
{
    VBoxSHGSMICommandCancelAsynch (pHeap, pHeader);
    RTSEMEVENT hEventSem = (RTSEMEVENT)pHeader->u64Info2;
    RTSemEventDestroy(hEventSem);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynch (PVBOXSHGSMI pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags &= ~VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)pfnCompletion;
    pHeader->u64Info2 = (uint64_t)pvCompletion;
    pHeader->fFlags = fFlags;

    return vboxSHGSMICommandPrepAsynch (pHeap, pHeader);
}

const VBOXSHGSMIHEADER* VBoxSHGSMICommandPrepAsynchIrq (PVBOXSHGSMI pHeap, PVOID pvBuff, PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCompletion, PVOID pvCompletion, uint32_t fFlags)
{
    fFlags |= VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ | VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ;
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader (pvBuff);
    pHeader->u64Info1 = (uint64_t)pfnCompletion;
    pHeader->u64Info2 = (uint64_t)pvCompletion;
    /* we must assign rather than or because flags field does not get zeroed on command creation */
    pHeader->fFlags = fFlags;

    return vboxSHGSMICommandPrepAsynch (pHeap, pHeader);
}

void* VBoxSHGSMIHeapAlloc(PVBOXSHGSMI pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo)
{
    KIRQL OldIrql;
    void* pvData;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    pvData = HGSMIHeapAlloc(&pHeap->Heap, cbData, u8Channel, u16ChannelInfo);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
    if (!pvData)
        WARN(("HGSMIHeapAlloc failed!"));
    return pvData;
}

void VBoxSHGSMIHeapFree(PVBOXSHGSMI pHeap, void *pvBuffer)
{
    KIRQL OldIrql;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    HGSMIHeapFree(&pHeap->Heap, pvBuffer);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
}

void* VBoxSHGSMIHeapBufferAlloc(PVBOXSHGSMI pHeap, HGSMISIZE cbData)
{
    KIRQL OldIrql;
    void* pvData;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    pvData = HGSMIHeapBufferAlloc(&pHeap->Heap, cbData);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
    if (!pvData)
        WARN(("HGSMIHeapAlloc failed!"));
    return pvData;
}

void VBoxSHGSMIHeapBufferFree(PVBOXSHGSMI pHeap, void *pvBuffer)
{
    KIRQL OldIrql;
    Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&pHeap->HeapLock, &OldIrql);
    HGSMIHeapBufferFree(&pHeap->Heap, pvBuffer);
    KeReleaseSpinLock(&pHeap->HeapLock, OldIrql);
}

int VBoxSHGSMIInit(PVBOXSHGSMI pHeap, void *pvBase, HGSMISIZE cbArea, HGSMIOFFSET offBase, bool fOffsetBased)
{
    KeInitializeSpinLock(&pHeap->HeapLock);
    return HGSMIHeapSetup(&pHeap->Heap, pvBase, cbArea, offBase, fOffsetBased);
}

void VBoxSHGSMITerm(PVBOXSHGSMI pHeap)
{
    HGSMIHeapDestroy(&pHeap->Heap);
}

void* VBoxSHGSMICommandAlloc(PVBOXSHGSMI pHeap, HGSMISIZE cbData, uint8_t u8Channel, uint16_t u16ChannelInfo)
{
    /* Issue the flush command. */
    PVBOXSHGSMIHEADER pHeader = (PVBOXSHGSMIHEADER)VBoxSHGSMIHeapAlloc(pHeap, cbData + sizeof (VBOXSHGSMIHEADER), u8Channel, u16ChannelInfo);
    Assert(pHeader);
    if (pHeader)
    {
        pHeader->cRefs = 1;
        return VBoxSHGSMIBufferData(pHeader);
    }
    return NULL;
}

void VBoxSHGSMICommandFree(PVBOXSHGSMI pHeap, void *pvBuffer)
{
    PVBOXSHGSMIHEADER pHeader = VBoxSHGSMIBufferHeader(pvBuffer);
    vboxSHGSMICommandRelease (pHeap, pHeader);
}

#define VBOXSHGSMI_CMD2LISTENTRY(_pCmd) ((PVBOXVTLIST_ENTRY)&(_pCmd)->pvNext)
#define VBOXSHGSMI_LISTENTRY2CMD(_pEntry) ( (PVBOXSHGSMIHEADER)((uint8_t *)(_pEntry) - RT_OFFSETOF(VBOXSHGSMIHEADER, pvNext)) )

int VBoxSHGSMICommandProcessCompletion (PVBOXSHGSMI pHeap, VBOXSHGSMIHEADER* pCur, bool bIrq, PVBOXVTLIST pPostProcessList)
{
    int rc = VINF_SUCCESS;

    do
    {
        if (pCur->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ)
        {
            Assert(bIrq);

            PFNVBOXSHGSMICMDCOMPLETION pfnCompletion = NULL;
            void *pvCompletion;
            PFNVBOXSHGSMICMDCOMPLETION_IRQ pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION_IRQ)pCur->u64Info1;
            void *pvCallback = (void*)pCur->u64Info2;

            pfnCallback(pHeap, VBoxSHGSMIBufferData(pCur), pvCallback, &pfnCompletion, &pvCompletion);
            if (pfnCompletion)
            {
                pCur->u64Info1 = (uint64_t)pfnCompletion;
                pCur->u64Info2 = (uint64_t)pvCompletion;
                pCur->fFlags &= ~VBOXSHGSMI_FLAG_GH_ASYNCH_CALLBACK_IRQ;
            }
            else
            {
                /* nothing to do with this command */
                break;
            }
        }

        if (!bIrq)
        {
            PFNVBOXSHGSMICMDCOMPLETION pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION)pCur->u64Info1;
            void *pvCallback = (void*)pCur->u64Info2;
            pfnCallback(pHeap, VBoxSHGSMIBufferData(pCur), pvCallback);
        }
        else
            vboxVtListPut(pPostProcessList, VBOXSHGSMI_CMD2LISTENTRY(pCur), VBOXSHGSMI_CMD2LISTENTRY(pCur));
    } while (0);


    return rc;
}

int VBoxSHGSMICommandPostprocessCompletion (PVBOXSHGSMI pHeap, PVBOXVTLIST pPostProcessList)
{
    PVBOXVTLIST_ENTRY pNext, pCur;
    for (pCur = pPostProcessList->pFirst; pCur; pCur = pNext)
    {
        /* need to save next since the command may be released in a pfnCallback and thus its data might be invalid */
        pNext = pCur->pNext;
        PVBOXSHGSMIHEADER pCmd = VBOXSHGSMI_LISTENTRY2CMD(pCur);
        PFNVBOXSHGSMICMDCOMPLETION pfnCallback = (PFNVBOXSHGSMICMDCOMPLETION)pCmd->u64Info1;
        void *pvCallback = (void*)pCmd->u64Info2;
        pfnCallback(pHeap, VBoxSHGSMIBufferData(pCmd), pvCallback);
    }

    return VINF_SUCCESS;
}
