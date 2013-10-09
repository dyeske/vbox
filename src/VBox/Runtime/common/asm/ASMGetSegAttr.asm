; $Id$
;; @file
; IPRT - ASMGetSegAttr().
;

;
; Copyright (C) 2013 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;
; The contents of this file may alternatively be used under the terms
; of the Common Development and Distribution License Version 1.0
; (CDDL) only, as it comes in the "COPYING.CDDL" file of the
; VirtualBox OSE distribution, in which case the provisions of the
; CDDL are applicable instead of those of the GPL.
;
; You may elect to license modified versions of this file under the
; terms and conditions of either the GPL or the CDDL or both.
;

;*******************************************************************************
;* Header Files                                                                *
;*******************************************************************************
%include "iprt/asmdefs.mac"

BEGINCODE

;;
; Get the segment attributes for a selector.
;
; @param   uSel     x86: [ebp + 08h]  msc: ecx  gcc: edi   The selector value.
; @returns Segment attributes on success or ~0U on failure.
;
; @remarks Using ~0U for failure is chosen because valid access rights always
;          have bits 0:7 as 0 (on both Intel & AMD).
;
BEGINPROC_EXPORTED ASMGetSegAttr
%ifdef ASM_CALL64_MSC
    mov         r10, rdi
    mov         rdi, rcx
%elifdef RT_ARCH_X86
    push        ebp
    mov         ebp, esp
    push        edi
    mov         edi, [ebp + 08h]
%endif

    larl    eax, edi
    jz      done
    mov     eax, 0ffffffffh
done:

%ifdef ASM_CALL64_MSC
    mov     rdi, r10
%elifdef RT_ARCH_X86
    pop     edi
    leave
%endif
    ret
ENDPROC ASMGetSegAttr

