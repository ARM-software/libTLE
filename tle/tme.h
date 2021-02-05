/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TME_H__
#define __TME_H__

#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h>
#endif

#ifndef __aarch64__
#error "TME requires AArch64"
#endif

#pragma GCC target ("+tme")

#define _XBEGIN_STARTED   (0)
#define _XABORT_RETRY     (1 << 15)
#define _XABORT_EXPLICIT  (1 << 16)
#define _XABORT_CONFLICT  (1 << 17)
#define _XABORT_UNKNOWN   (1 << 18)
#define _XABORT_ERROR     (1 << 19)
#define _XABORT_CAPACITY  (1 << 20)
#define _XABORT_NESTED    (1 << 21)
#define _XABORT_DEBUG     (1 << 22)
#define _XABORT_INTERRUPT (1 << 23)

/* Extract the abort reason */
#define _XABORT_CODE(x) ((x) & 0x7fff)


/*
 * Start a transaction. Returns _XBEGIN_STARTED when the transaction started
 * successfully. When the transaction aborts all side effects are undone and
 * an abort code is returned. There is no guarantee any transaction ever
 * succeeds, so there always needs to be a valid tested fallback path.
 */
static inline unsigned int __attribute__((__always_inline__))
_xbegin(void)
{
    int ret;
    __asm__ volatile ("tstart %0" : "=r"(ret) :: "memory");
    return ret;
}


/*
 * Commit the current transaction. When no transaction is active this will
 * fault. All memory side effects of the transactions will become visible to
 * other threads in an atomic matter.
 */
static inline void __attribute__((__always_inline__))
_xend(void)
{
    __asm__ volatile ("tcommit" ::: "memory");
}


/*
 * Returns transaction depth when in Transactional State, otherwise 0.
 */
static inline int __attribute__((__always_inline__))
_xtest(void)
{
    int ret;
    __asm__ ("ttest %0" : "=r"(ret) :: );
    return ret;
}


/*
 * Abort the current transaction. When no transaction is active this is a
 * no-op. status must be a 8bit constant, that is included in the status code
 * returned by _xbegin()
 *
 * We write this as a macro because gcc with -O0 does not inline the function,
 * even when __always_inline__ is specified.
 */
#define _xabort(status) \
    __asm__ volatile ("tcancel %0" :: "n"(status) : "memory")

#endif /* __TME_H__ */
