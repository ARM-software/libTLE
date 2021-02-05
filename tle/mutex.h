/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBTLE_MUTEX_H__
#define __LIBTLE_MUTEX_H__

#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h>
#include <stdalign.h>
#endif
#include "profile.h"
#include "spinlock.h"
#include "rwlock.h"

#if defined(__x86_64__)

#include <immintrin.h>

#define LIBTLE_LOCK_IS_LOCKED   (255)

#define _XBEGIN_RESTART(s) \
    ((s) & (_XABORT_EXPLICIT | _XABORT_RETRY | _XABORT_CONFLICT))

#elif defined(__aarch64__)

#include "tme.h"

#define LIBTLE_LOCK_IS_LOCKED   (65535)

#define _XBEGIN_RESTART(s) \
    ((s) & _XABORT_RETRY)

#else

#error "Only x86-64 and Aarch64 HTM support for now"

#endif


#ifndef LIBTLE_HTM_SPIN_MUTEX_RETRY_LIMIT
#define LIBTLE_HTM_SPIN_MUTEX_RETRY_LIMIT (10)
#endif

#ifndef LIBTLE_HTM_SPIN_SHARED_MUTEX_WRITE_RETRY_LIMIT
#define LIBTLE_HTM_SPIN_SHARED_MUTEX_WRITE_RETRY_LIMIT (10)
#endif

#ifndef LIBTLE_HTM_SPIN_SHARED_MUTEX_READ_RETRY_LIMIT
#define LIBTLE_HTM_SPIN_SHARED_MUTEX_READ_RETRY_LIMIT (10)
#endif


