; $Id$
;; @file
; BS3Kit - Bs3SwitchTo16BitV86
;

;
; Copyright (C) 2007-2016 Oracle Corporation
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

%include "bs3kit-template-header.mac"

%if TMPL_BITS != 64

BS3_EXTERN_DATA16   g_bBs3CurrentMode
BS3_EXTERN_CMN      Bs3SwitchToRing0
TMPL_BEGIN_TEXT


;;
; @cproto   BS3_DECL(void) Bs3SwitchTo16BitV86(void);
;
BS3_PROC_BEGIN_CMN Bs3SwitchTo16BitV86
        ; Construct basic v8086 return frame.
        BS3_ONLY_16BIT_STMT push gs
        BS3_ONLY_16BIT_STMT movzx esp, sp
        push    dword fs
        push    dword BS3_SEL_DATA16    ; ES
        push    dword BS3_SEL_DATA16    ; DS
        push    dword ss
        push    dword 0                 ; return ESP, later.
        pushfd
        or      dword [esp], X86_EFL_VM ; Set the VM flag in EFLAGS.
        push    dword BS3_SEL_TEXT16
        push    word 0
 %if TMPL_BITS == 16
        push    word [esp + 2 + 7 * 4 + 2]
 %else
        push    word [esp + 2 + 7 * 4]
 %endif
        ; Save registers and stuff.
        push    eax
 %if TMPL_BITS == 16
        push    ds

        ; Check g_bBs3CurrentMode whether we're in v8086 mode or not.
        mov     ax, seg g_bBs3CurrentMode
        mov     ds, ax
        mov     al, [g_bBs3CurrentMode]
        and     al, BS3_MODE_CODE_MASK
        cmp     al, BS3_MODE_CODE_V86
        jne     .not_v8086

        pop     ds
        pop     eax
        add     xSP, (9-1)*4
        ret

.not_v8086:
        pop     ax                      ; Drop the push ds so the stacks are identical. Keep DS = BS3DATA16 though.
 %endif

        ; Ensure that we're in ring-0.
        mov     ax, ss
        test    ax, 3
        jz      .is_ring0
        call    Bs3SwitchToRing0
.is_ring0:

        ; Update globals.
        and     byte [g_bBs3CurrentMode], ~BS3_MODE_CODE_MASK
        or      byte [g_bBs3CurrentMode], BS3_MODE_CODE_16

 %if TMPL_BITS != 16
        ; Set GS.
        mov     ax, gs
        mov     [xSP + 4 + 20h], ax
 %endif

        ; Thunk SS:ESP, first step: calc flat address.
        lea     eax, [esp + 4 + 24h]
        push    eax
        push    ss
;        call    Bs3SelFar32ToFlat32

        ; Second step: Calc realmode segment and offset.


        ; Return to v8086 mode.
        pop     eax
        iretd
BS3_PROC_END_CMN   Bs3SwitchTo16Bit



%endif ; ! 64-bit

