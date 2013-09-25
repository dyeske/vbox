/*  $NetBSD: vsscanf.c,v 1.14 2005/11/29 03:12:00 christos Exp $  */

/*-
 * Copyright (c) 1990, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at UUNET Technologies, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include  <LibConfig.h>
#include <sys/EfiCdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)vsscanf.c 8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: vsscanf.c,v 1.14 2005/11/29 03:12:00 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "reentrant.h"
#include "local.h"

static int eofread __P((void *, char *, int));

/* ARGSUSED */
static int
eofread(cookie, buf, len)
  void *cookie;
  char *buf;
  int len;
{
  return (0);
}

int
vsscanf(str, fmt, ap)
  const char *str;
  const char *fmt;
  _BSD_VA_LIST_ ap;
{
  FILE f;
  struct __sfileext fext;

  _DIAGASSERT(str != NULL);
  _DIAGASSERT(fmt != NULL);

  _FILEEXT_SETUP(&f, &fext);
  f._flags = __SRD;
  f._bf._base = f._p = __UNCONST(str);
  f._bf._size = f._r = strlen(str);
  f._read = eofread;
  _UB(&f)._base = NULL;
  f._lb._base = NULL;
  return (__svfscanf_unlocked(&f, fmt, ap));
}