#ifdef __cplusplus
namespace tle{ namespace detail{
#endif


/* -------------------------------------------------------------------------- */
/* Common types                                                               */
/* -------------------------------------------------------------------------- */


enum libtle_mutex_status_t {
    LIBTLE_MUTEX_STATUS_UNKNOWN = 0,
    LIBTLE_MUTEX_STATUS_UNLOCKED = 1,
    LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE = 2,
    LIBTLE_MUTEX_STATUS_LOCKED_SHARED = 3,
    LIBTLE_MUTEX_STATUS_ELIDED = 4
};


/* -------------------------------------------------------------------------- */
/* Null mutex (no locking)                                                    */
/* -------------------------------------------------------------------------- */


typedef struct {
} libtle_null_mutex_t;


#ifndef __cplusplus
#define LIBTLE_NULL_MUTEX_INIT  { }
#endif


typedef struct {
#ifndef NDEBUG
    enum libtle_mutex_status_t status;
#endif
} libtle_null_mutex_handle_t;


static inline void
libtle_null_mutex_handle_init(libtle_null_mutex_handle_t *st)
{
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNKNOWN;
#endif
}


static inline void
libtle_null_mutex_init(libtle_null_mutex_t *mtx)
{ }


static inline void
libtle_null_mutex_lock(libtle_null_mutex_t *mtx,
                       libtle_null_mutex_handle_t *st,
                       libtle_null_mutex_profile_t *p)
{
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE;
#endif
}


static inline void
libtle_null_mutex_unlock(libtle_null_mutex_t *mtx,
                         libtle_null_mutex_handle_t *st,
                         libtle_null_mutex_profile_t *p)
{
    assert(st->status == LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#endif
    if (p) {
        libtle_null_mutex_profile_update_unlock(p);
    }
}


/* -------------------------------------------------------------------------- */
/* Spinlock-based mutex                                                       */
/* -------------------------------------------------------------------------- */


typedef struct {
    alignas(64) libtle_spinlock_t state;
} libtle_spin_mutex_t;


#ifndef __cplusplus
#define LIBTLE_SPIN_MUTEX_INIT  { LIBTLE_SPINLOCK_INIT }
#endif


typedef struct {
#ifndef NDEBUG
    enum libtle_mutex_status_t status;
#endif
} libtle_spin_mutex_handle_t;


static inline void
libtle_spin_mutex_handle_init(libtle_spin_mutex_handle_t *st)
{
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNKNOWN;
#endif
}


static inline void
libtle_spin_mutex_init(libtle_spin_mutex_t *mtx)
{
    libtle_spinlock_init(&mtx->state);
}


static inline void
libtle_spin_mutex_lock(libtle_spin_mutex_t *mtx,
                       libtle_spin_mutex_handle_t *st,
                       libtle_mutex_profile_t *p)
{
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
    libtle_spinlock_lock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE;
#endif
}


static inline void
libtle_spin_mutex_unlock(libtle_spin_mutex_t *mtx,
                         libtle_spin_mutex_handle_t *st,
                         libtle_mutex_profile_t *p)
{
    assert(st->status == LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE);
    libtle_spinlock_unlock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#endif
    if (p) {
        libtle_mutex_profile_update_unlock(p);
    }
}


/* -------------------------------------------------------------------------- */
/* HTM-based mutex with a spinlock as fallback                                */
/* -------------------------------------------------------------------------- */

typedef struct {
    alignas(64) libtle_spinlock_t state;
} libtle_htm_spin_mutex_t;


#ifndef __cplusplus
#define LIBTLE_HTM_SPIN_MUTEX_INIT  { LIBTLE_SPINLOCK_INIT }
#endif


typedef struct {
#ifndef NDEBUG
    enum libtle_mutex_status_t status;
#endif
} libtle_htm_spin_mutex_handle_t;


static inline void
libtle_htm_spin_mutex_handle_init(libtle_htm_spin_mutex_handle_t *st)
{
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNKNOWN;
#endif
}


static inline void
libtle_htm_spin_mutex_init(libtle_htm_spin_mutex_t *mtx)
{
    libtle_spinlock_init(&mtx->state);
}


static inline void
libtle_htm_spin_mutex_lock(libtle_htm_spin_mutex_t *mtx,
                           libtle_htm_spin_mutex_handle_t *st,
                           libtle_htm_mutex_profile_t *p)
{
    int num_retries = 0;
    unsigned xstatus;
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
    do {
        libtle_spinlock_unlock_wait(&mtx->state);
        xstatus = _xbegin();
        if (__builtin_expect(xstatus == _XBEGIN_STARTED, 1)) {
            // add the lock to our read-set
            if (libtle_spinlock_is_locked(&mtx->state)) {
                _xabort(LIBTLE_LOCK_IS_LOCKED);
                __builtin_unreachable();
            }
#ifndef NDEBUG
            st->status = LIBTLE_MUTEX_STATUS_ELIDED;
#endif
            return;
        }
        ++num_retries;
        if (p) {
            libtle_htm_mutex_profile_update_abort(p, xstatus);
        }
    }
    while (_XBEGIN_RESTART(xstatus) &&
           num_retries < LIBTLE_HTM_SPIN_MUTEX_RETRY_LIMIT);

    // we failed too many times; grab the lock!
    libtle_spinlock_lock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE;
#endif
}


static inline void
libtle_htm_spin_mutex_unlock(libtle_htm_spin_mutex_t *mtx,
                             libtle_htm_spin_mutex_handle_t *st,
                             libtle_htm_mutex_profile_t *p)
{
#ifndef NDEBUG
    switch (st->status) {
    case LIBTLE_MUTEX_STATUS_ELIDED:
        _xend();
        if (p && !_xtest()) {
            libtle_htm_mutex_profile_update_commit(p);
        }
        break;
    case LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE:
        libtle_spinlock_unlock(&mtx->state);
        if (p) {
            libtle_htm_mutex_profile_update_unlock(p);
        }
        break;
    default:
        assert(0);
    }
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#else
    if (libtle_spinlock_is_locked(&mtx->state)) {
        libtle_spinlock_unlock(&mtx->state);
        if (p) {
            libtle_htm_mutex_profile_update_unlock(p);
        }
    } else {
        _xend();
        if (p && !_xtest()) {
            libtle_htm_mutex_profile_update_commit(p);
        }
    }
#endif
}


/* -------------------------------------------------------------------------- */
/* Null reader/writer mutex (no locking)                                      */
/* -------------------------------------------------------------------------- */


typedef struct {
} libtle_null_shared_mutex_t;


#ifndef __cplusplus
#define LIBTLE_NULL_SHARED_MUTEX_INIT  { }
#endif


typedef struct {
#ifndef NDEBUG
    enum libtle_mutex_status_t status;
#endif
} libtle_null_shared_mutex_handle_t;


static inline void
libtle_null_shared_mutex_handle_init(libtle_null_shared_mutex_handle_t *st)
{
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNKNOWN;
#endif
}


static inline void
libtle_null_shared_mutex_init(libtle_null_shared_mutex_t *mtx)
{ }


static inline void
libtle_null_shared_mutex_lock(libtle_null_shared_mutex_t *mtx,
                              libtle_null_shared_mutex_handle_t *st,
                              libtle_null_mutex_profile_t *p)
{
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE;
#endif
}


static inline void
libtle_null_shared_mutex_lock_shared(libtle_null_shared_mutex_t *mtx,
                                     libtle_null_shared_mutex_handle_t *st,
                                     libtle_null_mutex_profile_t *p)
{
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_SHARED;
#endif
}


static inline void
libtle_null_shared_mutex_unlock(libtle_null_shared_mutex_t *mtx,
                                libtle_null_shared_mutex_handle_t *st,
                                libtle_null_mutex_profile_t *p)
{
    assert(st->status == LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#endif
    if (p) {
        libtle_null_mutex_profile_update_unlock(p);
    }
}


static inline void
libtle_null_shared_mutex_unlock_shared(libtle_null_shared_mutex_t *mtx,
                                       libtle_null_shared_mutex_handle_t *st,
                                       libtle_null_mutex_profile_t *p)
{
    assert(st->status == LIBTLE_MUTEX_STATUS_LOCKED_SHARED);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#endif
    if (p) {
        libtle_null_mutex_profile_update_unlock(p);
    }
}


/* -------------------------------------------------------------------------- */
/* Rwlock based reader/writer mutex                                           */
/* -------------------------------------------------------------------------- */


typedef struct {
    alignas(64) libtle_rwlock_t state;
} libtle_spin_shared_mutex_t;


#ifndef __cplusplus
#define LIBTLE_SPIN_SHARED_MUTEX_INIT  { LIBTLE_RWLOCK_INIT }
#endif


typedef struct {
#ifndef NDEBUG
    enum libtle_mutex_status_t status;
#endif
} libtle_spin_shared_mutex_handle_t;


static inline void
libtle_spin_shared_mutex_handle_init(libtle_spin_shared_mutex_handle_t *st)
{
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNKNOWN;
#endif
}


static inline void
libtle_spin_shared_mutex_init(libtle_spin_shared_mutex_t *mtx)
{
    libtle_rwlock_init(&mtx->state);
}


static inline void
libtle_spin_shared_mutex_lock(libtle_spin_shared_mutex_t *mtx,
                              libtle_spin_shared_mutex_handle_t *st,
                              libtle_mutex_profile_t *p)
{
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
    libtle_rwlock_write_lock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE;
#endif
}


static inline void
libtle_spin_shared_mutex_lock_shared(libtle_spin_shared_mutex_t *mtx,
                                     libtle_spin_shared_mutex_handle_t *st,
                                     libtle_mutex_profile_t *p)
{
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
    libtle_rwlock_read_lock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_SHARED;
#endif
}


static inline void
libtle_spin_shared_mutex_unlock(libtle_spin_shared_mutex_t *mtx,
                                libtle_spin_shared_mutex_handle_t *st,
                                libtle_mutex_profile_t *p)
{
    assert(st->status == LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE);
    libtle_rwlock_write_unlock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#endif
    if (p) {
        libtle_mutex_profile_update_unlock(p);
    }
}


static inline void
libtle_spin_shared_mutex_unlock_shared(libtle_spin_shared_mutex_t *mtx,
                                       libtle_spin_shared_mutex_handle_t *st,
                                       libtle_mutex_profile_t *p)
{
    assert(st->status == LIBTLE_MUTEX_STATUS_LOCKED_SHARED);
    libtle_rwlock_read_unlock(&mtx->state);
#ifndef NDEBUG
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
#endif
    if (p) {
        libtle_mutex_profile_update_unlock(p);
    }
}


/* -------------------------------------------------------------------------- */
/* HTM-based reader/writer mutex with rwlock as fallback                      */
/* -------------------------------------------------------------------------- */


typedef struct {
    alignas(64) libtle_rwlock_t     state;
    alignas(64) libtle_spinlock_t   wflag;
} libtle_htm_spin_shared_mutex_t;


#ifndef __cplusplus
#define LIBTLE_HTM_SPIN_SHARED_MUTEX_INIT  { LIBTLE_RWLOCK_INIT, LIBTLE_SPINLOCK_INIT }
#endif


typedef struct {
    enum libtle_mutex_status_t status;
} libtle_htm_spin_shared_mutex_handle_t;


static inline void
libtle_htm_spin_shared_mutex_handle_init(libtle_htm_spin_shared_mutex_handle_t *st)
{
    st->status = LIBTLE_MUTEX_STATUS_UNKNOWN;
}


static inline void
libtle_htm_spin_shared_mutex_init(libtle_htm_spin_shared_mutex_t *mtx)
{
    libtle_rwlock_init(&mtx->state);
    libtle_spinlock_init(&mtx->wflag);
}


static inline void
libtle_htm_spin_shared_mutex_lock(libtle_htm_spin_shared_mutex_t *mtx,
                                  libtle_htm_spin_shared_mutex_handle_t *st,
                                  libtle_htm_mutex_profile_t *p)
{
    int num_retries = 0;
    unsigned xstatus;
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
    do {
        libtle_rwlock_unlock_wait(&mtx->state);
        xstatus = _xbegin();
        if (__builtin_expect(xstatus == _XBEGIN_STARTED, 1)) {
            /* add the lock to our read-set */
            if (libtle_rwlock_is_locked(&mtx->state)) {
                _xabort(LIBTLE_LOCK_IS_LOCKED);
                __builtin_unreachable();
            }
            st->status = LIBTLE_MUTEX_STATUS_ELIDED;
            return;
        }
        ++num_retries;
        if (p) {
            libtle_htm_mutex_profile_update_abort(p, xstatus);
        }
    }
    while (_XBEGIN_RESTART(xstatus) &&
           num_retries < LIBTLE_HTM_SPIN_SHARED_MUTEX_WRITE_RETRY_LIMIT);

    /* we failed too many times; grab the lock! */
    libtle_rwlock_write_lock(&mtx->state);
    libtle_spinlock_lock_uncontended(&mtx->wflag);
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE;
}


static inline void
libtle_htm_spin_shared_mutex_lock_shared(libtle_htm_spin_shared_mutex_t *mtx,
                                         libtle_htm_spin_shared_mutex_handle_t *st,
                                         libtle_htm_mutex_profile_t *p)
{
    int num_retries = 0;
    unsigned xstatus;
    assert(st->status <= LIBTLE_MUTEX_STATUS_UNLOCKED);
    do {
        libtle_spinlock_unlock_wait(&mtx->wflag);
        xstatus = _xbegin();
        if (__builtin_expect(xstatus == _XBEGIN_STARTED, 1)) {
            /* add the lock to our read-set */
            if (libtle_spinlock_is_locked(&mtx->wflag)) {
                _xabort(LIBTLE_LOCK_IS_LOCKED);
                __builtin_unreachable();
            }
            st->status = LIBTLE_MUTEX_STATUS_ELIDED;
            return;
        }
        ++num_retries;
        if (p) {
            libtle_htm_mutex_profile_update_abort(p, xstatus);
        }
    }
    while (_XBEGIN_RESTART(xstatus) &&
           num_retries < LIBTLE_HTM_SPIN_SHARED_MUTEX_READ_RETRY_LIMIT);

    /* we failed too many times; grab the lock! */
    libtle_rwlock_read_lock(&mtx->state);
    st->status = LIBTLE_MUTEX_STATUS_LOCKED_SHARED;
}


static inline void
libtle_htm_spin_shared_mutex_unlock(libtle_htm_spin_shared_mutex_t *mtx,
                                    libtle_htm_spin_shared_mutex_handle_t *st,
                                    libtle_htm_mutex_profile_t *p)
{
    switch (st->status) {
    case LIBTLE_MUTEX_STATUS_ELIDED:
        _xend();
        if (p && !_xtest()) {
            libtle_htm_mutex_profile_update_commit(p);
        }
        break;
    case LIBTLE_MUTEX_STATUS_LOCKED_UNIQUE:
        libtle_spinlock_unlock(&mtx->wflag);
        libtle_rwlock_write_unlock(&mtx->state);
        if (p) {
            libtle_htm_mutex_profile_update_unlock(p);
        }
        break;
    default:
        assert(0);
    }
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
}


static inline void
libtle_htm_spin_shared_mutex_unlock_shared(libtle_htm_spin_shared_mutex_t *mtx,
                                           libtle_htm_spin_shared_mutex_handle_t *st,
                                           libtle_htm_mutex_profile_t *p)
{
    switch (st->status) {
    case LIBTLE_MUTEX_STATUS_ELIDED:
        _xend();
        if (p && !_xtest()) {
            libtle_htm_mutex_profile_update_commit(p);
        }
        break;
    case LIBTLE_MUTEX_STATUS_LOCKED_SHARED:
        libtle_rwlock_read_unlock(&mtx->state);
        if (p) {
            libtle_htm_mutex_profile_update_unlock(p);
        }
        break;
    default:
        assert(0);
    }
    st->status = LIBTLE_MUTEX_STATUS_UNLOCKED;
}


/* -------------------------------------------------------------------------- */
/* Generics                                                                   */
/* -------------------------------------------------------------------------- */

#ifndef __cplusplus

#define libtle_mutex_handle_init(M) _Generic((M), \
               libtle_null_mutex_handle_t*: libtle_null_mutex_handle_init, \
               libtle_spin_mutex_handle_t*: libtle_spin_mutex_handle_init, \
           libtle_htm_spin_mutex_handle_t*: libtle_htm_spin_mutex_handle_init, \
        libtle_null_shared_mutex_handle_t*: libtle_null_shared_mutex_handle_init, \
        libtle_spin_shared_mutex_handle_t*: libtle_spin_shared_mutex_handle_init, \
    libtle_htm_spin_shared_mutex_handle_t*: libtle_htm_spin_shared_mutex_handle_init \
)(M)


#define libtle_mutex_init(M) _Generic((M), \
               libtle_null_mutex_t*: libtle_null_mutex_init, \
               libtle_spin_mutex_t*: libtle_spin_mutex_init, \
           libtle_htm_spin_mutex_t*: libtle_htm_spin_mutex_init, \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_init, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_init, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_init \
)(M)


#define libtle_mutex_lock(M,S) _Generic((M), \
               libtle_null_mutex_t*: libtle_null_mutex_lock, \
               libtle_spin_mutex_t*: libtle_spin_mutex_lock, \
           libtle_htm_spin_mutex_t*: libtle_htm_spin_mutex_lock, \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_lock, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_lock, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_lock \
)(M,S,NULL)


#define libtle_mutex_lock_profiled(M,S,P) _Generic((M), \
               libtle_null_mutex_t*: libtle_null_mutex_lock, \
               libtle_spin_mutex_t*: libtle_spin_mutex_lock, \
           libtle_htm_spin_mutex_t*: libtle_htm_spin_mutex_lock, \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_lock, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_lock, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_lock \
)(M,S,P)


