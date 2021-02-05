/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TLE_PROFILE_HPP__
#define __TLE_PROFILE_HPP__

#include <cstdint>
#include "profile.h"

namespace tle{ namespace detail{

    template<typename Tp>
    struct mutex_profile_wrapper : public Tp
    {
        Tp* _M_recast() {
            return static_cast<Tp*>(this);
        }

        const Tp* _M_recast() const {
            return static_cast<const Tp*>(this);
        }

        mutex_profile_wrapper() noexcept {
            libtle_mutex_profile_init(_M_recast());
        }

        void update_unlock() noexcept {
            libtle_mutex_profile_update_unlock(_M_recast());
        }

        bool internally_consistent(uint64_t sum) const noexcept {
            return libtle_mutex_profile_internally_consistent(_M_recast(), sum);
        }

        mutex_profile_wrapper<Tp>&
        operator+=(const mutex_profile_wrapper<Tp>& other) noexcept {
            libtle_mutex_profile_accumulate(_M_recast(), other._M_recast());
            return *this;
        }

        mutex_profile_wrapper<Tp>
        operator+(const mutex_profile_wrapper<Tp>& other) const noexcept {
            return mutex_profile_wrapper<Tp>(*this) += other;
        }
    };


    template<typename Tp>
    struct htm_mutex_profile_wrapper : public Tp
    {
        Tp* _M_recast() {
            return static_cast<Tp*>(this);
        }

        const Tp* _M_recast() const {
            return static_cast<const Tp*>(this);
        }

        htm_mutex_profile_wrapper() noexcept {
            libtle_htm_mutex_profile_init(_M_recast());
        }

        void update_unlock() noexcept {
            libtle_htm_mutex_profile_update_unlock(_M_recast());
        }

        void update_commit() noexcept {
            libtle_htm_mutex_profile_update_commit(_M_recast());
        }

        void update_abort() noexcept {
            libtle_htm_mutex_profile_update_abort(_M_recast());
        }

        bool internally_consistent(uint64_t sum) const noexcept {
            return libtle_htm_mutex_profile_internally_consistent(_M_recast(), sum);
        }

        htm_mutex_profile_wrapper<Tp>&
        operator+=(const htm_mutex_profile_wrapper<Tp>& other) noexcept {
            libtle_htm_mutex_profile_accumulate(_M_recast(), other._M_recast());
            return *this;
        }

        htm_mutex_profile_wrapper<Tp>
        operator+(const htm_mutex_profile_wrapper<Tp>& other) const noexcept {
            return htm_mutex_profile_wrapper<Tp>(*this) += other;
        }
    };

}} // namespace tle::detail


namespace tle {

    using null_mutex_profile =
        detail::mutex_profile_wrapper<detail::libtle_null_mutex_profile_t>;

    using mutex_profile =
        detail::mutex_profile_wrapper<detail::libtle_mutex_profile_t>;

    using htm_mutex_profile =
        detail::htm_mutex_profile_wrapper<detail::libtle_htm_mutex_profile_t>;

} // tle

#endif // __TLE_PROFILE_HPP__
