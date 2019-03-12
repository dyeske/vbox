;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   ReadMm1.Asm
;
; Abstract:
;
;   AsmReadMm1 function
;
; Notes:
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; UINT64
; EFIAPI
; AsmReadMm1 (
;   VOID
;   );
;------------------------------------------------------------------------------
global ASM_PFX(AsmReadMm1)
ASM_PFX(AsmReadMm1):
    ;
    ; 64-bit MASM doesn't support MMX instructions, so use opcode here
    ;
    DB      0x48, 0xf, 0x7e, 0xc8
    ret