#define libtle_mutex_lock_shared(M,S) _Generic((M), \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_lock_shared, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_lock_shared, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_lock_shared \
)(M,S,NULL)


#define libtle_mutex_lock_shared_profiled(M,S,P) _Generic((M), \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_lock_shared, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_lock_shared, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_lock_shared \
)(M,S,P)


#define libtle_mutex_unlock(M,S) _Generic((M), \
               libtle_null_mutex_t*: libtle_null_mutex_unlock, \
               libtle_spin_mutex_t*: libtle_spin_mutex_unlock, \
           libtle_htm_spin_mutex_t*: libtle_htm_spin_mutex_unlock, \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_unlock, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_unlock, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_unlock \
)(M,S,NULL)


#define libtle_mutex_unlock_profiled(M,S,P) _Generic((M), \
               libtle_null_mutex_t*: libtle_null_mutex_unlock, \
               libtle_spin_mutex_t*: libtle_spin_mutex_unlock, \
           libtle_htm_spin_mutex_t*: libtle_htm_spin_mutex_unlock, \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_unlock, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_unlock, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_unlock \
)(M,S,P)


#define libtle_mutex_unlock_shared(M,S) _Generic((M), \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_unlock_shared, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_unlock_shared, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_unlock_shared \
)(M,S,NULL)


