; $Id$
;; @file
; Compiler support routines.
;

;
; Copyright (C) 2012 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;


;*******************************************************************************
;*  Exported Symbols                                                           *
;*******************************************************************************
public          _small_code_
public          __U4D
public          __I4D
public          __U4M
public          _fmemset_
public          _fmemcpy_



                .386p

_TEXT           segment public 'CODE' use16
                assume cs:_TEXT


;;
; 32-bit unsigned division.
;
; @param    dx:ax   Dividend.
; @param    cx:bx   Divisor.
; @returns  dx:ax   Quotient.
;           cx:bx   Reminder.
;
__U4D:
                pushf
                push    eax
                push    edx
                push    ecx

                rol     eax, 16
                mov     ax, dx
                ror     eax, 16
                xor     edx, edx

                shr     ecx, 16
                mov     cx, bx

                div     ecx                 ; eax:edx / ecx -> eax=quotient, edx=reminder.

                mov     bx, dx
                pop     ecx
                shr     edx, 16
                mov     cx, dx

                pop     edx
                ror     eax, 16
                mov     dx, ax
                add     sp, 2
                pop     ax
                rol     eax, 16

                popf
                ret

;;
; 32-bit signed division.
;
; @param    dx:ax   Dividend.
; @param    cx:bx   Divisor.
; @returns  dx:ax   Quotient.
;           cx:bx   Reminder.
;
__I4D:
                pushf
                push    eax
                push    edx
                push    ecx

                rol     eax, 16
                mov     ax, dx
                ror     eax, 16
                xor     edx, edx

                shr     ecx, 16
                mov     cx, bx

                idiv    ecx                 ; eax:edx / ecx -> eax=quotient, edx=reminder.

                mov     bx, dx
                pop     ecx
                shr     edx, 16
                mov     cx, dx

                pop     edx
                ror     eax, 16
                mov     dx, ax
                add     sp, 2
                pop     ax
                rol     eax, 16

                popf
                ret

;;
; 32-bit unsigned multiplication.
;
; @param    dx:ax   Factor 1.
; @param    cx:bx   Factor 2.
; @returns  dx:ax   Result.
;
__U4M:
                pushf
                push    eax
                push    edx
                push    ecx
                push    ebx

                rol     eax, 16
                mov     ax, dx
                ror     eax, 16
                xor     edx, edx

                shr     ecx, 16
                mov     cx, bx

                mul     ecx                 ; eax * ecx -> edx:eax

                pop     ebx
                pop     ecx

                pop     edx
                ror     eax, 16
                mov     dx, ax
                add     sp, 2
                pop     ax
                rol     eax, 16

                popf
                ret


;;
; memset taking a far pointer.
;
; @returns  dx:ax unchanged.
; @param    dx:ax   Pointer to the memory.
; @param    bl      The fill value.
; @param    cx      The number of bytes to fill.
;
_fmemset_:
                push    es
                push    di
                push    cx
                pushf

                mov     es, dx
                mov     di, ax
                xchg    al, bl
                rep stosb
                xchg    al, bl

                popf
                pop     cx
                pop     di
                pop     es
                ret


;;
; memset taking far pointers.
;
; @returns  dx:ax unchanged.
; @param    dx:ax   Pointer to the destination memory.
; @param    cx:bx   Pointer to the source memory.
; @param    sp+2    The number of bytes to copy (dw).
;
_fmemcpy_:
                push    bp
                mov     bp, sp
                push    es
                push    di
                push    ds
                push    si
                push    cx
                popf

                mov     es, dx
                mov     di, ax
                mov     ds, cx
                mov     si, bx
                mov     cx, [bp + 4]
                rep     movsb

                pushf
                pop     cx
                pop     si
                pop     ds
                pop     di
                pop     es
                leave
                ret


_TEXT           ends
                end

