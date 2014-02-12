/** @file
 *
 * VBox Host Guest Shared Memory Interface (HGSMI).
 * Host/Guest shared part.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___VBox_HGSMI_HGSMI_h
#define ___VBox_HGSMI_HGSMI_h

#include <iprt/assert.h>
#include <iprt/types.h>

#include <VBox/HGSMI/HGSMIDefs.h>
#include <VBox/HGSMI/HGSMIChannels.h>

/*
 * Basic mechanism for the HGSMI is to prepare and pass data buffer to the host and the guest.
 * Data inside these buffers are opaque for the HGSMI and are interpreted by higher levels.
 *
 * Every shared memory buffer passed between the guest/host has the following structure:
 *
 * HGSMIBUFFERHEADER header;
 * uint8_t data[header.u32BufferSize];
 * HGSMIBUFFERTAIL tail;
 *
 * Note: Offset of the 'header' in the memory is used for virtual hardware IO.
 *
 * Buffers are verifyed using the offset and the content of the header and the tail,
 * which are constant during a call.
 *
 * Invalid buffers are ignored.
 *
 * Actual 'data' is not verifyed, as it is expected that the data can be changed by the
 * called function.
 *
 * Since only the offset of the buffer is passed in a IO operation, the header and tail
 * must contain:
 *     * size of data in this buffer;
 *     * checksum for buffer verification.
 *
 * For segmented transfers:
 *     * the sequence identifier;
 *     * offset of the current segment in the sequence;
 *     * total bytes in the transfer.
 *
 * Additionally contains:
 *     * the channel ID;
 *     * the channel information.
 */

#pragma pack(1)
typedef struct _HGSMIHEAP
{
    union
    {
        RTHEAPSIMPLE  hPtr;         /**< Pointer based heap. */
        RTHEAPOFFSET  hOff;         /**< Offset based heap. */
    } u;
    HGSMIAREA     area;             /**< Description. */
    int           cRefs;            /**< Number of heap allocations. */
    bool          fOffsetBased;     /**< Set if offset based. */
} HGSMIHEAP;
#pragma pack()

#pragma pack(1)
/* The size of the array of channels. Array indexes are uint8_t. Note: the value must not be changed. */
#define HGSMI_NUMBER_OF_CHANNELS 0x100

/* Channel handler called when the guest submits a buffer. */
typedef DECLCALLBACK(int) FNHGSMICHANNELHANDLER(void *pvHandler, uint16_t u16ChannelInfo, void *pvBuffer, HGSMISIZE cbBuffer);
typedef FNHGSMICHANNELHANDLER *PFNHGSMICHANNELHANDLER;

/* Information about a handler: pfn + context. */
typedef struct _HGSMICHANNELHANDLER
{
    PFNHGSMICHANNELHANDLER pfnHandler;
    void *pvHandler;
} HGSMICHANNELHANDLER;

/* Channel description. */
typedef struct _HGSMICHANNEL
{
    HGSMICHANNELHANDLER handler;       /* The channel handler. */
    const char *pszName;               /* NULL for hardcoded channels or RTStrDup'ed name. */
    uint8_t u8Channel;                 /* The channel id, equal to the channel index in the array. */
    uint8_t u8Flags;                   /* HGSMI_CH_F_* */
} HGSMICHANNEL;

typedef struct _HGSMICHANNELINFO
{
    HGSMICHANNEL Channels[HGSMI_NUMBER_OF_CHANNELS]; /* Channel handlers indexed by the channel id.
                                                      * The array is accessed under the instance lock.
                                                      */
}  HGSMICHANNELINFO;
#pragma pack()


RT_C_DECLS_BEGIN

DECLINLINE(HGSMISIZE) HGSMIBufferMinimumSize (void)
{
    return sizeof (HGSMIBUFFERHEADER) + sizeof (HGSMIBUFFERTAIL);
}

DECLINLINE(uint8_t *) HGSMIBufferData (const HGSMIBUFFERHEADER *pHeader)
{
    return (uint8_t *)pHeader + sizeof (HGSMIBUFFERHEADER);
}

DECLINLINE(HGSMIBUFFERTAIL *) HGSMIBufferTail (const HGSMIBUFFERHEADER *pHeader)
{
    return (HGSMIBUFFERTAIL *)(HGSMIBufferData (pHeader) + pHeader->u32DataSize);
}

DECLINLINE(HGSMIBUFFERHEADER *) HGSMIBufferHeaderFromData (const void *pvData)
{
    return (HGSMIBUFFERHEADER *)((uint8_t *)pvData - sizeof (HGSMIBUFFERHEADER));
}

DECLINLINE(HGSMISIZE) HGSMIBufferRequiredSize (uint32_t u32DataSize)
{
    return HGSMIBufferMinimumSize () + u32DataSize;
}

DECLINLINE(HGSMIOFFSET) HGSMIPointerToOffset (const HGSMIAREA *pArea,
                                              const HGSMIBUFFERHEADER *pHeader)
{
    return pArea->offBase + (HGSMIOFFSET)((uint8_t *)pHeader - pArea->pu8Base);
}

DECLINLINE(HGSMIBUFFERHEADER *) HGSMIOffsetToPointer (const HGSMIAREA *pArea,
                                                      HGSMIOFFSET offBuffer)
{
    return (HGSMIBUFFERHEADER *)(pArea->pu8Base + (offBuffer - pArea->offBase));
}

