/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBTLE_RWLOCK_H__
#define __LIBTLE_RWLOCK_H__

#ifdef __cplusplus
#include <atomic>

namespace tle{ namespace detail{

using std::atomic_uint;
using std::memory_order_acquire;
using std::memory_order_release;

#else
#include <stdatomic.h>
#endif


/**
 * @brief  A reader-writer spinlock.
 *
 *
 * Bit 0 of %lock signifies an active writer
 * Bit 1 of %lock signifies a pending writer (ie. waiting to acquire
 * the lock)
 * Bits 2:N of %lock hold the number of active readers
 */
typedef struct {
    atomic_uint lock;
} libtle_rwlock_t;


#ifndef __cplusplus
#define LIBTLE_RWLOCK_INIT    { ATOMIC_VAR_INIT(0u) }
#endif


static inline void
libtle_rwlock_init(libtle_rwlock_t *lck)
{
    atomic_init(&lck->lock, 0u);
}


static inline void
libtle_rwlock_write_lock(libtle_rwlock_t *lck)
{
#if defined(__x86_64__)
    unsigned one, tmp;
    __asm__ volatile(
"       mov    $0x1, %2\n"
"    1: mov    %0, %1\n"
"       testl  $0xfffffffd, %1\n"
"       je     3f\n"
"       testb  $0x2, %b1\n"
"       jne    2f\n"
"       lock ; orl $0x2, %0\n"
"    2: pause \n"
"       jmp    1b\n"
"    3: lock ; cmpxchg %2, %0\n"
"       jne    2b\n"
        : "+m" (lck->lock), "=&r" (tmp), "=&r" (one)
        :
        : "memory");
#elif defined(__aarch64__)
    unsigned tmp;
    __asm__ volatile(
"       sevl\n"
"       prfm    pstl1strm, %1\n"
"    1: wfe\n"
"    2: ldaxr   %w0, %1\n"
"       tst     %w0, #0xfffffffd\n"
"       b.eq    3f\n"
"       tbnz    %w0, #1, 1b\n"
"       orr     %w0, %w0, #0x2\n"
"       stlxr   %w0, %w0, %1\n"
"       b       2b\n"
"    3: stlxr   %w0, %w2, %1\n"
"       cbnz    %w0, 2b\n"
        : "=&r" (tmp), "+Q" (lck->lock)
        : "r" (0x1)
        : "memory");
#else
    while (1) {
        unsigned s = atomic_load_explicit(&lck->lock, memory_order_acquire);
        if (!(s & ~2u)) {
            /* the lock is not acquired by any readers or a writer */
            if (atomic_compare_exchange_strong(&lck->lock, &s, 1u)) {
                /* we got the lock! */
                break;
            }
        } else if (!(s & 2u)) {
            /* no other pending writers, mark as pending to block readers */
            atomic_fetch_or(&lck->lock, 2u);
        }
    }
#endif
}


static inline void
libtle_rwlock_read_lock(libtle_rwlock_t *lck)
{
#if defined(__x86_64__)
    unsigned tmp;
    __asm__ volatile(
"    1: mov    %0, %1\n"
"       testb  $0x3, %b1\n"
"       jne    2f\n"
"       mov    $0x4, %1\n"
"       lock ; xadd %1, %0\n"
"       testb  $0x1, %b1\n"
"       je     3f\n"
"       lock ; subl $0x4, %0\n"
"    2: pause\n"
"       jmp    1b\n"
"    3:\n"
        : "+m" (lck->lock), "=&r" (tmp)
        :
        : "memory");
#elif defined(__aarch64__)
    unsigned tmp;
    __asm__ volatile(
"       sevl\n"
"       prfm    pstl1strm, %1\n"
"    1: wfe\n"
"    2: ldaxr   %w0, %1\n"
"       tst     %w0, #0x3\n"
"       b.ne    1b\n"
"       add     %w0, %w0, #0x4\n"
"       stlxr   %w0, %w0, %1\n"
"       cbnz    %w0, 2b\n"
        : "=&r" (tmp), "+Q" (lck->lock)
        :
        : "memory");
#else
    while (1) {
        unsigned s = atomic_load_explicit(&lck->lock, memory_order_acquire);
        if (!(s & 3u)) {
            /* there is no pending or active writer, so we can
               acquire a reader lock (increment reader count) */
            unsigned t = atomic_fetch_add(&lck->lock, 4u);
            if (!(t & 1u)) {
                /* really no active writers, so we're OK */
                break;
            }
            /* writer got there first, undo the increment */
            (void) atomic_fetch_sub(&lck->lock, 4u);
        }
    }
#endif
}


static inline void
libtle_rwlock_write_unlock(libtle_rwlock_t *lck)
{
    /* clear pending and active writer flag; this will reset the
       pending writer so we give a chance for waiting readers to
       go ahead if they can get there before the pending writers */
    (void) atomic_fetch_and(&lck->lock, ~3u);
}


static inline void
libtle_rwlock_read_unlock(libtle_rwlock_t *lck)
{
    /* release the reader lock (decrement reader count) */
    (void) atomic_fetch_sub_explicit(&lck->lock, 4u, memory_order_release);
}


static inline int
libtle_rwlock_is_locked(libtle_rwlock_t *lck)
{
    /* true if no readers or writers hold the lock */
    return atomic_load_explicit(&lck->lock, memory_order_acquire);
}


static inline void
libtle_rwlock_unlock_wait(libtle_rwlock_t *lck)
{
#if defined(__x86_64__)
    while (libtle_rwlock_is_locked(lck)) {
        __asm__ volatile("pause");
    }
#elif defined(__aarch64__)
    unsigned tmp;
    __asm__ volatile(
"       sevl\n"
"    1: wfe\n"
"       ldaxr   %w0, %1\n"
"       cbnz    %w0, 1b\n"
        : "=r" (tmp)
        : "Q" (lck->lock));
#else
    /* wait until no readers or writers hold the lock */
    while (libtle_rwlock_is_locked(lck)) {
        __asm__ volatile ("" ::: "memory");
    }
#endif
}


#ifdef __cplusplus
}} // namespace tle::detail
#endif

#endif /* __LIBTLE_RWLOCK_H__ */
