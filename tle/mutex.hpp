/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __TLE_MUTEX_HPP__
#define __TLE_MUTEX_HPP__

#include <cassert>
#include <atomic>
#include <memory>

#include "mutex.h"
#include "lock.hpp"
#include "profile.hpp"


namespace tle{ namespace detail{

    //
    // Generic wrapper of a C mutex type into a C++ mutex class
    //
    template<typename Mutex, typename Handle, typename Profile>
    class mutex_wrapper {
    public:
        typedef Profile profile_type;

        //
        // This is a local handle for a mutex object.  The handle provides
        // lock() and unlock() methods similar to the std::mutex type.
        //
        class handle_type {
        public:
            typedef mutex_wrapper<Mutex,Handle,Profile> mutex_type;
            typedef typename mutex_type::profile_type   profile_type;

            handle_type(const handle_type&) = delete;
            handle_type& operator=(const handle_type&) = delete;

            handle_type(handle_type&&) = delete;
            handle_type& operator=(handle_type&&) = delete;

            handle_type(mutex_type& __m, profile_type* __s = nullptr) noexcept
            : _M_mutex(std::addressof(__m._M_impl)), _M_stats(__s), _M_handle()
            {
                libtle_mutex_handle_init(&_M_handle);
            }

            void lock() {
                detail::libtle_mutex_lock(_M_mutex, &_M_handle,
                                          _M_stats->_M_recast());
            }

            void unlock() {
                detail::libtle_mutex_unlock(_M_mutex, &_M_handle,
                                            _M_stats->_M_recast());
            }

        private:
            Mutex*          _M_mutex;
            profile_type*   _M_stats;
            Handle          _M_handle;
        };

    public:
        mutex_wrapper(const mutex_wrapper&) = delete;
        mutex_wrapper& operator=(const mutex_wrapper&) = delete;

        mutex_wrapper(mutex_wrapper&&) = delete;
        mutex_wrapper& operator=(mutex_wrapper&&) = delete;

        mutex_wrapper() noexcept
        {
            detail::libtle_mutex_init(&_M_impl);
        }

    private:
        Mutex _M_impl;
        friend handle_type;
    };


    //
    // Specialization of the C mutex wrapper for mutexes that
    // do not use the handle
    //
    template<typename Mutex, typename Profile>
    class mutex_wrapper<Mutex,void,Profile> {
    public:
        typedef Profile profile_type;

        //
        // This is a local handle for a mutex object.  The handle provides
        // lock() and unlock() methods similar to the std::mutex type.
        //
        class handle_type {
        public:
            typedef mutex_wrapper<Mutex,void,Profile> mutex_type;
            typedef typename mutex_type::profile_type profile_type;

            handle_type(const handle_type&) = delete;
            handle_type& operator=(const handle_type&) = delete;

            handle_type(handle_type&&) = delete;
            handle_type& operator=(handle_type&&) = delete;

            handle_type(mutex_type& __m, profile_type* __s = nullptr) noexcept
            : _M_mutex(std::addressof(__m._M_impl)), _M_stats(__s)
            { }

            void lock() {
                detail::libtle_mutex_lock(_M_mutex, nullptr,
                                          _M_stats->_M_recast());
            }

            void unlock() {
                detail::libtle_mutex_unlock(_M_mutex, nullptr,
                                            _M_stats->_M_recast());
            }

        private:
            Mutex*          _M_mutex;
            profile_type*   _M_stats;
        };

    public:
        mutex_wrapper(const mutex_wrapper&) = delete;
        mutex_wrapper& operator=(const mutex_wrapper&) = delete;

        mutex_wrapper(mutex_wrapper&&) = delete;
        mutex_wrapper& operator=(mutex_wrapper&&) = delete;

        mutex_wrapper() noexcept
        {
            detail::libtle_mutex_init(&_M_impl);
        }

        // expose the lock/unlock interface to use the mutex without a handle

        void lock(profile_type* __s = nullptr) {
            detail::libtle_mutex_lock(&_M_impl, nullptr, __s->_M_recast());
        }

        void unlock(profile_type* __s = nullptr) {
            detail::libtle_mutex_unlock(&_M_impl, nullptr, __s->_M_recast());
        }