#define libtle_mutex_unlock_shared_profiled(M,S,P) _Generic((M), \
        libtle_null_shared_mutex_t*: libtle_null_shared_mutex_unlock_shared, \
        libtle_spin_shared_mutex_t*: libtle_spin_shared_mutex_unlock_shared, \
    libtle_htm_spin_shared_mutex_t*: libtle_htm_spin_shared_mutex_unlock_shared \
)(M,S,P)

#else

// libtle_mutex_handle_init()

static inline void
libtle_mutex_handle_init(libtle_null_mutex_handle_t *h)
{
    libtle_null_mutex_handle_init(h);
}

static inline void
libtle_mutex_handle_init(libtle_spin_mutex_handle_t *h)
{
    libtle_spin_mutex_handle_init(h);
}

static inline void
libtle_mutex_handle_init(libtle_htm_spin_mutex_handle_t *h)
{
    libtle_htm_spin_mutex_handle_init(h);
}

static inline void
libtle_mutex_handle_init(libtle_null_shared_mutex_handle_t *h)
{
    libtle_null_shared_mutex_handle_init(h);
}

static inline void
libtle_mutex_handle_init(libtle_spin_shared_mutex_handle_t *h)
{
    libtle_spin_shared_mutex_handle_init(h);
}

static inline void
libtle_mutex_handle_init(libtle_htm_spin_shared_mutex_handle_t *h)
{
    libtle_htm_spin_shared_mutex_handle_init(h);
}

