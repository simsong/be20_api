/*
 * be13_api threadpool test is in this file.
 * The goal is to have complete test coverage of the v2 API
 *
 */

// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_CONSOLE_WIDTH 120

#include "config.h"
#include "catch.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <csignal>

#include "dfxml_cpp/src/hash_t.h"
#include "dfxml_cpp/src/dfxml_writer.h"

#include "atomic_unicode_histogram.h"
#include "sbuf.h"
#include "sbuf_stream.h"
#include "scanner_set.h"
#include "threadpool.h"
#include "utils.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef BARAKSH_THREADPOOL
TEST_CASE("atomic_set_mt", "[atomic]") {
    thread_pool pool;
    atomic_set<std::string> as;
    for (size_t i=0;i<100;i++){
        pool.push_task( [&as, i] {
            for (int j=0;j<100;j++){
                std::stringstream ss;
                ss << "this is string " << j << " " << i << " " << std::this_thread::get_id();
                as.insert( ss.str() );
            }
        });
    }
    pool.wait_for_tasks();
    REQUIRE( as.keys().size() == 10000 );
    as.clear();
    REQUIRE( as.keys().size() == 0 );
}

TEST_CASE("atomic_map_mt", "[atomic]") {
    thread_pool pool;
    atomic_map<std::string, std::string> am;
    for (size_t i=0;i<100;i++){
        pool.push_task( [&am, i] {
            for (int j=0;j<100;j++){
                std::stringstream s1;
                s1 << "thread " << i;
                std::stringstream s2;
                s2 << "string " << j << " " << i << " " << std::this_thread::get_id();
                am[s1.str()] = s2.str();
            }
        });
    }
    pool.wait_for_tasks();
    REQUIRE( am.keys().size() == 100 );
    am.clear();
    REQUIRE( am.keys().size() == 0 );
}
#endif

#ifndef BARAKSH_THREADPOOL
[[noreturn]] void alarm_handler(int signal)
{
    std::cerr << "alarm\n";
    throw std::runtime_error("scanner_set_mt timeout");
}

// This will give an error unless run with MallocNanoZone=0
TEST_CASE("scanner_set_mt", "[thread_pool]") {
    std::signal(SIGALRM, alarm_handler);
    alarm(60);                          // in case it never finishes
    scanner_config sc;
    feature_recorder_set::flags_t f;
    scanner_set ss(sc, f, nullptr);
    ss.launch_workers( 12 );
    ss.set_spin_poll_time(1); // spin fast.
    ss.join();
    alarm(0);
}
#endif
