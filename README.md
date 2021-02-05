This is a library of simple C11 and C++11 mutex primitives that provides support
for Transactional Lock Elision (TLE) on Intel 64 with TSX and AArch64 with TME
platforms.

# C++11 API

The library provides the following simple lock types:

 * `tle::null_mutex`: just expose the mutex interface without actually locking
 * `tle::spin_mutex`: a test-and-set spinlock
 * `tle::htm_spin_mutex`: a transactionally elided test-and-set spinlock

Also, the library provides the following reader/writer lock types:

 * `tle::null_shared_mutex`: just expose the reader/writer lock interface
   without actually locking
 * `tle::spin_shared_mutex`: a reader/writer spinlock, with writer priority
 * `tle::htm_spin_shared_mutex`: a transactionally elided reader/writer
   spinlock, with writer priority

The above mutex types have a handle subtype (e.g.,
`tle::spin_mutex::handle_type`). Each thread must have a handle to hold the
mutex per-thread state, and optionally some profiling information.

The `tle::htm_spin_shared_mutex` cannot be used directly, it must be used
through its handle type. The same is true for all the other mutexes when
compiled with debugging enabled (without -DNDEBUG=1).

A mutex handle (or a mutex without a handle) can be used directly, or through
the `tle::unique_lock`, and `tle::shared_lock` wrappers or their equivalent
`std::unique_lock` and `std::shared_lock` (in C++14) wrappers.  The TLE wrappers
behave (mostly) like the STL ones, but they are more light-weight.

For convenience, the mutex handle types are aliased to the following names:
`tle::null_mutex_handle`, `tle::spin_mutex_handle`,
`tle::htm_spin_mutex_handle`, `tle::null_shared_mutex_handle`,
`tle::spin_shared_mutex_handle`, and `tle::htm_spin_shared_mutex_handle`.

Finally, the library provides `tle::real_clock`, a clock class similar to
the `std::chrono` clock classes (`system_clock`, `steady_clock`, etc.)

Example usage for mutexes, using `tle::htm_spin_mutex`:

```c++
#include <thread>
#include <iostream>
#include <tle/real_clock.hpp>
#include <tle/mutex.hpp>

// global variable "i" and an associated lock that protects it.
int g_i = 0;
tle::htm_spin_mutex g_i_mutex;
thread_local tle::htm_spin_mutex_handle g_i_lock(g_i_mutex);

int safe_access_scoped()
{
    tle::unique_lock<tle::htm_spin_mutex_handle> lock(g_i_lock);
    return ++g_i;
}

int safe_access_unscoped()
{
    g_i_lock.lock();
    int i = ++g_i;
    g_i_lock.unlock();
    return i;
}

int main()
{
    std::cout << __func__ << ": " << safe_access_scoped() << std::endl;

    std::thread t1(safe_access_scoped);
    std::thread t2(safe_access_unscoped);

    std::cout << __func__ << ": " << safe_access_scoped() << std::endl;

    auto start_tick = tle::real_clock::now();
    t1.join();
    t2.join();
    auto stop_tick = tle::real_clock::now();

    std::cout << __func__ << ": " << safe_access_unscoped() << std::endl;

    std::cout << "nsec: "
             << std::chrono::nanoseconds(stop_tick - start_tick).count()
             << std::endl;

    return 0;
}
```

Possible output:

```
main: 1
main: 2
main: 5
nsec: 38174
```

Example usage for shared mutexes, using `tle::htm_spin_shared_mutex`:

