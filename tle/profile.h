/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __LIBTLE_PROFILE_H__
#define __LIBTLE_PROFILE_H__

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#include <stdalign.h>
#endif

#if defined(__x86_64__)
#include <immintrin.h>
#elif defined(__aarch64__)
#include "tme.h"
#else
#error "Only x86-64 and Aarch64 HTM support for now"
#endif


#ifdef __cplusplus
namespace tle{ namespace detail{
#endif


/* -------------------------------------------------------------------------- */
/* Runtime statistics for empty locks                                         */
/* -------------------------------------------------------------------------- */


typedef struct {
} libtle_null_mutex_profile_t;


static inline void
libtle_null_mutex_profile_init(libtle_null_mutex_profile_t *p)
{ }


static inline int
libtle_null_mutex_profile_internally_consistent(const libtle_null_mutex_profile_t *p,
                                                uint64_t sum)
{
    return 1;
}


static inline void
libtle_null_mutex_profile_accumulate(libtle_null_mutex_profile_t *p,
                                     const libtle_null_mutex_profile_t *q)
{ }


static inline void
libtle_null_mutex_profile_update_unlock(libtle_null_mutex_profile_t *p)
{ }


/* -------------------------------------------------------------------------- */
/* Runtime statistics for spinlocks                                           */
/* -------------------------------------------------------------------------- */


typedef struct {
    alignas(64) uint64_t locks_acquired;
} libtle_mutex_profile_t;


static inline void
libtle_mutex_profile_init(libtle_mutex_profile_t *p)
{
    p->locks_acquired = 0;
}


static inline int
libtle_mutex_profile_internally_consistent(const libtle_mutex_profile_t *p,
                                           uint64_t sum)
{
    return p->locks_acquired == sum;
}


static inline void
libtle_mutex_profile_accumulate(libtle_mutex_profile_t *p,
                                const libtle_mutex_profile_t *q)
{
    p->locks_acquired += q->locks_acquired;
}


static inline void
libtle_mutex_profile_update_unlock(libtle_mutex_profile_t *p)
{
    p->locks_acquired += 1;
}


/* -------------------------------------------------------------------------- */
/* Runtime statistics for HTM eliding spinlocks                               */
/* -------------------------------------------------------------------------- */


typedef struct {
    alignas(64) uint64_t  locks_acquired;
    uint64_t  locks_elided;
    uint64_t  explicit_aborts;
    uint64_t  conflict_aborts;
    uint64_t  capacity_aborts;
    uint64_t  nested_aborts;
    uint64_t  other_aborts;
} libtle_htm_mutex_profile_t;


static inline void
libtle_htm_mutex_profile_init(libtle_htm_mutex_profile_t *p)
{
    p->locks_acquired = 0;
    p->locks_elided = 0;
    p->explicit_aborts = 0;
    p->conflict_aborts = 0;
    p->capacity_aborts = 0;
    p->nested_aborts = 0;
    p->other_aborts = 0;
}


static inline int
libtle_htm_mutex_profile_internally_consistent(const libtle_htm_mutex_profile_t *p,
                                               uint64_t sum)
{
    if (p->locks_acquired + p->locks_elided != sum) {
        return 0;
    }
    uint64_t aborts = p->explicit_aborts + p->conflict_aborts +
        p->capacity_aborts + p->nested_aborts + p->other_aborts;
    return (p->locks_acquired <= aborts)
        || (aborts == 0 && p->locks_elided == 0);
}


static inline void
libtle_htm_mutex_profile_accumulate(libtle_htm_mutex_profile_t *p,
                                    const libtle_htm_mutex_profile_t *q)
{
    p->locks_acquired  += q->locks_acquired;
    p->locks_elided    += q->locks_elided;
    p->explicit_aborts += q->explicit_aborts;
    p->conflict_aborts += q->conflict_aborts;
    p->capacity_aborts += q->capacity_aborts;
    p->nested_aborts   += q->nested_aborts;
    p->other_aborts    += q->other_aborts;
}


static inline void
libtle_htm_mutex_profile_update_unlock(libtle_htm_mutex_profile_t *p)
{
    p->locks_acquired += 1;
}


static inline void
libtle_htm_mutex_profile_update_commit(libtle_htm_mutex_profile_t *p)
{
    p->locks_elided += 1;
}


static inline void
libtle_htm_mutex_profile_update_abort(libtle_htm_mutex_profile_t *p,
                                      unsigned xstatus)
{
    if (xstatus & _XABORT_CONFLICT) {
        p->conflict_aborts += 1;
    }
    else if (xstatus & _XABORT_EXPLICIT) {
        p->explicit_aborts += 1;
    }
    else if (xstatus & _XABORT_CAPACITY) {
        p->capacity_aborts += 1;
    }
    else if (xstatus & _XABORT_NESTED) {
        p->nested_aborts += 1;
    }
    else {
        // _XABORT_DEBUG, _XABORT_INVALID, _XABORT_ERROR, _XABORT_UNKNOWN
        p->other_aborts += 1;
    }
}


/* -------------------------------------------------------------------------- */
/* Generics                                                                   */
/* -------------------------------------------------------------------------- */

#ifndef __cplusplus

#define libtle_mutex_profile_init(X) _Generic((X), \
         libtle_mutex_profile_t*: libtle_mutex_profile_init, \
     libtle_htm_mutex_profile_t*: libtle_htm_mutex_profile_init, \
    libtle_null_mutex_profile_t*: libtle_null_mutex_profile_init \
)(X)


#define libtle_mutex_profile_internally_consistent(X, Y) _Generic((X), \
         libtle_mutex_profile_t*: libtle_mutex_profile_internally_consistent, \
     libtle_htm_mutex_profile_t*: libtle_htm_mutex_profile_internally_consistent, \
    libtle_null_mutex_profile_t*: libtle_null_mutex_profile_internally_consistent \
)(X, Y)


#define libtle_mutex_profile_accumulate(X, Y) _Generic((X), \
         libtle_mutex_profile_t*: libtle_mutex_profile_accumulate, \
     libtle_htm_mutex_profile_t*: libtle_htm_mutex_profile_accumulate, \
    libtle_null_mutex_profile_t*: libtle_null_mutex_profile_accumulate \
)(X, Y)

#else

static inline void
libtle_mutex_profile_init(libtle_null_mutex_profile_t *p)
{
    libtle_null_mutex_profile_init(p);
}

static inline void
libtle_mutex_profile_init(libtle_htm_mutex_profile_t *p)
{
    libtle_htm_mutex_profile_init(p);
}


static inline int
libtle_mutex_profile_internally_consistent(const libtle_null_mutex_profile_t *p,
                                           uint64_t sum)
{
    return libtle_null_mutex_profile_internally_consistent(p ,sum);
}

static inline int
libtle_mutex_profile_internally_consistent(const libtle_htm_mutex_profile_t *p,
                                           uint64_t sum)
{
    return libtle_htm_mutex_profile_internally_consistent(p, sum);
}


static inline void
libtle_mutex_profile_accumulate(libtle_null_mutex_profile_t *p,
                                const libtle_null_mutex_profile_t *q)
{
    libtle_null_mutex_profile_accumulate(p, q);
}

static inline void
libtle_mutex_profile_accumulate(libtle_htm_mutex_profile_t *p,
                                const libtle_htm_mutex_profile_t *q)
{
    libtle_htm_mutex_profile_accumulate(p, q);
}

#endif


#ifdef __cplusplus
}} // namespace tle::detail
#endif

#endif /* __LIBTLE_PROFILE_H__ */
