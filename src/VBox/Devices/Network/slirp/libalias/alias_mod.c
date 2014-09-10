/*-
 * Copyright (c) 2005 Paolo Pisati <piso@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef VBOX
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet/libalias/alias_mod.c,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $");

#ifdef _KERNEL
#include <sys/libkern.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#else
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#ifdef _KERNEL
#include <netinet/libalias/alias_local.h>
#include <netinet/libalias/alias_mod.h>
#else
#include "alias_local.h"
#include "alias_mod.h"
#endif

/* Protocol and userland module handlers chains. */
LIST_HEAD(handler_chain, proto_handler) handler_chain = LIST_HEAD_INITIALIZER(foo);
#else  /* VBOX */
# include <slirp.h>
# include "alias_local.h"
# include "alias_mod.h"
#endif /* VBOX */
#ifdef _KERNEL
struct rwlock   handler_rw;
#endif
SLIST_HEAD(dll_chain, dll) dll_chain = SLIST_HEAD_INITIALIZER(foo);

#ifdef _KERNEL

#define LIBALIAS_RWLOCK_INIT() \
        rw_init(&handler_rw, "Libalias_modules_rwlock")
#define LIBALIAS_RWLOCK_DESTROY()   rw_destroy(&handler_rw)
#define LIBALIAS_WLOCK_ASSERT() \
        rw_assert(&handler_rw, RA_WLOCKED)

static __inline void
LIBALIAS_RLOCK(void)
{
    rw_rlock(&handler_rw);
}

static __inline void
LIBALIAS_RUNLOCK(void)
{
    rw_runlock(&handler_rw);
}

static __inline void
LIBALIAS_WLOCK(void)
{
    rw_wlock(&handler_rw);
}

static __inline void
LIBALIAS_WUNLOCK(void)
{
    rw_wunlock(&handler_rw);
}

static void
_handler_chain_init(void)
{

    if (!rw_initialized(&handler_rw))
        LIBALIAS_RWLOCK_INIT();
}

static void
_handler_chain_destroy(void)
{

    if (rw_initialized(&handler_rw))
        LIBALIAS_RWLOCK_DESTROY();
}

#else /* VBOX */
# define LIBALIAS_WLOCK_ASSERT() ;
# define LIBALIAS_RLOCK() \
    do { \
        int rc = RTCritSectRwEnterShared(&pData->CsRwHandlerChain); \
        AssertRC(rc); \
    } while (0)
# define LIBALIAS_RUNLOCK() \
    do { \
        int rc = RTCritSectRwLeaveShared(&pData->CsRwHandlerChain); \
        AssertRC(rc); \
    } while (0)
# define LIBALIAS_WLOCK() \
    do { \
        int rc = RTCritSectRwEnterExcl(&pData->CsRwHandlerChain); \
        AssertRC(rc); \
    } while (0)
# define LIBALIAS_WUNLOCK() \
    do { \
        int rc = RTCritSectRwLeaveExcl(&pData->CsRwHandlerChain); \
        AssertRC(rc); \
    } while (0)
# define _handler_chain_init() ;
# define _handler_chain_destroy() ;
#endif

void
handler_chain_init(void)
{
    _handler_chain_init();
}

void
handler_chain_destroy(void)
{
    _handler_chain_destroy();
}

static int
#ifdef VBOX
_attach_handler(PNATState pData, struct proto_handler *p)
#else
_attach_handler(struct proto_handler *p)
#endif
{
    struct proto_handler *b = NULL, *handler_chain_tail = NULL;

    LIBALIAS_WLOCK_ASSERT();
    LIST_FOREACH(b, &handler_chain, entries) {
        if ((b->pri == p->pri) &&
            (b->dir == p->dir) &&
            (b->proto == p->proto))
            return (EEXIST); /* Priority conflict. */
        if (b->pri > p->pri) {
            LIST_INSERT_BEFORE(b, p, entries);
            return (0);
        }

        /* If the conditions above do not work, we should keep the last
         * element of the list in order to insert *p right after it. */
        handler_chain_tail = b;
    }
    /* End of list or found right position, inserts here. */
    if (handler_chain_tail)
        LIST_INSERT_AFTER(handler_chain_tail, p, entries);
    else
        LIST_INSERT_HEAD(&handler_chain, p, entries);
    return (0);
}

