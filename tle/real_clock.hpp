/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TLE_REAL_CLOCK_HPP__
#define __TLE_REAL_CLOCK_HPP__

#include <cassert>
#include <chrono>
#include <unistd.h>
#if _POSIX_TIMERS && !defined(__MACH__)
#include <time.h>
#else
#include <sys/time.h>
#endif

namespace tle {

    struct real_clock {
#if _POSIX_TIMERS && !defined(__MACH__)
        typedef std::chrono::nanoseconds    duration;
#else
        typedef std::chrono::microseconds   duration;
#endif
        typedef duration::rep               rep;
        typedef duration::period            period;
        typedef std::chrono::time_point<real_clock> time_point;

        static const bool is_steady = true;

        static time_point now() noexcept {
            rep answer;
#if _POSIX_TIMERS && !defined(__MACH__)
            struct timespec ts;
            if (clock_gettime(CLOCK_REALTIME, &ts)) {
                assert(0);
            }
            answer = static_cast<rep>(1000000000L) * static_cast<rep>(ts.tv_sec) +
                     static_cast<rep>(ts.tv_nsec);
#else
            struct timeval tv;
            if (gettimeofday(&tv, NULL)) {
                assert(0);
            }
            answer = static_cast<rep>(1000000L) * static_cast<rep>(tv.tv_sec) +
                     static_cast<rep>(tv.tv_usec);
#endif
            return time_point(duration(answer));
        }
    };

} // namespace tle

#endif // __TLE_REAL_CLOCK_HPP__