// libtle_mutex_init()

static inline void
libtle_mutex_init(libtle_null_mutex_t *m)
{
    libtle_null_mutex_init(m);
}

static inline void
libtle_mutex_init(libtle_spin_mutex_t *m)
{
    libtle_spin_mutex_init(m);
}

static inline void
libtle_mutex_init(libtle_htm_spin_mutex_t *m)
{
    libtle_htm_spin_mutex_init(m);
}

static inline void
libtle_mutex_init(libtle_null_shared_mutex_t *m)
{
    libtle_null_shared_mutex_init(m);
}

static inline void
libtle_mutex_init(libtle_spin_shared_mutex_t *m)
{
    libtle_spin_shared_mutex_init(m);
}

static inline void
libtle_mutex_init(libtle_htm_spin_shared_mutex_t *m)
{
    libtle_htm_spin_shared_mutex_init(m);
}

// libtle_mutex_lock()

static inline void
libtle_mutex_lock(libtle_null_mutex_t *m,
                  libtle_null_mutex_handle_t *h,
                  libtle_null_mutex_profile_t *p = nullptr)
{
    libtle_null_mutex_lock(m, h, p);
}

static inline void
libtle_mutex_lock(libtle_spin_mutex_t *m,
                  libtle_spin_mutex_handle_t *h,
                  libtle_mutex_profile_t *p = nullptr)
{
    libtle_spin_mutex_lock(m, h, p);
}

