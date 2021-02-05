/* Copyright (C) 2020 by Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <set>
#include <vector>
#include <tle/mutex.hpp>
#include <tle/real_clock.hpp>

using jiffies = std::chrono::duration<double, std::ratio<1, 1000000>>;

//
// command line parameters
//
size_t num_threads = 4;
std::string mutex_type("");
jiffies lock_interval(10);
jiffies lock_duration(5);

std::set<std::string> mutex_types = {
    "null_mutex",
    "spin_mutex",
    "htm_spin_mutex",
    "null_shared_mutex",
    "spin_shared_mutex",
    "htm_spin_shared_mutex"
};


//
// global state
//
auto time_limit = std::chrono::seconds(2);
uint64_t average_unlocked_work;
uint64_t average_locked_work;

//
// thread synchronization
//
std::atomic_int start_work;

//
// statistics
//
struct thread_stats {
    uint64_t work_done;
    uint64_t iterations;
    uint64_t result;
    jiffies  overshoot;

    uint64_t locks_acquired;
    uint64_t locks_elided;
    uint64_t explicit_aborts;
    uint64_t conflict_aborts;
    uint64_t capacity_aborts;
    uint64_t nested_aborts;
    uint64_t other_aborts;

    thread_stats(): work_done(0), iterations(0), result(0), overshoot(0),
        locks_acquired(0), locks_elided(0), explicit_aborts(0),
        conflict_aborts(0), capacity_aborts(0), nested_aborts(0),
        other_aborts(0)
    { }

    void assign_from(tle::null_mutex_profile& stats) {
    }

    void assign_from(tle::mutex_profile& stats) {
        locks_acquired = stats.locks_acquired;
    }

    void assign_from(tle::htm_mutex_profile& stats) {
        locks_acquired = stats.locks_acquired;
        locks_elided = stats.locks_elided;
        explicit_aborts = stats.explicit_aborts;
        conflict_aborts = stats.conflict_aborts;
        capacity_aborts = stats.capacity_aborts;
        nested_aborts = stats.nested_aborts;
        other_aborts = stats.other_aborts;
    }
};

std::vector<thread_stats> stats;


// -----------------------------------------------------------------------------
// Busy work
//

uint64_t __attribute__((noinline))
dummy_work(uint64_t amount, std::mt19937_64& busy)
{
    uint64_t dummy = busy();
    for (uint64_t i = 0; i < amount; i++) {
        dummy ^= busy();
    }

    return dummy;
}


// -----------------------------------------------------------------------------
// Thread execution
//

template<typename Mutex>
void thread_actions(size_t id, Mutex* mtx)
{
    typedef typename Mutex::profile_type profile_type;
    typedef typename Mutex::handle_type  handle_type;

    profile_type mtx_stats;
    handle_type work_locker(*mtx, &mtx_stats);

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 busy(seed);
    std::mt19937_64 generator(seed);
    std::poisson_distribution<uint64_t> unlocked_distribution(average_unlocked_work);
    std::poisson_distribution<uint64_t> locked_distribution(average_locked_work);

    start_work.fetch_sub(1);
    while (start_work.load(std::memory_order_acquire) > 0)
        /*wait*/;

    jiffies elapsed_time;
    auto start_tick = tle::real_clock::now();
    while (true) {

        // do some work without holding the lock
        uint64_t work_items = unlocked_distribution(generator);
        uint64_t dummy = dummy_work(work_items, busy);
        auto stop_tick = tle::real_clock::now();
        stats[id].result += dummy;
        stats[id].work_done += work_items;
        elapsed_time = std::chrono::duration_cast<jiffies>(stop_tick - start_tick);
        if (elapsed_time >= time_limit)
            break;

        // do some work holding the lock
        work_items = locked_distribution(generator);
        work_locker.lock();
        dummy = dummy_work(work_items, busy);
        work_locker.unlock();
        stop_tick = tle::real_clock::now();
        stats[id].result += dummy;
        stats[id].work_done += work_items;

        ++stats[id].iterations;

        elapsed_time = std::chrono::duration_cast<jiffies>(stop_tick - start_tick);
        if (elapsed_time >= time_limit)
            break;
    }

    stats[id].overshoot = elapsed_time - time_limit;
    stats[id].assign_from(mtx_stats);
}


