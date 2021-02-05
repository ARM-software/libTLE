/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TLE_LOCK_HPP__
#define __TLE_LOCK_HPP__

namespace tle {

    //
    // Used to specify locking strategies for tle::unique_lock and
    // tle::shared_lock.
    //
    struct defer_lock_t {};

    //
    // Instance of empty tag struct type
    //
    constexpr defer_lock_t  defer_lock = defer_lock_t();


    //
    // A unique_lock controls mutex ownership within a scope, releasing
    // ownership in the destructor.
    //
    template<typename _Mutex> struct unique_lock {
        typedef _Mutex mutex_type;

        unique_lock() = delete;

        unique_lock(const unique_lock&) = delete;
        unique_lock& operator=(const unique_lock&) = delete;

        unique_lock(unique_lock&&) = delete;
        unique_lock& operator=(unique_lock&&) = delete;

        explicit unique_lock(mutex_type& __m) noexcept
        : _M_handle(__m) {
            lock();
        }

        unique_lock(mutex_type& __m, defer_lock_t) noexcept
        : _M_handle(__m) {
            // defer locking, must explicitly lock the unique_lock or the mutex
        }

        ~unique_lock() {
            unlock();
        }

        void lock() {
            _M_handle.lock();
        }

        void unlock() {
            _M_handle.unlock();
        }

    private:
        mutex_type&     _M_handle;
    };


    //
    // A shared_lock controls shared_mutex ownership within a scope, releasing
    // ownership in the destructor.
    //
    template<typename _Mutex> struct shared_lock {
        typedef _Mutex mutex_type;

        shared_lock() = delete;

        shared_lock(const shared_lock&) = delete;
        shared_lock& operator=(const shared_lock&) = delete;

        shared_lock(shared_lock&&) = delete;
        shared_lock& operator=(shared_lock&&) = delete;

        explicit shared_lock(mutex_type& __m) noexcept
        : _M_handle(__m) {
            lock();
        }

        shared_lock(mutex_type& __m, defer_lock_t) noexcept
        : _M_handle(__m) {
            // defer locking, must explicitly lock the shared_lock or the mutex
        }

        ~shared_lock() {
            unlock();
        }

        void lock() {
            _M_handle.lock_shared();
        }

        void unlock() {
            _M_handle.unlock_shared();
        }

    private:
        mutex_type&     _M_handle;
    };

} // namespace tle

#endif // __TLE_LOCK_HPP__
