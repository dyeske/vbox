/** @file
 *
 * AutoWriteLock/AutoReadLock: smart R/W semaphore wrappers
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_AUTOLOCK
#define ____H_AUTOLOCK

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/critsect.h>
#include <iprt/thread.h>
#include <iprt/semaphore.h>

#include <iprt/err.h>
#include <iprt/assert.h>

#if defined(DEBUG)
# include <iprt/asm.h> // for ASMReturnAddress
#endif

#include <iprt/semaphore.h>

namespace util
{

////////////////////////////////////////////////////////////////////////////////
//
// LockHandle and friends
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Abstract lock operations. See LockHandle and AutoWriteLock for details.
 */
class LockOps
{
public:

    virtual ~LockOps() {}

    virtual void lock() = 0;
    virtual void unlock() = 0;
};

/**
 * Read lock operations. See LockHandle and AutoWriteLock for details.
 */
class ReadLockOps : public LockOps
{
public:

    /**
     * Requests a read (shared) lock.
     */
    virtual void lockRead() = 0;

    /**
     * Releases a read (shared) lock ackquired by lockRead().
     */
    virtual void unlockRead() = 0;

    // LockOps interface
    void lock() { lockRead(); }
    void unlock() { unlockRead(); }
};

/**
 * Write lock operations. See LockHandle and AutoWriteLock for details.
 */
class WriteLockOps : public LockOps
{
public:

    /**
     * Requests a write (exclusive) lock.
     */
    virtual void lockWrite() = 0;

    /**
     * Releases a write (exclusive) lock ackquired by lockWrite().
     */
    virtual void unlockWrite() = 0;

    // LockOps interface
    void lock() { lockWrite(); }
    void unlock() { unlockWrite(); }
};

/**
 * Abstract read/write semaphore handle.
 *
 * This is a base class to implement semaphores that provide read/write locking.
 * Subclasses must implement all pure virtual methods of this class together
 * with pure methods of ReadLockOps and WriteLockOps classes.
 *
 * See the AutoWriteLock class documentation for the detailed description of
 * read and write locks.
 */
class LockHandle : protected ReadLockOps, protected WriteLockOps
{
public:

    LockHandle() {}
    virtual ~LockHandle() {}

    /**
     * Returns @c true if the current thread holds a write lock on this
     * read/write semaphore. Intended for debugging only.
     */
    virtual bool isWriteLockOnCurrentThread() const = 0;

    /**
     * Returns the current write lock level of this semaphore. The lock level
     * determines the number of nested #lock() calls on the given semaphore
     * handle.
     *
     * Note that this call is valid only when the current thread owns a write
     * lock on the given semaphore handle and will assert otherwise.
     */
    virtual uint32_t writeLockLevel() const = 0;

    /**
     * Returns an interface to read lock operations of this semaphore.
     * Used by constructors of AutoMultiLockN classes.
     */
    LockOps *rlock() { return (ReadLockOps *) this; }

    /**
     * Returns an interface to write lock operations of this semaphore.
     * Used by constructors of AutoMultiLockN classes.
     */
    LockOps *wlock() { return (WriteLockOps *) this; }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (LockHandle)

    friend class AutoWriteLock;
    friend class AutoReadLock;
};

/**
 * Full-featured read/write semaphore handle implementation.
 *
 * This is an auxiliary base class for classes that need full-featured
 * read/write locking as described in the AutoWriteLock class documentation.
 * Instances of classes inherited from this class can be passed as arguments to
 * the AutoWriteLock and AutoReadLock constructors.
 */
class RWLockHandle : public LockHandle
{
public:

    RWLockHandle();
    virtual ~RWLockHandle();

    bool isWriteLockOnCurrentThread() const;

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (RWLockHandle)

    void lockWrite();
    void unlockWrite();
    void lockRead();
    void unlockRead();

    uint32_t writeLockLevel() const;

    RTSEMRW mSemRW;
};

/**
 * Write-only semaphore handle implementation.
 *
 * This is an auxiliary base class for classes that need write-only (exclusive)
 * locking and do not need read (shared) locking. This implementation uses a
 * cheap and fast critical section for both lockWrite() and lockRead() methods
 * which makes a lockRead() call fully equivalent to the lockWrite() call and
 * therefore makes it pointless to use instahces of this class with
 * AutoReadLock instances -- shared locking will not be possible anyway and
 * any call to lock() will block if there are lock owners on other threads.
 *
 * Use with care only when absolutely sure that shared locks are not necessary.
 */