template<typename Mutex>
void run_threads()
{
    Mutex mtx;

    // create N-1 threads
    std::vector<std::thread> threads;
    for (size_t id = 0; id < num_threads-1; ++id) {
        threads.push_back(std::thread(thread_actions<Mutex>, id, &mtx));
    }

    // run N threads
    thread_actions(num_threads-1, &mtx);

    // wait for the threads to finish
    for (auto& thr: threads) {
        thr.join();
    }
}


// -----------------------------------------------------------------------------
// Map real time to work items
//

jiffies jiffies_per_work_item()
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 busy(seed);
    uint64_t amount = 100000000;
    auto start_tick = tle::real_clock::now();
    std::ignore = dummy_work(amount, busy);
    auto stop_tick = tle::real_clock::now();
    double elapsed_time = std::chrono::duration_cast<jiffies>(stop_tick - start_tick).count();
    return jiffies(elapsed_time / static_cast<double>(amount));
}


// -----------------------------------------------------------------------------
// Pretty-print time intervals
//

std::string to_time_string(jiffies ticks)
{
    std::stringstream ss;
    if (ticks >= jiffies(1e6)) {
        ss << std::fixed << std::setprecision(3)
            << (ticks.count() / 1e6) << "s";
    }
    if (ticks >= jiffies(1e3)) {
        ss << std::fixed << std::setprecision(3)
            << (ticks.count() / 1e3) << "ms";
    }
    else if (ticks < jiffies(1)) {
        ss << std::fixed << std::setprecision(3)
            << (ticks.count() * 1e3) << "ns";
    } else {
        ss << std::fixed << std::setprecision(3)
            << ticks.count() << "us";
    }
    return ss.str();
}


// -----------------------------------------------------------------------------
// Command line argument parsing functions
//

void help()
{
    std::cout
        << "usage: bench [-h] [-n N] [-i R] [-d R] -t NAME" << std::endl
        << "where:" << std::endl
        << "  -h      shows this help message" << std::endl
        << "  -n N    number of threads" << std::endl
        << "  -i F    average interval (in usec) between lock acquisitions" << std::endl
        << "  -d F    average duration (in usec) of each lock acquisition" << std::endl
        << "  -t NAME type of mutex. One of: null_mutex, spin_mutex, " << std::endl
        << "          htm_spin_mutex, null_shared_mutex, " << std::endl
        << "          spin_shared_mutex, htm_spin_shared_mutex" << std::endl;
}


size_t safe_strtoul(const char* arg, const char* msg = NULL)
{
    errno = 0; // some glibc versions require this when linked statically

    char* end;
    size_t number = std::strtoul(arg, &end, 0);
    if (end == arg || end == NULL || *end != '\0') {
        std::cout << "error: illegal value for option " << msg << std::endl;
        help();
        exit(EXIT_FAILURE);
    }
    if (errno) {
        std::perror(msg);
        exit(EXIT_FAILURE);
    }
    return number;
}


double safe_strtod(const char* arg, const char* msg = NULL)
{
    errno = 0; // some glibc versions require this when linked statically

    char* end;
    double number = std::strtod(arg, &end);
    if (end == arg || end == NULL || *end != '\0') {
        std::cout << "error: illegal value for option " << msg << std::endl;
        help();
        exit(EXIT_FAILURE);
    }
    if (errno) {
        std::perror(msg);
        exit(EXIT_FAILURE);
    }
    return number;
}


// -----------------------------------------------------------------------------
// Program entry point
//

