/** @file
 * IPRT - AMD64 and x86 Specific Assembly Functions, 32-bit Watcom C pragma aux.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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

#ifndef ___iprt_asm_amd64_x86_h
# error "Don't include this header directly."
#endif
#ifndef ___iprt_asm_amd64_x86_watcom_32_h
#define ___iprt_asm_amd64_x86_watcom_32_h

#ifndef __FLAT__
# error "Only works with flat pointers! (-mf)"
#endif


#pragma aux ASMGetIDTR = \
    "sidt fword ptr [ecx]" \
    parm [ecx] \
    modify exact [];

#pragma aux ASMGetIdtrLimit = \
    "sub  esp, 8" \
    "sidt fword ptr [esp]" \
    "mov  cx, [esp]" \
    "add  esp, 8" \
    parm [] \
    value [cx] \
    modify exact [ecx];

#pragma aux ASMSetIDTR = \
    "lidt fword ptr [ecx]" \
    parm [ecx] nomemory \
    modify nomemory;

#pragma aux ASMGetGDTR = \
    "sgdt fword ptr [ecx]" \
    parm [ecx] \
    modify exact [];

#pragma aux ASMSetGDTR = \
    "lgdt fword ptr [ecx]" \
    parm [ecx] nomemory \
    modify exact [] nomemory;

#pragma aux ASMGetCS = \
    "mov ax, cs" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetDS = \
    "mov ax, ds" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetES = \
    "mov ax, es" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetFS = \
    "mov ax, fs" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetGS = \
    "mov ax, gs" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetSS = \
    "mov ax, ss" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetTR = \
    "str ax" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetLDTR = \
    "sldt ax" \
    parm [] nomemory \
    value [ax] \
    modify exact [eax] nomemory;

/** @todo ASMGetSegAttr   */

#pragma aux ASMGetFlags = \
    "pushfd" \
    "pop eax" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMSetFlags = \
    "push eax" \
    "popfd" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMChangeFlags = \
    "pushfd" \
    "pop eax" \
    "and edx, eax" \
    "or  edx, ecx" \
    "push edx" \
    "popfd" \
    parm [edx] [ecx] nomemory \
    value [eax] \
    modify exact [edx] nomemory;

#pragma aux ASMAddFlags = \
    "pushfd" \
    "pop eax" \
    "or  edx, eax" \
    "push edx" \
    "popfd" \
    parm [edx] nomemory \
    value [eax] \
    modify exact [edx] nomemory;

#pragma aux ASMClearFlags = \
    "pushfd" \
    "pop eax" \
    "and edx, eax" \
    "push edx" \
    "popfd" \
    parm [edx] nomemory \
    value [eax] \
    modify exact [edx] nomemory;

/* Note! Must use the 64-bit integer return value convension.
         The order of registers in the value [set] does not seem to mean anything. */
#pragma aux ASMReadTSC = \
    ".586" \
    "rdtsc" \
    parm [] nomemory \
    value [eax edx] \
    modify exact [edx eax] nomemory;

#pragma aux ASMReadTscWithAux = \
    0x0f 0x01 0xf9 \
    parm [ecx] \
    value [eax edx] \
    modify exact [eax edx];

/* ASMCpuId: Implemented externally, too many parameters. */
/* ASMCpuId_Idx_ECX: Implemented externally, too many parameters. */
/* ASMCpuIdExSlow: Always implemented externally. */

#pragma aux ASMCpuId_ECX_EDX = \
    "cpuid" \
    "mov [edi], ecx" \
    "mov [esi], edx" \
    parm [ecx] [edi] [esi] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMCpuId_EAX = \
    "cpuid" \
    parm [ecx] \
    value [eax] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMCpuId_EBX = \
    "cpuid" \
    parm [ecx] \
    value [ebx] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMCpuId_ECX = \
    "cpuid" \
    parm [ecx] \
    value [ecx] \
    modify exact [eax ebx ecx edx];

#pragma aux ASMCpuId_EDX = \
    "cpuid" \
    parm [ecx] \
    value [edx] \
    modify exact [eax ebx ecx edx];

/* ASMHasCpuId: MSC inline in main source file. */
/* ASMGetApicId: Implemented externally, lazy bird. */

#pragma aux ASMGetCR0 = \
    "mov eax, cr0" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMSetCR0 = \
    "mov cr0, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMGetCR2 = \
    "mov eax, cr2" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMSetCR2 = \
    "mov cr2, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMGetCR3 = \
    "mov eax, cr3" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMSetCR3 = \
    "mov cr3, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMReloadCR3 = \
    "mov eax, cr3" \
    "mov cr3, eax" \
    parm [] nomemory \
    modify exact [eax] nomemory;

#pragma aux ASMGetCR4 = \
    "mov eax, cr4" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMSetCR4 = \
    "mov cr4, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