class WriteLockHandle : public LockHandle
{
public:

    WriteLockHandle()
    {
        RTCritSectInit (&mCritSect);
    }

    virtual ~WriteLockHandle()
    {
        RTCritSectDelete (&mCritSect);
    }

    bool isWriteLockOnCurrentThread() const
    {
        return RTCritSectIsOwner (&mCritSect);
    }

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (WriteLockHandle)

    void lockWrite()
    {
#if defined(DEBUG)
        RTCritSectEnterDebug (&mCritSect,
                              "WriteLockHandle::lockWrite() return address >>>",
                              0, (RTUINTPTR) ASMReturnAddress());
#else
        RTCritSectEnter (&mCritSect);
#endif
    }

    void unlockWrite()
    {
        RTCritSectLeave (&mCritSect);
    }

    void lockRead() { lockWrite(); }
    void unlockRead() { unlockWrite(); }

    uint32_t writeLockLevel() const
    {
        return RTCritSectGetRecursion (&mCritSect);
    }

    mutable RTCRITSECT mCritSect;
};

////////////////////////////////////////////////////////////////////////////////
//
// Lockable
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Lockable interface.
 *
 * This is an abstract base for classes that need read/write locking. Unlike
 * RWLockHandle and other classes that makes the read/write semaphore a part of
 * class data, this class allows subclasses to decide which semaphore handle to
 * use.
 */
class Lockable
{
public:

    /**
     * Returns a pointer to a LockHandle used by AutoWriteLock/AutoReadLock
     * for locking. Subclasses are allowed to return @c NULL -- in this case,
     * the AutoWriteLock/AutoReadLock object constructed using an instance of
     * such subclass will simply turn into no-op.
     */
    virtual LockHandle *lockHandle() const = 0;

    /**
     * Equivalent to <tt>#lockHandle()->isWriteLockOnCurrentThread()</tt>.
     * Returns @c false if lockHandle() returns @c NULL.
     */
    bool isWriteLockOnCurrentThread()
    {
        LockHandle *h = lockHandle();
        return h ? h->isWriteLockOnCurrentThread() : false;
    }

    /**
     * Equivalent to <tt>#lockHandle()->rlock()</tt>.
     * Returns @c NULL false if lockHandle() returns @c NULL.
     */
    LockOps *rlock()
    {
        LockHandle *h = lockHandle();
        return h ? h->rlock() : NULL;
    }