```c++
#include <thread>
#include <iostream>
#include <tle/real_clock.hpp>
#include <tle/shared_mutex.hpp>

// global variable "i" and an associated reader-writer lock that protects it.
int g_i = 0;
tle::htm_spin_shared_mutex g_i_mutex;
thread_local tle::htm_spin_shared_mutex_handle g_i_lock(g_i_mutex);

void safe_increment_scoped()
{
    tle::unique_lock<tle::htm_spin_shared_mutex_handle> lock(g_i_lock);
    ++g_i;
}

void safe_increment_unscoped()
{
    g_i_lock.lock();
    ++g_i;
    g_i_lock.unlock();
}

int safe_read_scoped()
{
    tle::shared_lock<tle::htm_spin_shared_mutex_handle> lock(g_i_lock);
    return g_i;
}

int safe_read_unscoped()
{
    g_i_lock.lock_shared();
    i = g_i;
    g_i_lock.unlock_shared();
    return i;
}

int main()
{
    std::cout << __func__ << ": " << safe_read_scoped() << std::endl;

    std::thread t1(safe_increment_scoped);
    std::thread t2(safe_increment_unscoped);

    std::cout << __func__ << ": " << safe_read_scoped() << std::endl;

    auto start_tick = tle::real_clock::now();
    t1.join();
    t2.join();
    auto stop_tick = tle::real_clock::now();

    std::cout << __func__ << ": " << safe_read_unscoped() << std::endl;

    std::cout << "nsec: "
             << std::chrono::nanoseconds(stop_tick - start_tick).count()
             << std::endl;

    return 0;
}
```

Possible output:

```
main: 0
main: 0
main: 2
nsec: 36623
```

# C11 API

The library provides the following simple lock types:

 * `libtle_null_mutex_t`: just expose the mutex interface without actually
   locking
 * `libtle_spin_mutex_t`: a test-and-set spinlock
 * `libtle_htm_spin_mutex_t`: a transactionally elided test-and-set spinlock

Also, the library provides the following reader/writer lock types:

 * `libtle_null_shared_mutex_t`: just expose the reader/writer lock interface
   without actually locking
 * `libtle_shared_mutex_t`: a reader/writer lock, with writer priority
 * `libtle_htm_spin_shared_mutex_t`: a transactionally elided reader/writer lock

The above mutex types have a corresponding handle subtype (e.g.,
`libtle_spin_mutex_handle_t`). Each thread must have a handle to hold the mutex
per-thread state, and optionally some profiling information.  Mutexes cannot be
used directly, they must be used through their handles.

There are two ways to initialize a mutex, either via the `tle_mutex_init()`
function or via assignment to a constant object (e.g.,
`LIBTLE_SPIN_MUTEX_INIT`).

Example usage for shared mutexes, using `libtle_htm_spin_shared_mutex_t`:

```c
#include <pthread.h>
#include <stdio.h>
#include <tle/mutex.h>

int g_i = 0;

/* protect g_i */
libtle_htm_spin_shared_mutex_t g_i_mutex = LIBTLE_HTM_SPIN_SHARED_MUTEX_INIT;
_Thread_local libtle_htm_spin_shared_mutex_handle_t g_i_lock;

void* safe_increment(void* data)
{
    libtle_mutex_lock(&g_i_mutex, &g_i_lock);
    ++g_i;
    libtle_mutex_unlock(&g_i_mutex, &g_i_lock);
}

int safe_read(void)
{
    int tmp;
    libtle_mutex_lock_shared(&g_i_mutex, &g_i_lock);
    tmp = g_i;
    libtle_mutex_unlock_shared(&g_i_mutex, &g_i_lock);
    return tmp;
}

int main()
{
    pthread_t thr[2];

    /* alternative way to initialize the mutex
    libtle_mutex_init(&g_i_mutex); */

    printf("%s: %d\n", __func__, safe_read());
    (void)pthread_create(&thr[0], NULL, safe_increment, NULL);
    (void)pthread_create(&thr[1], NULL, safe_increment, NULL);
    printf("%s: %d\n", __func__, safe_read());
    pthread_join(thr[0], NULL);
    pthread_join(thr[1], NULL);
    printf("%s: %d\n", __func__, safe_read());

    return 0;
}
```

Possible output:

```
main: 0
main: 0
main: 2
```
