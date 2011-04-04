/** @file
 * IPRT - Generic thread-safe list Class.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___iprt_cpp_mtlist_h
#define ___iprt_cpp_mtlist_h

#include <iprt/cpp/list.h>

#include <iprt/semaphore.h>

namespace iprt
{

/** @addtogroup grp_rt_cpp_list
 * @{
 */

/**
 * A guard class for thread-safe read/write access.
 */
template <>
class ListGuard<true>
{
public:
    ListGuard() { int rc = RTSemRWCreate(&m_hRWSem); AssertRC(rc); }
    ~ListGuard() { RTSemRWDestroy(m_hRWSem); }
    inline void enterRead() const { int rc = RTSemRWRequestRead(m_hRWSem, RT_INDEFINITE_WAIT); AssertRC(rc); }
    inline void leaveRead() const { int rc = RTSemRWReleaseRead(m_hRWSem); AssertRC(rc); }
    inline void enterWrite()      { int rc = RTSemRWRequestWrite(m_hRWSem, RT_INDEFINITE_WAIT); AssertRC(rc); }
    inline void leaveWrite()      { int rc = RTSemRWReleaseWrite(m_hRWSem); AssertRC(rc); }

private:
    mutable RTSEMRW m_hRWSem;
};

/**
 * @brief Generic thread-safe list class.
 *
 * mtlist is a thread-safe implementation of the list class. It uses a
 * read/write semaphore to serialize the access to the items. Several readers
 * can simultaneous access different or the same item. If one thread is writing
 * to an item, the other accessors are blocked until the write has finished.
 *
 * Although the access is guarded, the user has to make sure the list content
 * is consistent when iterating over the list or doing any other kind of access
 * which makes assumptions about the list content. For a finer control of access
 * restrictions, use your own locking mechanism and the standard list
 * implementation.
 *
 * @see ListBase
 */
template <class T, typename ITYPE = typename if_<(sizeof(T) > sizeof(void*)), T*, T>::result>
class mtlist : public ListBase<T, ITYPE, true> {};

/**
 * Specialized thread-safe list class for using the native type list for
 * unsigned 64-bit values even on a 32-bit host.
 *
 * @see ListBase
 */
template <>
class mtlist<uint64_t>: public ListBase<uint64_t, uint64_t, true> {};

/**
 * Specialized thread-safe list class for using the native type list for
 * signed 64-bit values even on a 32-bit host.
 *
 * @see ListBase
 */
template <>
class mtlist<int64_t>: public ListBase<int64_t, uint64_t, true> {};

/** @} */

} /* namespace iprt */

#endif /* !___iprt_cpp_mtlist_h */

