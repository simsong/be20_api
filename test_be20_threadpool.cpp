/*
 * be20_api threadpool test is in this file.
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

[[noreturn]] void alarm_handler(int signal)
{
    std::cerr << "alarm\n";
    throw std::runtime_error("scanner_set_mt timeout");
}

// This will give an error unless run with MallocNanoZone=0
TEST_CASE("scanner_set_mt", "[thread_pool]") {
    std::cout << std::endl << "This will take at least 60 seconds. Don't give up..." << std::endl;
    INFO("scanner_set_mt test start");
    std::atomic<bool> done{false};

    std::thread watchdog([&] {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(60s);
        if (!done) {
            FAIL("scanner_set_mt test timed out");
        }
    });

    scanner_config sc;
    feature_recorder_set::flags_t f;
    scanner_set ss(sc, f, nullptr);
    ss.launch_workers(12);
    ss.set_spin_poll_time(1);
    ss.join();

    done = true;
    watchdog.join();
}