int main(int argc, char** argv)
{
    auto jpw = jiffies_per_work_item();

    // Parse command line arguments
    int ch = '\0';
    opterr = 0;
    while ((ch = getopt(argc, argv, "n:i:l:t:h")) != -1) {
        switch (ch) {
        case 'n':
            num_threads = safe_strtoul(optarg, "-n");
            break;
        case 'i':
            lock_interval = jiffies(safe_strtod(optarg, "-i"));
            if (lock_interval < jpw) {
                std::cerr
                    << "error: lock interval (" << to_time_string(lock_interval)
                    << ") must be bigger than " << to_time_string(jpw)
                    << std::endl;
                exit(EXIT_FAILURE);
            }
            break;
        case 'l':
            lock_duration = jiffies(safe_strtod(optarg, "-l"));
            if (lock_duration < jpw && lock_duration.count() != 0) {
                std::cerr
                    << "error: lock duration (" << to_time_string(lock_duration)
                    << ") must be bigger than " << to_time_string(jpw)
                    << std::endl;
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            mutex_type = std::string(optarg);
            if (mutex_types.find(mutex_type) == mutex_types.end()) {
                std::cerr << "error: mutex type (" << mutex_type
                    << ") must be on of: ";
                for (auto it = mutex_types.cbegin(); it != mutex_types.cend(); ++it) {
                    if (it != mutex_types.cbegin())
                        std::cerr << ", ";
                    std::cerr << *it;
                }
                std::cerr << std::endl;
                exit(EXIT_FAILURE);
            }
            break;
        default:
            if (ch != 'h') {
                std::cout << "error: illegal option -"
                    << static_cast<char>(optopt)
                    << std::endl;
            }
            help();
            exit((ch == 'h') ? EXIT_SUCCESS : EXIT_FAILURE);
        }
    }
    if (mutex_type == "") {
        std::cerr << "error: missing mutex type parameter" << std::endl;
        exit(EXIT_FAILURE);
    }

    // update global state
    start_work.store(num_threads, std::memory_order_release);
    stats.resize(num_threads);
    uint64_t average_lock_interval = 0.5f + lock_interval.count() / jpw.count();
    average_locked_work = 0.5f + lock_duration.count() / jpw.count();
    average_unlocked_work = average_lock_interval -  average_locked_work;

    // report options
    std::cout
        << "number of threads:      " << num_threads << std::endl
        << "avg. lock interval:     " << to_time_string(lock_interval) << std::endl
        << "avg. lock duration:     " << to_time_string(lock_duration)
        << " (" << (100.0 * lock_duration.count() / lock_interval.count()) << "%)"
        << std::endl
        << "avg. work per interval: " << average_unlocked_work
        << " (" << to_time_string(jiffies(average_unlocked_work * jpw.count())) << ")"
        << std::endl
        << "avg. work per lock:     " << average_locked_work
        << " (" << to_time_string(jiffies(average_locked_work * jpw.count())) << ")"
        << std::endl;

    // run the test
    if (mutex_type == "null_mutex") {
        run_threads<tle::null_mutex>();
    }
    else if (mutex_type == "spin_mutex") {
        run_threads<tle::spin_mutex>();
    }
    else if (mutex_type == "htm_spin_mutex") {
        run_threads<tle::htm_spin_mutex>();
    }
    else if (mutex_type == "null_shared_mutex") {
        run_threads<tle::null_shared_mutex>();
    }
    else if (mutex_type == "spin_shared_mutex") {
        run_threads<tle::spin_shared_mutex>();
    }
    else if (mutex_type == "htm_spin_shared_mutex") {
        run_threads<tle::htm_spin_shared_mutex>();
    }
    else {
        exit(EXIT_FAILURE);
    }

    // report results
    thread_stats total;
    for (auto& st: stats) {
        total.work_done += st.work_done;
        total.iterations += st.iterations;
        total.overshoot += st.overshoot;
        total.locks_acquired += st.locks_acquired;
        total.locks_elided += st.locks_elided;
        total.explicit_aborts += st.explicit_aborts;
        total.conflict_aborts += st.conflict_aborts;
        total.capacity_aborts += st.capacity_aborts;
        total.nested_aborts += st.nested_aborts;
        total.other_aborts += st.other_aborts;
    }
    std::cout
        << "throughput (Mwork/sec):  " << 1e-6 * (total.work_done / time_limit.count()) << std::endl
        << "overshoot:  " << to_time_string(total.overshoot) << std::endl
        << "work items: " << total.work_done << std::endl
        << "iterations: " << total.iterations << std::endl;
    std::cout
        << "locks_acquired:  " << total.locks_acquired << std::endl
        << "locks_elided:    " << total.locks_elided << std::endl;
    if (total.locks_elided) {
        std::cout
            << "conflict_aborts: " << total.conflict_aborts << std::endl
            << "capacity_aborts: " << total.capacity_aborts << std::endl
            << "explicit_aborts: " << total.explicit_aborts << std::endl
            << "nested_aborts:   " << total.nested_aborts << std::endl
            << "other_aborts:    " << total.other_aborts << std::endl;
    }

    return 0;
}