static inline void
libtle_mutex_lock(libtle_htm_spin_mutex_t *m,
                  libtle_htm_spin_mutex_handle_t *h,
                  libtle_htm_mutex_profile_t *p = nullptr)
{
    libtle_htm_spin_mutex_lock(m, h, p);
}

static inline void
libtle_mutex_lock(libtle_null_shared_mutex_t *m,
                  libtle_null_shared_mutex_handle_t *h,
                  libtle_null_mutex_profile_t *p = nullptr)
{
    libtle_null_shared_mutex_lock(m, h, p);
}

static inline void
libtle_mutex_lock(libtle_spin_shared_mutex_t *m,
                  libtle_spin_shared_mutex_handle_t *h,
                  libtle_mutex_profile_t *p = nullptr)
{
    libtle_spin_shared_mutex_lock(m, h, p);
}

static inline void
libtle_mutex_lock(libtle_htm_spin_shared_mutex_t *m,
                  libtle_htm_spin_shared_mutex_handle_t *h,
                  libtle_htm_mutex_profile_t *p = nullptr)
{
    libtle_htm_spin_shared_mutex_lock(m, h, p);
}

// libtle_mutex_lock_shared()

static inline void
libtle_mutex_lock_shared(libtle_null_shared_mutex_t *m,
                         libtle_null_shared_mutex_handle_t *h,
                         libtle_null_mutex_profile_t *p = nullptr)
{
    libtle_null_shared_mutex_lock_shared(m, h, p);
}