    private:
        Mutex _M_impl;
        friend handle_type;
    };


    //
    // Generic wrapper of a C shared_mutex type into a C++ shared_mutex class
    //
    template<typename Mutex, typename Handle, typename Profile>
    class shared_mutex_wrapper {
    public:
        typedef Profile profile_type;

        //
        // This is a local handle for a shared mutex object.  The handle
        // provides lock(), unlock(), lock_shared(), and unlock_shared()
        // methods similar to the std::shared_mutex type.
        //
        class handle_type {
        public:
            typedef shared_mutex_wrapper<Mutex,Handle,Profile>  mutex_type;
            typedef typename mutex_type::profile_type           profile_type;

            handle_type(const handle_type&) = delete;
            handle_type& operator=(const handle_type&) = delete;

            handle_type(handle_type&&) = delete;
            handle_type& operator=(handle_type&&) = delete;

            handle_type(mutex_type& __m, profile_type* __s = nullptr) noexcept
            : _M_mutex(std::addressof(__m._M_impl)), _M_stats(__s), _M_handle()
            {
                libtle_mutex_handle_init(&_M_handle);
            }

            void lock() {
                detail::libtle_mutex_lock(_M_mutex, &_M_handle,
                                          _M_stats->_M_recast());
            }

            void unlock() {
                detail::libtle_mutex_unlock(_M_mutex, &_M_handle,
                                            _M_stats->_M_recast());
            }

            void lock_shared() {
                detail::libtle_mutex_lock_shared(_M_mutex, &_M_handle,
                                                 _M_stats->_M_recast());
            }

            void unlock_shared() {
                detail::libtle_mutex_unlock_shared(_M_mutex, &_M_handle,
                                                   _M_stats->_M_recast());
            }

        private:
            Mutex*          _M_mutex;
            profile_type*   _M_stats;
            Handle          _M_handle;
        };

    public:
        shared_mutex_wrapper(const shared_mutex_wrapper&) = delete;
        shared_mutex_wrapper& operator=(const shared_mutex_wrapper&) = delete;

        shared_mutex_wrapper(shared_mutex_wrapper&&) = delete;
        shared_mutex_wrapper& operator=(shared_mutex_wrapper&&) = delete;

        shared_mutex_wrapper() noexcept
        {
            detail::libtle_mutex_init(&_M_impl);
        }

    private:
        Mutex _M_impl;
        friend handle_type;
    };


    //
    // Specialization of the C shared_mutex wrapper for mutexes
    // that do not use the handle
    //
    template<typename Mutex, typename Profile>
    class shared_mutex_wrapper<Mutex,void,Profile> {
    public:
        typedef Profile profile_type;

        //
        // This is a local handle for a shared mutex object.  The handle
        // provides lock(), unlock(), lock_shared(), and unlock_shared()
        // methods similar to the std::shared_mutex type.
        //
        class handle_type {
        public:
            typedef shared_mutex_wrapper<Mutex,void,Profile>  mutex_type;
            typedef typename mutex_type::profile_type         profile_type;

            handle_type(const handle_type&) = delete;
            handle_type& operator=(const handle_type&) = delete;

            handle_type(handle_type&&) = delete;
            handle_type& operator=(handle_type&&) = delete;

            handle_type(mutex_type& __m, profile_type* __s = nullptr) noexcept
            : _M_mutex(std::addressof(__m._M_impl)), _M_stats(__s)
            { }

            void lock() {
                detail::libtle_mutex_lock(_M_mutex, nullptr,
                                          _M_stats->_M_recast());
            }

            void unlock() {
                detail::libtle_mutex_unlock(_M_mutex, nullptr,
                                            _M_stats->_M_recast());
            }

            void lock_shared() {
                detail::libtle_mutex_lock_shared(_M_mutex, nullptr,
                                                 _M_stats->_M_recast());
            }

            void unlock_shared() {
                detail::libtle_mutex_unlock_shared(_M_mutex, nullptr,
                                                   _M_stats->_M_recast());
            }

        private:
            Mutex*          _M_mutex;
            profile_type*   _M_stats;
        };

    public:
        shared_mutex_wrapper(const shared_mutex_wrapper&) = delete;
        shared_mutex_wrapper& operator=(const shared_mutex_wrapper&) = delete;