DECLINLINE(uint8_t *) HGSMIBufferDataFromOffset (const HGSMIAREA *pArea, HGSMIOFFSET offBuffer)
{
    HGSMIBUFFERHEADER *pHeader = HGSMIOffsetToPointer (pArea, offBuffer);
    Assert(pHeader);
    if(pHeader)
        return HGSMIBufferData(pHeader);
    return NULL;
}

DECLINLINE(uint8_t *) HGSMIBufferDataAndChInfoFromOffset (const HGSMIAREA *pArea, HGSMIOFFSET offBuffer, uint16_t * pChInfo)
{
    HGSMIBUFFERHEADER *pHeader = HGSMIOffsetToPointer (pArea, offBuffer);
    Assert(pHeader);
    if(pHeader)
    {
        *pChInfo = pHeader->u16ChannelInfo;
        return HGSMIBufferData(pHeader);
    }
    return NULL;
}

HGSMICHANNEL *HGSMIChannelFindById (HGSMICHANNELINFO * pChannelInfo, uint8_t u8Channel);

uint32_t HGSMIChecksum (HGSMIOFFSET offBuffer,
                        const HGSMIBUFFERHEADER *pHeader,
                        const HGSMIBUFFERTAIL *pTail);

int HGSMIAreaInitialize (HGSMIAREA *pArea,
                         void *pvBase,
                         HGSMISIZE cbArea,
                         HGSMIOFFSET offBase);

void HGSMIAreaClear (HGSMIAREA *pArea);

DECLINLINE(bool) HGSMIAreaContainsOffset(HGSMIAREA *pArea, HGSMIOFFSET offSet)
{
    return pArea->offBase <= offSet && pArea->offBase + pArea->cbArea > offSet;
}

HGSMIOFFSET HGSMIBufferInitializeSingle (const HGSMIAREA *pArea,
                                         HGSMIBUFFERHEADER *pHeader,
                                         HGSMISIZE cbBuffer,
                                         uint8_t u8Channel,
                                         uint16_t u16ChannelInfo);

int HGSMIHeapSetup (HGSMIHEAP *pHeap,
                    void *pvBase,
                    HGSMISIZE cbArea,
                    HGSMIOFFSET offBase,
                    bool fOffsetBased);

int HGSMIHeapRelocate (HGSMIHEAP *pHeap,
                       void *pvBase,
                       uint32_t offHeapHandle,
                       uintptr_t offDelta,
                       HGSMISIZE cbArea,
                       HGSMIOFFSET offBase,
                       bool fOffsetBased);

void HGSMIHeapSetupUnitialized (HGSMIHEAP *pHeap);
bool HGSMIHeapIsItialized (HGSMIHEAP *pHeap);

void HGSMIHeapDestroy (HGSMIHEAP *pHeap);

void* HGSMIHeapBufferAlloc (HGSMIHEAP *pHeap,
        HGSMISIZE cbBuffer);

void HGSMIHeapBufferFree(HGSMIHEAP *pHeap,
                    void *pvBuf);

void *HGSMIHeapAlloc (HGSMIHEAP *pHeap,
                      HGSMISIZE cbData,
                      uint8_t u8Channel,
                      uint16_t u16ChannelInfo);

HGSMIOFFSET HGSMIHeapBufferOffset (HGSMIHEAP *pHeap,
                                   void *pvData);

void HGSMIHeapFree (HGSMIHEAP *pHeap,
                    void *pvData);

DECLINLINE(HGSMIOFFSET) HGSMIHeapOffset(HGSMIHEAP *pHeap)
{
    return pHeap->area.offBase;
}

#ifdef IN_RING3
/* needed for heap relocation */
DECLINLINE(HGSMIOFFSET) HGSMIHeapHandleLocationOffset(HGSMIHEAP *pHeap)
{
#if (__GNUC__ * 100 + __GNUC_MINOR__) < 405
    /* does not work with gcc-4.5 */
    AssertCompile((uintptr_t)NIL_RTHEAPSIMPLE == (uintptr_t)NIL_RTHEAPOFFSET);
#endif
    return pHeap->u.hPtr != NIL_RTHEAPSIMPLE
        ? (HGSMIOFFSET)(pHeap->area.pu8Base - (uint8_t*)pHeap->u.hPtr)
        : HGSMIOFFSET_VOID;
}
#endif /* IN_RING3 */

DECLINLINE(HGSMISIZE) HGSMIHeapSize(HGSMIHEAP *pHeap)
{
    return pHeap->area.cbArea;
}

int HGSMIChannelRegister (HGSMICHANNELINFO * pChannelInfo,
                                 uint8_t u8Channel,
                                 const char *pszName,
                                 PFNHGSMICHANNELHANDLER pfnChannelHandler,
                                 void *pvChannelHandler,
                                 HGSMICHANNELHANDLER *pOldHandler);

int HGSMIBufferProcess (HGSMIAREA *pArea,
                         HGSMICHANNELINFO * pChannelInfo,
                         HGSMIOFFSET offBuffer);
RT_C_DECLS_END

#endif /* !___VBox_HGSMI_HGSMI_h */