/* ASMGetCR8: Don't bother for 32-bit. */
/* ASMSetCR8: Don't bother for 32-bit. */

#pragma aux ASMIntEnable = \
    "sti" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMIntDisable = \
    "cli" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMIntDisableFlags = \
    "pushfd" \
    "cli" \
    "pop eax" \
    parm [] nomemory \
    value [eax] \
    modify exact [] nomemory;

#pragma aux ASMHalt = \
    "hlt" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMRdMsr = \
    ".586" \
    "rdmsr" \
    parm [ecx] nomemory \
    value [eax edx] \
    modify exact [eax edx] nomemory;

#pragma aux ASMWrMsr = \
    ".586" \
    "wrmsr" \
    parm [ecx] [eax edx] nomemory \
    modify exact [] nomemory;

#pragma aux ASMRdMsrEx = \
    ".586" \
    "rdmsr" \
    parm [ecx] [edi] nomemory \
    value [eax edx] \
    modify exact [eax edx] nomemory;

#pragma aux ASMWrMsrEx = \
    ".586" \
    "wrmsr" \
    parm [ecx] [edi] [eax edx] nomemory \
    modify exact [] nomemory;

#pragma aux ASMRdMsr_Low = \
    ".586" \
    "rdmsr" \
    parm [ecx] nomemory \
    value [eax] \
    modify exact [eax edx] nomemory;

#pragma aux ASMRdMsr_High = \
    ".586" \
    "rdmsr" \
    parm [ecx] nomemory \
    value [edx] \
    modify exact [eax edx] nomemory;


#pragma aux ASMGetDR0 = \
    "mov eax, dr0" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetDR1 = \
    "mov eax, dr1" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetDR2 = \
    "mov eax, dr2" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetDR3 = \
    "mov eax, dr3" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetDR6 = \
    "mov eax, dr6" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMGetAndClearDR6 = \
    "mov edx, 0ffff0ff0h" \
    "mov eax, dr6" \
    "mov dr6, edx" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax edx] nomemory;

#pragma aux ASMGetDR7 = \
    "mov eax, dr7" \
    parm [] nomemory \
    value [eax] \
    modify exact [eax] nomemory;

#pragma aux ASMSetDR0 = \
    "mov dr0, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMSetDR1 = \
    "mov dr1, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMSetDR2 = \
    "mov dr2, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMSetDR3 = \
    "mov dr3, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMSetDR6 = \
    "mov dr6, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMSetDR7 = \
    "mov dr7, eax" \
    parm [eax] nomemory \
    modify exact [] nomemory;

/* Yeah, could've used outp here, but this keeps the main file simpler. */
#pragma aux ASMOutU8 = \
    "out dx, al" \
    parm [dx] [al] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInU8 = \
    "in al, dx" \
    parm [dx] nomemory \
    value [al] \
    modify exact [] nomemory;

#pragma aux ASMOutU16 = \
    "out dx, ax" \
    parm [dx] [ax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInU16 = \
    "in ax, dx" \
    parm [dx] nomemory \
    value [ax] \
    modify exact [] nomemory;

#pragma aux ASMOutU32 = \
    "out dx, eax" \
    parm [dx] [eax] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInU32 = \
    "in eax, dx" \
    parm [dx] nomemory \
    value [eax] \
    modify exact [] nomemory;

#pragma aux ASMOutStrU8 = \
    "rep outsb" \
    parm [dx] [esi] [ecx] nomemory \
    modify exact [esi ecx] nomemory;

#pragma aux ASMInStrU8 = \
    "rep insb" \
    parm [dx] [edi] [ecx] \
    modify exact [edi ecx];

#pragma aux ASMOutStrU16 = \
    "rep outsw" \
    parm [dx] [esi] [ecx] nomemory \
    modify exact [esi ecx] nomemory;

#pragma aux ASMInStrU16 = \
    "rep insw" \
    parm [dx] [edi] [ecx] \
    modify exact [edi ecx];

#pragma aux ASMOutStrU32 = \
    "rep outsd" \
    parm [dx] [esi] [ecx] nomemory \
    modify exact [esi ecx] nomemory;

#pragma aux ASMInStrU32 = \
    "rep insd" \
    parm [dx] [edi] [ecx] \
    modify exact [edi ecx];

#pragma aux ASMInvalidatePage = \
    "invlpg [eax]" \
    parm [eax] \
    modify exact [];

#pragma aux ASMWriteBackAndInvalidateCaches = \
    ".486" \
    "wbinvd" \
    parm [] nomemory \
    modify exact [] nomemory;

#pragma aux ASMInvalidateInternalCaches = \
    ".486" \
    "invd" \
    parm [] \
    modify exact [];

#endif

