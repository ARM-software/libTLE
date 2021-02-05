/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <thread>
#include <iostream>
#include <mutex>
#include <tle/mutex.hpp>

// shared variable
int g_i = 0;

// protects g_i
tle::spin_shared_mutex g_i_mutex;
thread_local tle::spin_shared_mutex_handle  g_i_lock(g_i_mutex);

void __attribute__((noinline)) safe_increment()
{
    g_i_lock.lock();
    ++g_i;
    g_i_lock.unlock();
}

int __attribute__((noinline)) safe_read()
{
    int tmp;
    g_i_lock.lock_shared();
    tmp = g_i;
    g_i_lock.unlock_shared();
    return tmp;
}

int main()
{
    std::cout << __func__ << ": " << safe_read() << std::endl;

    std::thread t1(safe_increment);
    std::thread t2(safe_increment);

    std::cout << __func__ << ": " << safe_read() << std::endl;

    t1.join();
    t2.join();

    std::cout << __func__ << ": " << safe_read() << std::endl;

    return 0;
}