    /**
     * Equivalent to <tt>#lockHandle()->wlock()</tt>. Returns @c NULL false if
     * lockHandle() returns @c NULL.
     */
    LockOps *wlock()
    {
        LockHandle *h = lockHandle();
        return h ? h->wlock() : NULL;
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoLockBase
//
////////////////////////////////////////////////////////////////////////////////

class AutoLockBase
{
protected:
    AutoLockBase(LockHandle *pHandle);
    virtual ~AutoLockBase();

    struct Data;
    Data *m;

    virtual void callLockImpl(LockHandle &l) = 0;
    virtual void callUnlockImpl(LockHandle &l) = 0;

    void callLockOnAllHandles();
    void callUnlockOnAllHandles();

    void cleanup();

public:
    void acquire();
    void release();

private:
    // prohibit copy + assignment
    AutoLockBase(const AutoLockBase&);
    AutoLockBase& operator=(AutoLockBase&);
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoWriteLock
//
////////////////////////////////////////////////////////////////////////////////

class AutoWriteLock : public AutoLockBase
{
public:

    /**
     * Constructs a null instance that does not manage any read/write
     * semaphore.
     *
     * Note that all method calls on a null instance are no-ops. This allows to
     * have the code where lock protection can be selected (or omitted) at
     * runtime.
     */
    AutoWriteLock()
        : AutoLockBase(NULL)
    { }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(LockHandle *aHandle)
        : AutoLockBase(aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(LockHandle &aHandle)
        : AutoLockBase(&aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(const Lockable &aLockable)
        : AutoLockBase(aLockable.lockHandle())
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a write lock.
     */
    AutoWriteLock(const Lockable *aLockable)
        : AutoLockBase(aLockable ? aLockable->lockHandle() : NULL)
    {
        acquire();
    }

    /**
     * Release all write locks acquired by this instance through the #lock()
     * call and destroys the instance.
     *
     * Note that if there there are nested #lock() calls without the
     * corresponding number of #unlock() calls when the destructor is called, it
     * will assert. This is because having an unbalanced number of nested locks
     * is a program logic error which must be fixed.
     */
    virtual ~AutoWriteLock()
    {
        cleanup();
    }

    virtual void callLockImpl(LockHandle &l);
    virtual void callUnlockImpl(LockHandle &l);

    void leave();
    void enter();

    /**
     * Same as #leave() but checks if the current thread actally owns the lock
     * and only proceeds in this case. As a result, as opposed to #leave(),
     * doesn't assert when called with no lock being held.
     */
    void maybeLeave()
    {
        if (isWriteLockOnCurrentThread())
            leave();
    }

    /**
     * Same as #enter() but checks if the current thread actally owns the lock
     * and only proceeds if not. As a result, as opposed to #enter(), doesn't
     * assert when called with the lock already being held.
     */
    void maybeEnter()
    {
        if (!isWriteLockOnCurrentThread())
            enter();
    }

    void attach(LockHandle *aHandle);

    /** @see attach (LockHandle *) */
    void attach(LockHandle &aHandle)
    {
        attach(&aHandle);
    }

    /** @see attach (LockHandle *) */
    void attach(const Lockable &aLockable)
    {
        attach(aLockable.lockHandle());
    }

    /** @see attach (LockHandle *) */
    void attach(const Lockable *aLockable)
    {
        attach(aLockable ? aLockable->lockHandle() : NULL);
    }

    void attachRaw(LockHandle *ph);

    bool isWriteLockOnCurrentThread() const;
    uint32_t writeLockLevel() const;
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoReadLock
//
////////////////////////////////////////////////////////////////////////////////

class AutoReadLock : public AutoLockBase
{
public:

    /**
     * Constructs a null instance that does not manage any read/write
     * semaphore.
     *
     * Note that all method calls on a null instance are no-ops. This allows to
     * have the code where lock protection can be selected (or omitted) at
     * runtime.
     */
    AutoReadLock()
        : AutoLockBase(NULL)
    { }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(LockHandle *aHandle)
        : AutoLockBase(aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(LockHandle &aHandle)
        : AutoLockBase(&aHandle)
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(const Lockable &aLockable)
        : AutoLockBase(aLockable.lockHandle())
    {
        acquire();
    }

    /**
     * Constructs a new instance that will start managing the given read/write
     * semaphore by requesting a read lock.
     */
    AutoReadLock(const Lockable *aLockable)
        : AutoLockBase(aLockable ? aLockable->lockHandle() : NULL)
    {
        acquire();
    }

    virtual ~AutoReadLock();

    virtual void callLockImpl(LockHandle &l);
    virtual void callUnlockImpl(LockHandle &l);
};

////////////////////////////////////////////////////////////////////////////////
//
// AutoMulti*
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Helper template class for AutoMultiLockN classes.
 *
 * @param Cnt number of read/write semaphores to manage.
 */
template <size_t Cnt>
class AutoMultiLockBase
{
public:

    /**
     * Releases all locks if not yet released by #unlock() and destroys the
     * instance.
     */
    ~AutoMultiLockBase()
    {
        if (mIsLocked)
            unlock();
    }

    /**
     * Calls LockOps::lock() methods of all managed semaphore handles
     * in order they were passed to the constructor.
     *
     * Note that as opposed to LockHandle::lock(), this call cannot be nested
     * and will assert if so.
     */
    void lock()
    {
        AssertReturnVoid (!mIsLocked);

        size_t i = 0;
        while (i < RT_ELEMENTS (mOps))
            if (mOps [i])
                mOps [i ++]->lock();
        mIsLocked = true;
    }

    /**
     * Calls LockOps::unlock() methods of all managed semaphore handles in
     * reverse to the order they were passed to the constructor.
     *
     * Note that as opposed to LockHandle::unlock(), this call cannot be nested
     * and will assert if so.
     */
    void unlock()
    {
        AssertReturnVoid (mIsLocked);

        AssertReturnVoid (RT_ELEMENTS (mOps) > 0);
        size_t i = RT_ELEMENTS (mOps);
        do
            if (mOps [-- i])
                mOps [i]->unlock();
        while (i != 0);
        mIsLocked = false;
    }

protected:

    AutoMultiLockBase() : mIsLocked (false) {}

    LockOps *mOps [Cnt];
    bool mIsLocked;

private:

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoMultiLockBase)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoMultiLockBase)
};

/** AutoMultiLockBase <0> is meaningless and forbidden. */
template<>
class AutoMultiLockBase <0> { private : AutoMultiLockBase(); };

/** AutoMultiLockBase <1> is meaningless and forbidden. */
template<>
class AutoMultiLockBase <1> { private : AutoMultiLockBase(); };

////////////////////////////////////////////////////////////////////////////////

/* AutoMultiLockN class definitions */

#define A(n) LockOps *l##n
#define B(n) mOps [n] = l##n

/**
 * AutoMultiLock for 2 locks.
 *
 * The AutoMultiLockN family of classes provides a possibility to manage several
 * read/write semaphores at once. This is handy if all managed semaphores need
 * to be locked and unlocked synchronously and will also help to avoid locking
 * order errors.
 *
 * Instances of AutoMultiLockN classes are constructed from a list of LockOps
 * arguments. The AutoMultiLockBase::lock() method will make sure that the given
 * list of semaphores represented by LockOps pointers will be locked in order
 * they are passed to the constructor. The AutoMultiLockBase::unlock() method
 * will make sure that they will be unlocked in reverse order.
 *
 * The type of the lock to request is specified for each semaphore individually
 * using the corresponding LockOps getter of a LockHandle or Lockable object:
 * LockHandle::wlock() in order to request a write lock or LockHandle::rlock()
 * in order to request a read lock.
 *
 * Here is a typical usage pattern:
 * <code>
 *  ...
 *  LockHandle data1, data2;
 *  ...
 *  {
 *      AutoMultiLock2 multiLock (data1.wlock(), data2.rlock());
 *      // both locks are held here:
 *      // - data1 is locked in write mode (like AutoWriteLock)
 *      // - data2 is locked in read mode (like AutoReadLock)
 *  }
 *  // both locks are released here
 * </code>
 */
class AutoMultiLock2 : public AutoMultiLockBase <2>
{
public:
    AutoMultiLock2 (A(0), A(1))
    { B(0); B(1); lock(); }
};

/** AutoMultiLock for 3 locks. See AutoMultiLock2 for more information. */
class AutoMultiLock3 : public AutoMultiLockBase <3>
{
public:
    AutoMultiLock3 (A(0), A(1), A(2))
    { B(0); B(1); B(2); lock(); }
};

/** AutoMultiLock for 4 locks. See AutoMultiLock2 for more information. */
class AutoMultiLock4 : public AutoMultiLockBase <4>
{
public:
    AutoMultiLock4 (A(0), A(1), A(2), A(3))
    { B(0); B(1); B(2); B(3); lock(); }
};

#undef B
#undef A

////////////////////////////////////////////////////////////////////////////////

/**
 * Helper template class for AutoMultiWriteLockN classes.
 *
 * @param Cnt number of write semaphores to manage.
 */
template <size_t Cnt>
class AutoMultiWriteLockBase
{

public:
    /**
     * Calls AutoWriteLock::acquire() methods for all managed semaphore handles in
     * order they were passed to the constructor.
     */
    void acquire()
    {
        size_t i = 0;
        while (i < RT_ELEMENTS(mLocks))
            mLocks[i++].acquire();
    }

    /**
     * Calls AutoWriteLock::unlock() methods for all managed semaphore handles
     * in reverse to the order they were passed to the constructor.
     */
    void release()
    {
        AssertReturnVoid(RT_ELEMENTS(mLocks) > 0);
        size_t i = RT_ELEMENTS(mLocks);
        do
            mLocks[--i].release();
        while (i != 0);
    }

    /**
     * Calls AutoWriteLock::leave() methods for all managed semaphore handles in
     * reverse to the order they were passed to the constructor.
     */
    void leave()
    {
        AssertReturnVoid(RT_ELEMENTS(mLocks) > 0);
        size_t i = RT_ELEMENTS(mLocks);
        do
            mLocks[--i].leave();
        while (i != 0);
    }

    /**
    * Calls AutoWriteLock::maybeLeave() methods for all managed semaphore
    * handles in reverse to the order they were passed to the constructor.
     */
    void maybeLeave()
    {
        AssertReturnVoid(RT_ELEMENTS(mLocks) > 0);
        size_t i = RT_ELEMENTS(mLocks);
        do
            mLocks [-- i].maybeLeave();
        while (i != 0);
    }

    /**
     * Calls AutoWriteLock::maybeEnter() methods for all managed semaphore
     * handles in order they were passed to the constructor.
     */
    void maybeEnter()
    {
        size_t i = 0;
        while (i < RT_ELEMENTS(mLocks))
            mLocks[i++].maybeEnter();
    }

    /**
     * Calls AutoWriteLock::enter() methods for all managed semaphore handles in
     * order they were passed to the constructor.
     */
    void enter()
    {
        size_t i = 0;
        while (i < RT_ELEMENTS(mLocks))
            mLocks[i++].enter();
    }

protected:

    AutoMultiWriteLockBase() {}

    void setLockHandle(size_t aIdx, LockHandle *aHandle)
    {
        mLocks[aIdx].attachRaw(aHandle);
    }

private:

    AutoWriteLock mLocks[Cnt];

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP (AutoMultiWriteLockBase)
    DECLARE_CLS_NEW_DELETE_NOOP (AutoMultiWriteLockBase)
};

/** AutoMultiWriteLockBase <0> is meaningless and forbidden. */
template<>
class AutoMultiWriteLockBase <0> { private : AutoMultiWriteLockBase(); };

/** AutoMultiWriteLockBase <1> is meaningless and forbidden. */
template<>
class AutoMultiWriteLockBase <1> { private : AutoMultiWriteLockBase(); };

////////////////////////////////////////////////////////////////////////////////

/* AutoMultiLockN class definitions */

#define A(n) LockHandle *l##n
#define B(n) setLockHandle (n, l##n)

#define C(n) Lockable *l##n
#define D(n) setLockHandle (n, l##n ? l##n->lockHandle() : NULL)

/**
 * AutoMultiWriteLock for 2 locks.
 *
 * The AutoMultiWriteLockN family of classes provides a possibility to manage
 * several read/write semaphores at once. This is handy if all managed
 * semaphores need to be locked and unlocked synchronously and will also help to
 * avoid locking order errors.
 *
 * The functionality of the AutoMultiWriteLockN class family is similar to the
 * functionality of the AutoMultiLockN class family (see the AutoMultiLock2
 * class for details) with two important differences:
 * <ol>
 *     <li>Instances of AutoMultiWriteLockN classes are constructed from a list
 *     of LockHandle or Lockable arguments directly instead of getting
 *     intermediate LockOps interface pointers.
 *     </li>
 *     <li>All locks are requested in <b>write</b> mode.
 *     </li>
 *     <li>Since all locks are requested in write mode, bulk
 *     AutoMultiWriteLockBase::leave() and AutoMultiWriteLockBase::enter()
 *     operations are also available, that will leave and enter all managed
 *     semaphores at once in the proper order (similarly to
 *     AutoMultiWriteLockBase::lock() and AutoMultiWriteLockBase::unlock()).
 *     </li>
 * </ol>
 *
 * Here is a typical usage pattern:
 * <code>
 *  ...
 *  LockHandle data1, data2;
 *  ...
 *  {
 *      AutoMultiWriteLock2 multiLock (&data1, &data2);
 *      // both locks are held in write mode here
 *  }
 *  // both locks are released here
 * </code>
 */
class AutoMultiWriteLock2 : public AutoMultiWriteLockBase <2>
{
public:
    AutoMultiWriteLock2 (A(0), A(1))
    { B(0); B(1); acquire(); }
    AutoMultiWriteLock2 (C(0), C(1))
    { D(0); D(1); acquire(); }
};

/** AutoMultiWriteLock for 3 locks. See AutoMultiWriteLock2 for more details. */
class AutoMultiWriteLock3 : public AutoMultiWriteLockBase <3>
{
public:
    AutoMultiWriteLock3 (A(0), A(1), A(2))
    { B(0); B(1); B(2); acquire(); }
    AutoMultiWriteLock3 (C(0), C(1), C(2))
    { D(0); D(1); D(2); acquire(); }
};

/** AutoMultiWriteLock for 4 locks. See AutoMultiWriteLock2 for more details. */
class AutoMultiWriteLock4 : public AutoMultiWriteLockBase <4>
{
public:
    AutoMultiWriteLock4 (A(0), A(1), A(2), A(3))
    { B(0); B(1); B(2); B(3); acquire(); }
    AutoMultiWriteLock4 (C(0), C(1), C(2), C(3))
    { D(0); D(1); D(2); D(3); acquire(); }
};

#undef D
#undef C
#undef B
#undef A

} /* namespace util */

#endif // ____H_AUTOLOCK

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
