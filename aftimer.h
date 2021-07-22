#ifndef __AFTIMER_H__
#define __AFTIMER_H__

#define __STDC_WANT_LIB_EXT1__ 1
#include <ctime>
#include <cstdio>
#include <string>
#include <chrono>

class aftimer {
    std::chrono::time_point<std::chrono::steady_clock> t0 {};
    bool   running{};
    uint64_t elapsed_ns {}; //  for all times we have started and stopped
    uint64_t last_ns    {}; // time from when we last did a "start"
public:
    aftimer()  {}

    void start(); // start the timer
    void stop();  // stop the timer

    //std::chrono::time_point<std::chrono::system_clock> tstart() const {
    //return t0;       // time we started
//}
    double elapsed_seconds() const;                   // how long timer has been running, total
    uint64_t elapsed_nanoseconds() const;
    uint64_t lap_seconds() const;                          // how long the timer is running this time
    double eta(double fraction_done) const;           // calculate ETA in seconds, given fraction
    std::string hms(long t) const;                    // turn a number of seconds into h:m:s
    std::string elapsed_text() const;                 /* how long we have been running */
    std::string eta_text(double fraction_done) const; // h:m:s
    std::string eta_time(double fraction_done) const; // the actual time
};

/* This code is from:
 * http://social.msdn.microsoft.com/Forums/en/vcgeneral/thread/430449b3-f6dd-4e18-84de-eebd26a8d668
 * and:
 * https://gist.github.com/ugovaretto/5875385
 */

//inline void timestamp(struct timeval* t) { gettimeofday(t, NULL); }

inline void aftimer::start() {
    assert (running == false);
    t0 = std::chrono::steady_clock::now();
    running = true;
}

inline void aftimer::stop() {
    assert (running==true);
    auto v = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0 );
    last_ns = v.count();
    elapsed_ns += v.count();
    running = false;
}

//inline aftimer::duration aftimer::lap_time() const { return last_ns; }

inline double aftimer::elapsed_seconds() const {
    assert (running==false);
    return elapsed_ns / (1000.0 * 1000.0 * 1000.0);
}

inline uint64_t aftimer::elapsed_nanoseconds() const {
    assert (running==false);
    return elapsed_ns;
}

inline std::string aftimer::hms(long t) const {
    char buf[64];
    int days = t / (60 * 60 * 24);

    t = t % (60 * 60 * 24); /* what's left */

    int h = t / 3600;
    int m = (t / 60) % 60;
    int s = t % 60;
    buf[0] = 0;
    switch (days) {
    case 0: snprintf(buf, sizeof(buf), "%2d:%02d:%02d", h, m, s); break;
    case 1: snprintf(buf, sizeof(buf), "%d day, %2d:%02d:%02d", days, h, m, s); break;
    default: snprintf(buf, sizeof(buf), "%d days %2d:%02d:%02d", days, h, m, s);
    }
    return std::string(buf);
}

inline std::string aftimer::elapsed_text() const {
    return hms((int)elapsed_seconds());
}

/**
 * returns the number of seconds until the job is complete.
 */
inline double aftimer::eta(double fraction_done) const {
    double t = elapsed_seconds();
    if (t <= 0) return -1;             // can't figure it out
    if (fraction_done <= 0) return -1; // can't figure it out
    return (t * 1.0 / fraction_done - t);
}

/**
 * Retuns the number of hours:minutes:seconds until the job is done.
 */
inline std::string aftimer::eta_text(double fraction_done) const {
    double e = eta(fraction_done);
    if (e < 0) return std::string("n/a"); // can't figure it out
    return hms((long)e);
}

/**
 * Returns the time when data is due.
 */
inline std::string aftimer::eta_time(double fraction_done) const {
    time_t t = time_t(eta(fraction_done)) + time(0);
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[64];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

#endif