static inline void
libtle_mutex_lock_shared(libtle_spin_shared_mutex_t *m,
                         libtle_spin_shared_mutex_handle_t *h,
                         libtle_mutex_profile_t *p = nullptr)
{
    libtle_spin_shared_mutex_lock_shared(m, h, p);
}

static inline void
libtle_mutex_lock_shared(libtle_htm_spin_shared_mutex_t *m,
                         libtle_htm_spin_shared_mutex_handle_t *h,
                         libtle_htm_mutex_profile_t *p = nullptr)
{
    libtle_htm_spin_shared_mutex_lock_shared(m, h, p);
}

// libtle_mutex_unlock()

static inline void
libtle_mutex_unlock(libtle_null_mutex_t *m,
                    libtle_null_mutex_handle_t *h,
                    libtle_null_mutex_profile_t *p = nullptr)
{
    libtle_null_mutex_unlock(m, h, p);
}

static inline void
libtle_mutex_unlock(libtle_spin_mutex_t *m,
                    libtle_spin_mutex_handle_t *h,
                    libtle_mutex_profile_t *p = nullptr)
{
    libtle_spin_mutex_unlock(m, h, p);
}

static inline void
libtle_mutex_unlock(libtle_htm_spin_mutex_t *m,
                    libtle_htm_spin_mutex_handle_t *h,
                    libtle_htm_mutex_profile_t *p = nullptr)
{
    libtle_htm_spin_mutex_unlock(m, h, p);
}

static inline void
libtle_mutex_unlock(libtle_null_shared_mutex_t *m,
                    libtle_null_shared_mutex_handle_t *h,
                    libtle_null_mutex_profile_t *p = nullptr)
{
    libtle_null_shared_mutex_unlock(m, h, p);
}

static inline void
libtle_mutex_unlock(libtle_spin_shared_mutex_t *m,
                    libtle_spin_shared_mutex_handle_t *h,
                    libtle_mutex_profile_t *p = nullptr)
{
    libtle_spin_shared_mutex_unlock(m, h, p);
}

static inline void
libtle_mutex_unlock(libtle_htm_spin_shared_mutex_t *m,
                    libtle_htm_spin_shared_mutex_handle_t *h,
                    libtle_htm_mutex_profile_t *p = nullptr)
{
    libtle_htm_spin_shared_mutex_unlock(m, h, p);
}

// libtle_mutex_unlock_shared()

static inline void
libtle_mutex_unlock_shared(libtle_null_shared_mutex_t *m,
                           libtle_null_shared_mutex_handle_t *h,
                           libtle_null_mutex_profile_t *p = nullptr)
{
    libtle_null_shared_mutex_unlock_shared(m, h, p);
}

static inline void
libtle_mutex_unlock_shared(libtle_spin_shared_mutex_t *m,
                           libtle_spin_shared_mutex_handle_t *h,
                           libtle_mutex_profile_t *p = nullptr)
{
    libtle_spin_shared_mutex_unlock_shared(m, h, p);
}

static inline void
libtle_mutex_unlock_shared(libtle_htm_spin_shared_mutex_t *m,
                           libtle_htm_spin_shared_mutex_handle_t *h,
                           libtle_htm_mutex_profile_t *p = nullptr)
{
    libtle_htm_spin_shared_mutex_unlock_shared(m, h, p);
}

#endif

#ifdef __cplusplus
}} // namespace tle::detail
#endif

#endif /* __LIBTLE_MUTEX_H__ */