        shared_mutex_wrapper(shared_mutex_wrapper&&) = delete;
        shared_mutex_wrapper& operator=(shared_mutex_wrapper&&) = delete;

        shared_mutex_wrapper() noexcept
        {
            detail::libtle_mutex_init(&_M_impl);
        }

        // expose the lock/unlock interface to use the mutex without a handle

        void lock(profile_type* __s = nullptr) {
            detail::libtle_mutex_lock(&_M_impl, nullptr, __s->_M_recast());
        }

        void unlock(profile_type* __s = nullptr) {
            detail::libtle_mutex_unlock(&_M_impl, nullptr, __s->_M_recast());
        }

        void lock_shared(profile_type* __s = nullptr) {
            detail::libtle_mutex_lock_shared(&_M_impl, nullptr,
                                             __s->_M_recast());
        }

        void unlock_shared(profile_type* __s = nullptr) {
            detail::libtle_mutex_unlock_shared(&_M_impl, nullptr,
                                               __s->_M_recast());
        }

    private:
        Mutex _M_impl;
        friend handle_type;
    };

}} // namespace tle::detail


namespace tle {

    //
    // Null mutex
    //
#ifndef NDEBUG
    using null_mutex =
        detail::mutex_wrapper<detail::libtle_null_mutex_t,
        detail::libtle_null_mutex_handle_t, null_mutex_profile>;
#else
    using null_mutex =
        detail::mutex_wrapper<detail::libtle_null_mutex_t,
        void, null_mutex_profile>;
#endif

    //
    // Spinlock-based mutex
    //
#ifndef NDEBUG
    using spin_mutex =
        detail::mutex_wrapper<detail::libtle_spin_mutex_t,
        detail::libtle_spin_mutex_handle_t, mutex_profile>;
#else
    using spin_mutex =
        detail::mutex_wrapper<detail::libtle_spin_mutex_t,
        void, mutex_profile>;
#endif

    //
    // HTM-based mutex with a spinlock as fallback
    //
#ifndef NDEBUG
    using htm_spin_mutex =
        detail::mutex_wrapper<detail::libtle_htm_spin_mutex_t,
        detail::libtle_htm_spin_mutex_handle_t, htm_mutex_profile>;
#else
    using htm_spin_mutex =
        detail::mutex_wrapper<detail::libtle_htm_spin_mutex_t,
        void, htm_mutex_profile>;
#endif

    //
    // Null reader/writer mutex
    //
#ifndef NDEBUG
    using null_shared_mutex =
        detail::shared_mutex_wrapper<detail::libtle_null_shared_mutex_t,
        detail::libtle_null_shared_mutex_handle_t, null_mutex_profile>;
#else
    using null_shared_mutex =
        detail::shared_mutex_wrapper<detail::libtle_null_shared_mutex_t,
        void, null_mutex_profile>;
#endif

    //
    // Reader/writer spinlock
    //
#ifndef NDEBUG
    using spin_shared_mutex =
        detail::shared_mutex_wrapper<detail::libtle_spin_shared_mutex_t,
        detail::libtle_spin_shared_mutex_handle_t, mutex_profile>;
#else
    using spin_shared_mutex =
        detail::shared_mutex_wrapper<detail::libtle_spin_shared_mutex_t,
        void, mutex_profile>;
#endif

    //
    // HTM-based mutex with a reader/writer spinlock as fallback
    //
    using htm_spin_shared_mutex =
        detail::shared_mutex_wrapper<detail::libtle_htm_spin_shared_mutex_t,
        detail::libtle_htm_spin_shared_mutex_handle_t, htm_mutex_profile>;

    //
    // Aliases for the mutex handles
    //
    using null_mutex_handle            = null_mutex::handle_type;
    using spin_mutex_handle            = spin_mutex::handle_type;
    using htm_spin_mutex_handle        = htm_spin_mutex::handle_type;
    using null_shared_mutex_handle     = null_shared_mutex::handle_type;
    using spin_shared_mutex_handle     = spin_shared_mutex::handle_type;
    using htm_spin_shared_mutex_handle = htm_spin_shared_mutex::handle_type;

} // namespace tle

#endif // __TLE_MUTEX_HPP__
