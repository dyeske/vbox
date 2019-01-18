/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver miscellaneous helpers and common includes.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaExt_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaExt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Svga.h"

#define VBOXWDDM_GA_MAX_FENCE_OBJECTS 4096

/* Gallium related device extension. */
typedef struct VBOXWDDM_EXT_GA
{
    union
    {
        /* Pointers to HW specific structs. */
        PVBOXWDDM_EXT_VMSVGA pSvga;
        void *pv;
    } hw;

    volatile uint32_t u32LastSubmittedFenceId; /* Updated in GaDxgkSubmitCommand. */
    volatile uint32_t u32LastCompletedFenceId; /* Updated in ISR. */
    volatile uint32_t u32PreemptionFenceId; /* Updated in GaDxgkDdiPreemptCommand. */
    volatile uint32_t u32LastCompletedSeqNo; /* Updated in DPC routine. */

    struct
    {
        /* Generation of SeqNo's. */
        volatile uint32_t u32SeqNoSource;
        /* Lock for accessing fence objects. Spin lock because it is used in DPC routine too. */
        KIRQL OldIrql;
        KSPIN_LOCK SpinLock;
        /* List of all fence objects. */
        RTLISTANCHOR list;
        /** Bitmap of used fence handles. Bit 0 - fence handle 0, etc. */
        uint32_t au32HandleBits[(VBOXWDDM_GA_MAX_FENCE_OBJECTS + 31) / 32];
    } fenceObjects;
} VBOXWDDM_EXT_GA;

/* Fence object (FO). */
typedef struct GAFENCEOBJECT
{
    volatile uint32_t cRefs;    /* By UM driver, by waiter, during submission. */
    uint32_t u32FenceHandle;    /* Unique identifier, used for communications with UM driver. */
    uint32_t u32FenceState;     /* GAFENCE_STATE_* */
    uint32_t fu32FenceFlags;    /* GAFENCE_F_* */
    uint32_t u32SubmissionFenceId; /* DXGK fence id. */
    uint32_t u32SeqNo;          /* Gallium Sequence Number, generated by the miniport. */
    PVBOXWDDM_DEVICE pDevice;   /* Device the fence is associated with. */
    KEVENT event;               /* Allows to wait for the fence completion. */
    RTLISTNODE node;            /* For the per adapter list of fence objects. */
    uint64_t u64SubmittedTS;    /* Nanoseconds timestamp when the corresponding buffer was submitted. */
} GAFENCEOBJECT;

#define GAFENCE_STATE_IDLE      0
#define GAFENCE_STATE_SUBMITTED 1
#define GAFENCE_STATE_SIGNALED  2

#define GAFENCE_F_WAITED        0x1 /* KEVENT is initialized and there is(are) waiter(s). */

NTSTATUS GaFenceCreate(PVBOXWDDM_EXT_GA pGaDevExt,
                       PVBOXWDDM_DEVICE pDevice,
                       uint32_t *pu32FenceHandle);
NTSTATUS GaFenceQuery(PVBOXWDDM_EXT_GA pGaDevExt,
                      uint32_t u32FenceHandle,
                      uint32_t *pu32SubmittedSeqNo,
                      uint32_t *pu32ProcessedSeqNo,
                      uint32_t *pu32FenceStatus);
NTSTATUS GaFenceWait(PVBOXWDDM_EXT_GA pGaDevExt,
                     uint32_t u32FenceHandle,
                     uint32_t u32TimeoutUS);
NTSTATUS GaFenceUnref(PVBOXWDDM_EXT_GA pGaDevExt,
                      uint32_t u32FenceHandle);

DECLINLINE(void) gaFenceObjectsLock(VBOXWDDM_EXT_GA *pGaDevExt)
{
    KeAcquireSpinLock(&pGaDevExt->fenceObjects.SpinLock, &pGaDevExt->fenceObjects.OldIrql);
}

DECLINLINE(void) gaFenceObjectsUnlock(VBOXWDDM_EXT_GA *pGaDevExt)
{
    KeReleaseSpinLock(&pGaDevExt->fenceObjects.SpinLock, pGaDevExt->fenceObjects.OldIrql);
}

void GaFenceObjectsDestroy(VBOXWDDM_EXT_GA *pGaDevExt,
                           PVBOXWDDM_DEVICE pDevice);
GAFENCEOBJECT *GaFenceLookup(VBOXWDDM_EXT_GA *pGaDevExt,
                             uint32_t u32FenceHandle);
void GaFenceUnrefLocked(VBOXWDDM_EXT_GA *pGaDevExt,
                        GAFENCEOBJECT *pFO);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_mp_wddm_gallium_VBoxMPGaExt_h */
