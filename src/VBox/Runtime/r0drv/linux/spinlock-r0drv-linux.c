/* $Id$ */
/** @file
 * IPRT - Spinlocks, Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"
#include "internal/iprt.h"
#include <iprt/spinlock.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/thread.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the spinlock_t structure.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The linux spinlock structure. */
    spinlock_t          Spinlock;
#if !defined(CONFIG_SMP) || defined(RT_ARCH_AMD64)
    /** A placeholder on Uni systems so we won't piss off RTMemAlloc(). */
    void                *pvUniDummy;
#endif
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;



RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock)
{
    /*
     * Allocate.
     */
    PRTSPINLOCKINTERNAL pSpinlockInt;
    Assert(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    pSpinlockInt = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pSpinlockInt));
    if (!pSpinlockInt)
        return VERR_NO_MEMORY;
    /*
     * Initialize and return.
     */
    pSpinlockInt->u32Magic = RTSPINLOCK_MAGIC;
    spin_lock_init(&pSpinlockInt->Spinlock);

    *pSpinlock = pSpinlockInt;
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSpinlockCreate);


RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock)
{
    /*
     * Validate input.
     */
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    if (!pSpinlockInt)
        return VERR_INVALID_PARAMETER;
    if (pSpinlockInt->u32Magic != RTSPINLOCK_MAGIC)
    {
        AssertMsgFailed(("Invalid spinlock %p magic=%#x\n", pSpinlockInt, pSpinlockInt->u32Magic));
        return VERR_INVALID_PARAMETER;
    }

    ASMAtomicIncU32(&pSpinlockInt->u32Magic);
    RTMemFree(pSpinlockInt);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSpinlockDestroy);


RTDECL(void) RTSpinlockAcquireNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt);

    spin_lock_irqsave(&pSpinlockInt->Spinlock, pTmp->flFlags);
}
RT_EXPORT_SYMBOL(RTSpinlockAcquireNoInts);


RTDECL(void) RTSpinlockReleaseNoInts(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt);

    spin_unlock_irqrestore(&pSpinlockInt->Spinlock, pTmp->flFlags);
}
RT_EXPORT_SYMBOL(RTSpinlockReleaseNoInts);


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
    RT_ASSERT_PREEMPT_CPUID_VAR();
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt); NOREF(pTmp);

    spin_lock(&pSpinlockInt->Spinlock);

#ifdef RT_MORE_STRICT
    {
        RTCPUID const idAssertCpuNow = RTMpCpuId(); /* Spinlocks are not preemptible, so we cannot be rescheduled. */
        AssertMsg(idAssertCpu == idAssertCpuNow || idAssertCpu == NIL_RTCPUID,  ("%#x, %#x\n", idAssertCpu, idAssertCpuNow));
        pTmp->flFlags = idAssertCpuNow;
    }
#endif
}
RT_EXPORT_SYMBOL(RTSpinlockAcquire);


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock, PRTSPINLOCKTMP pTmp)
{
    PRTSPINLOCKINTERNAL pSpinlockInt = (PRTSPINLOCKINTERNAL)Spinlock;
#ifdef RT_MORE_STRICT
    RTCPUID const idAssertCpu = pTmp->flFlags;
    pTmp->flFlags = 0;
    RT_ASSERT_PREEMPT_CPUID();
#endif
    AssertMsg(pSpinlockInt && pSpinlockInt->u32Magic == RTSPINLOCK_MAGIC,
              ("pSpinlockInt=%p u32Magic=%08x\n", pSpinlockInt, pSpinlockInt ? (int)pSpinlockInt->u32Magic : 0));
    NOREF(pSpinlockInt); NOREF(pTmp);

    spin_unlock(&pSpinlockInt->Spinlock);

#ifdef RT_MORE_STRICT
    if (!RTThreadPreemptIsEnabled(NIL_RTTHREAD))
        RT_ASSERT_PREEMPT_CPUID();
#endif
}
RT_EXPORT_SYMBOL(RTSpinlockRelease);

