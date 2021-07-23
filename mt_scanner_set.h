/*
 * subclasses the scanner_set to make it multi-threaded.
 */

#ifndef MT_SCANNER_SET_H
#define MT_SCANNER_SET_H
#include <atomic>
#include <thread>
#include <mutex>

#include "thread-pool/thread_pool.hpp"
#include "scanner_set.h"
#include "atomic_map.h"
class mt_scanner_set: public scanner_set {

    static const inline size_t SAME_THREAD_SBUF_SIZE = 8192; // sbufs smaller than this run in the same thread.
    mt_scanner_set(const mt_scanner_set& s) = delete;
    mt_scanner_set& operator=(const mt_scanner_set& s) = delete;
    class thread_pool *pool {nullptr};     // nullptr means we are not threading

    std::atomic<int> depth0_sbufs_in_queue {0};
    std::atomic<uint64_t> depth0_bytes_in_queue {0};
    std::atomic<int> sbufs_in_queue {0};
    std::atomic<uint64_t> bytes_in_queue {0};

    /* status and notification system */
    [[noreturn]] void notify_thread();               // notify what's going on
    std::thread *notifier {nullptr};                 // notifier thread
    atomic_map<std::thread::id, std::string> status_map {}; // the status of each thread::id; does not need a mutex

public:
    mt_scanner_set(scanner_config &sc, const feature_recorder_set::flags_t& f, class dfxml_writer* writer);
    virtual ~mt_scanner_set(){};
    void launch_workers(int count);
    virtual void decrement_queue_stats(sbuf_t *sbufp);
    virtual void process_sbuf(sbuf_t* sbuf) override; // process the sbuf, then delete it.
    virtual void schedule_sbuf(sbuf_t* sbuf) override; // process the sbuf, then delete it.
    virtual void delete_sbuf(sbuf_t* sbuf) override; // process the sbuf, then delete it.
    virtual void thread_set_status(const std::string &status) override; // set the status for this thread
    virtual void print_tp_status();                  // print the status of each thread
    void join();                            // join the threads
    void run_notify_thread();               // notify what's going on
};
#endif
