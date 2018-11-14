/**
 * \file vtxfmt.h
 * 
 * \author Keith Whitwell <keithw@vmware.com>
 * \author Gareth Hughes
 */

/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2004  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#ifndef _VTXFMT_H_
#define _VTXFMT_H_

#include "mtypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void _mesa_install_exec_vtxfmt( struct gl_context *ctx, const GLvertexformat *vfmt );
extern void _mesa_install_save_vtxfmt( struct gl_context *ctx, const GLvertexformat *vfmt );
extern void _mesa_initialize_vbo_vtxfmt(struct gl_context *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* _VTXFMT_H_ */
