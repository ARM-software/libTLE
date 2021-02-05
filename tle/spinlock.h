/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBTLE_SPINLOCK_H__
#define __LIBTLE_SPINLOCK_H__

#ifdef __cplusplus
#include <atomic>

namespace tle{ namespace detail{

using std::atomic_int;
using std::memory_order_acquire;
using std::memory_order_release;

#else
#include <stdatomic.h>
#endif


/**
 * @brief  Test-and-set spinlock
 *
 * For x86_64 the lock holds an integer value. When %lock is 1, then the lock
 * is free, when %lock <= 0 the lock is busy.
 *
 * For all other architectures the lock holds a boolean value. When %lock is 0,
 * the lock is free, otherwise the lock is busy.
 */
typedef struct {
    atomic_int lock;
} libtle_spinlock_t;


#ifndef __cplusplus
#if defined(__x86_64__)
#define LIBTLE_SPINLOCK_INIT    { ATOMIC_VAR_INIT(1) }
#else
#define LIBTLE_SPINLOCK_INIT    { ATOMIC_VAR_INIT(0) }
#endif
#endif


static inline void
libtle_spinlock_init(libtle_spinlock_t *lck)
{
#if defined(__x86_64__)
    atomic_init(&lck->lock, 1);
#else
    atomic_init(&lck->lock, 0);
#endif
}


static inline void
libtle_spinlock_lock(libtle_spinlock_t *lck)
{
#if defined(__x86_64__)
    __asm__ volatile(
"    1: lock ; decl %0\n"
"       jns 2f\n"
"    3: pause\n"
"       cmpl $0, %0\n"
"       jle 3b\n"
"       jmp 1b\n"
"    2: "
        : "=m" (lck->lock) :: "memory", "cc");
#elif defined(__aarch64__)
    int lockval = 1;
    int tmp;
    __asm__ volatile(
"       sevl\n"
"       prfm    pstl1strm, %1\n"
"    1: wfe\n"
"    2: ldaxr   %w0, %2\n"
"       cbnz    %w0, 1b\n"
"       stxr    %w0, %w3, %1\n"
"       cbnz    %w0, 2b\n"
        : "=&r" (tmp), "+Q" (lck->lock)
        : "Q" (lck->lock), "r" (lockval)
        : "memory");
#else
    while (atomic_exchange_explicit(&lck->lock, 1, memory_order_acquire))
        ;
#endif
}


static inline void
libtle_spinlock_lock_uncontended(libtle_spinlock_t *lck)
{
#if defined(__x86_64__)
    atomic_store_explicit(&lck->lock, 0, memory_order_release);
#else
    atomic_store_explicit(&lck->lock, 1, memory_order_release);
#endif
}


static inline void
libtle_spinlock_unlock(libtle_spinlock_t *lck)
{
#if defined(__x86_64__)
    atomic_store_explicit(&lck->lock, 1, memory_order_release);
#else
    atomic_store_explicit(&lck->lock, 0, memory_order_release);
#endif
}


static inline int
libtle_spinlock_is_locked(libtle_spinlock_t *lck)
{
#if defined(__x86_64__)
    return atomic_load_explicit(&lck->lock, memory_order_acquire) <= 0;
#else
    return atomic_load_explicit(&lck->lock, memory_order_acquire);
#endif
}


static inline void
libtle_spinlock_unlock_wait(libtle_spinlock_t *lck)
{
#if defined(__x86_64__)
    while (libtle_spinlock_is_locked(lck))
        __asm__ volatile("pause");
#elif defined(__aarch64__)
    int tmp;
    __asm__ volatile(
"       sevl\n"
"    1: wfe\n"
"       ldaxr   %w0, %1\n"
"       cbnz    %w0, 1b\n"
        : "=&r" (tmp)
        : "Q" (lck->lock));
#else
    while (libtle_spinlock_is_locked(lck))
        ;
#endif
}


#ifdef __cplusplus
}} // namespace tle::detail
#endif

#endif /* __LIBTLE_SPINLOCK_H__ */
