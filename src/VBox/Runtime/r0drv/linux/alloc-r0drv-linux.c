/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Memory Allocation, Ring-0 Driver, Linux.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include "r0drv/alloc-r0drv.h"


/**
 * OS specific allocation function.
 */
PRTMEMHDR rtMemAlloc(size_t cb, uint32_t fFlags)
{
    /*
     * Allocate.
     */
    PRTMEMHDR pHdr;
    Assert(cb != sizeof(void *)); /* 99% of pointer sized allocations are wrong. */
    if (fFlags & RTMEMHDR_FLAG_EXEC)
    {
#if defined(__AMD64__)
#if 0
        /*
         * We need memory in the module range (~2GB to ~0) this can only be obtained
         * thru APIs that are not exported (see module_alloc()).
         *
         * So, we'll have to create a quick and dirty heap here using BSS memory. 
         * Very annoying and it's going to restrict us!
         */
        static uint8_t      s_abMemory[_2M];
        static uint8_t     *s_pbMemory = NULL; /**< NULL if not initialized. */
        static RTSPINLOCK   s_Spinlock = NIL_RTSPINLOCK;
        static struct RTMEMRECLNXEXEC
        {
            PRTMEMHDR   pHdr;   /**< NULL if no free range. */
            uint32_t    off;    /**< Offset into s_pbMemory. */
            uint32_t    cb;     
        }                   s_aMemRecs[64];
        RTSPINLOCKTMP       SpinlockTmp = RTSPINLOCKTMP_INITIALIZER;

        if (!s_pbMemory) 
        {
            /* serialize */
        }

        RTSpinlockAcquireNoInts(s_Spinlock, &SpinlockTmp);
        /* find free area and split it... */

        RTSpinlockReleaseNoInts(s_Spinlock, &SpinlockTmp);

# else
        pHdr = (PRTMEMHDR)__vmalloc(cb + sizeof(*pHdr), GFP_KERNEL | __GFP_HIGHMEM, PAGE_KERNEL_EXEC);
# endif 


#elif defined(PAGE_KERNEL_EXEC) && defined(CONFIG_X86_PAE)
        pHdr = (PRTMEMHDR)__vmalloc(cb + sizeof(*pHdr), GFP_KERNEL | __GFP_HIGHMEM,
                                    __pgprot(cpu_has_pge ? _PAGE_KERNEL_EXEC | _PAGE_GLOBAL : _PAGE_KERNEL_EXEC));
#else
        pHdr = (PRTMEMHDR)vmalloc(cb + sizeof(*pHdr));
#endif
        fFlags &= ~RTMEMHDR_FLAG_KMALLOC;
    }
    else
    {
        if (cb <= PAGE_SIZE)
        {
            fFlags |= RTMEMHDR_FLAG_KMALLOC;
            pHdr = kmalloc(cb + sizeof(*pHdr), GFP_KERNEL);
        }
        else
        {
            fFlags &= ~RTMEMHDR_FLAG_KMALLOC;
            pHdr = vmalloc(cb + sizeof(*pHdr));
        }
    }

    /*
     * Initialize.
     */
    if (pHdr)
    {
        pHdr->u32Magic  = RTMEMHDR_MAGIC;
        pHdr->fFlags    = fFlags;
        pHdr->cb        = cb;
        pHdr->u32Padding= 0;
    }
    return pHdr;
}


/**
 * OS specific free function.
 */
void rtMemFree(PRTMEMHDR pHdr)
{
    pHdr->u32Magic += 1;
    if (pHdr->fFlags & RTMEMHDR_FLAG_KMALLOC)
        kfree(pHdr);
    else
        vfree(pHdr);
}


/**
 * Compute order. Some functions allocate 2^order pages.
 *
 * @returns order.
 * @param   cPages      Number of pages.
 */
static int CalcPowerOf2Order(unsigned long cPages)
{
    int             iOrder;
    unsigned long   cTmp;

    for (iOrder = 0, cTmp = cPages; cTmp >>= 1; ++iOrder)
        ;
    if (cPages & ~(1 << iOrder))
        ++iOrder;

    return iOrder;
}