static int
#ifdef VBOX
_detach_handler(PNATState pData, struct proto_handler *p)
#else
_detach_handler(struct proto_handler *p)
#endif
{
    struct proto_handler *b, *b_tmp;;

    LIBALIAS_WLOCK_ASSERT();
    LIST_FOREACH_SAFE(b, &handler_chain, entries, b_tmp) {
        if (b == p) {
            LIST_REMOVE(b, entries);
            return (0);
        }
    }
    return (ENOENT); /* Handler not found. */
}

int
#ifdef VBOX
LibAliasAttachHandlers(PNATState pData, struct proto_handler *_p)
#else
LibAliasAttachHandlers(struct proto_handler *_p)
#endif
{
    int i, error = -1;

    LIBALIAS_WLOCK();
    for (i=0; 1; i++) {
        if (*((int *)&_p[i]) == EOH)
            break;
#ifdef VBOX
        error = _attach_handler(pData, &_p[i]);
#else
        error = _attach_handler(&_p[i]);
#endif
        if (error != 0)
            break;
    }
    LIBALIAS_WUNLOCK();
    return (error);
}

int
#ifdef VBOX
LibAliasDetachHandlers(PNATState pData, struct proto_handler *_p)
#else
LibAliasDetachHandlers(struct proto_handler *_p)
#endif
{
    int i, error = -1;

    LIBALIAS_WLOCK();
    for (i=0; 1; i++) {
        if (*((int *)&_p[i]) == EOH)
            break;
#ifdef VBOX
        error = _detach_handler(pData, &_p[i]);
#else
        error = _detach_handler(&_p[i]);
#endif
        if (error != 0)
            break;
    }
    LIBALIAS_WUNLOCK();
    return (error);
}

int
#ifdef VBOX
detach_handler(PNATState pData, struct proto_handler *_p)
#else
detach_handler(struct proto_handler *_p)
#endif
{
    int error = -1;

    LIBALIAS_WLOCK();
#ifdef VBOX
    error = _detach_handler(pData, _p);
#else
    error = _detach_handler(_p);
#endif
    LIBALIAS_WUNLOCK();
    return (error);
}

int
find_handler(int8_t dir, int8_t proto, struct libalias *la, struct ip *pip,
         struct alias_data *ad)
{
#ifdef VBOX
    PNATState pData = la->pData;
#endif
    struct proto_handler *p;
    int error = ENOENT;

    LIBALIAS_RLOCK();

    LIST_FOREACH(p, &handler_chain, entries) {
        if ((p->dir & dir) && (p->proto & proto))
            if (p->fingerprint(la, pip, ad) == 0) {
                error = p->protohandler(la, pip, ad);
                break;
            }
    }
    LIBALIAS_RUNLOCK();
    return (error);
}

struct proto_handler *
#ifdef VBOX
first_handler(PNATState pData)
#else
first_handler(void)
#endif
{

    return (LIST_FIRST(&handler_chain));
}

/* Dll manipulation code - this code is not thread safe... */

int
attach_dll(struct dll *p)
{
    struct dll *b;

    SLIST_FOREACH(b, &dll_chain, next) {
        if (!strncmp(b->name, p->name, DLL_LEN))
            return (EEXIST); /* Dll name conflict. */
    }
    SLIST_INSERT_HEAD(&dll_chain, p, next);
    return (0);
}

void *
detach_dll(char *p)
{
    struct dll *b = NULL, *b_tmp;
    void *error = NULL;

    SLIST_FOREACH_SAFE(b, &dll_chain, next, b_tmp)
        if (!strncmp(b->name, p, DLL_LEN)) {
            SLIST_REMOVE(&dll_chain, b, dll, next);
            error = b;
            break;
        }
    return (error);
}

struct dll *
walk_dll_chain(void)
{
    struct dll *t;

    t = SLIST_FIRST(&dll_chain);
    if (t == NULL)
        return (NULL);
    SLIST_REMOVE_HEAD(&dll_chain, next);
    return (t);
}