/**
 * Allocates physical contiguous memory (below 4GB).
 * The allocation is page aligned and the content is undefined.
 *
 * @returns Pointer to the memory block. This is page aligned.
 * @param   pPhys   Where to store the physical address.
 * @param   cb      The allocation size in bytes. This is always
 *                  rounded up to PAGE_SIZE.
 */
RTR0DECL(void *) RTMemContAlloc(PRTCCPHYS pPhys, size_t cb)
{
    int             cOrder;
    unsigned        cPages;
    struct page    *paPages;

    /*
     * validate input.
     */
    Assert(VALID_PTR(pPhys));
    Assert(cb > 0);

    /*
     * Allocate page pointer array.
     */
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    cPages = cb >> PAGE_SHIFT;
    cOrder = CalcPowerOf2Order(cPages);
#ifdef __AMD64__ /** @todo check out if there is a correct way of getting memory below 4GB (physically). */
    paPages = alloc_pages(GFP_DMA, cOrder);
#else
    paPages = alloc_pages(GFP_USER, cOrder);
#endif
    if (paPages)
    {
        /*
         * Reserve the pages and mark them executable.
         */
        unsigned iPage;
        for (iPage = 0; iPage < cPages; iPage++)
        {
            Assert(!PageHighMem(&paPages[iPage]));
            if (iPage + 1 < cPages)
            {
                AssertMsg(          (uintptr_t)phys_to_virt(page_to_phys(&paPages[iPage])) + PAGE_SIZE
                                ==  (uintptr_t)phys_to_virt(page_to_phys(&paPages[iPage + 1]))
                          &&        page_to_phys(&paPages[iPage]) + PAGE_SIZE
                                ==  page_to_phys(&paPages[iPage + 1]),
                          ("iPage=%i cPages=%u [0]=%#llx,%p [1]=%#llx,%p\n", iPage, cPages,
                           (long long)page_to_phys(&paPages[iPage]),     phys_to_virt(page_to_phys(&paPages[iPage])),
                           (long long)page_to_phys(&paPages[iPage + 1]), phys_to_virt(page_to_phys(&paPages[iPage + 1])) ));
            }

            SetPageReserved(&paPages[iPage]);
            if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
                MY_CHANGE_PAGE_ATTR(&paPages[iPage], 1, MY_PAGE_KERNEL_EXEC);
        }
        *pPhys = page_to_phys(paPages);
        return phys_to_virt(page_to_phys(paPages));
    }

    return NULL;
}


/**
 * Frees memory allocated ysing RTMemContAlloc().
 *
 * @param   pv      Pointer to return from RTMemContAlloc().
 * @param   cb      The cb parameter passed to RTMemContAlloc().
 */
RTR0DECL(void) RTMemContFree(void *pv, size_t cb)
{
    if (pv)
    {
        int             cOrder;
        unsigned        cPages;
        unsigned        iPage;
        struct page    *paPages;

        /* validate */
        AssertMsg(!((uintptr_t)pv & PAGE_OFFSET_MASK), ("pv=%p\n", pv));
        Assert(cb > 0);

        /* calc order and get pages */
        cb = RT_ALIGN_Z(cb, PAGE_SIZE);
        cPages = cb >> PAGE_SHIFT;
        cOrder = CalcPowerOf2Order(cPages);
        paPages = virt_to_page(pv);

        /*
         * Restore page attributes freeing the pages.
         */
        for (iPage = 0; iPage < cPages; iPage++)
        {
            ClearPageReserved(&paPages[iPage]);
            if (pgprot_val(MY_PAGE_KERNEL_EXEC) != pgprot_val(PAGE_KERNEL))
                MY_CHANGE_PAGE_ATTR(&paPages[iPage], 1, PAGE_KERNEL);
        }
        __free_pages(paPages, cOrder);
    }
}

