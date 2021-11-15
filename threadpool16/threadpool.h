#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

/****************************************************************
 *** THREADING SUPPORT
 ****************************************************************/

/**
 * \addtogroup internal_interfaces
 * @{
 */


/**
 * \file
 * http://stackoverflow.com/questions/4264460/wait-for-one-of-several-threads-to-finish
 * Here is the algorithm to run the thread pool with a work queue:
 *
 * \verbatim
 * main:
 *     set freethreads to numthreads
 *     init mutex M, condvars TO_MAIN and TO_WORKER
 *     start N worker threads
 *     while true:
 *         wait for work item
 *         claim M
 *         while freethreads == 0:
 *             cond-wait TO_MAIN, M
 *         put work item in queue
 *         decrement freethreads
 *         cond-signal TO_WORKER
 *         release M
 *
 * worker:
 *     init
 *     while true:
 *         claim M
 *         while no work in queue:
 *             cond-wait TO_WORKER, M
 *         get work to local storage
 *         release M
 *         do work
 *         claim M
 *         increment freethreads
 *         cond-signal TO_MAIN
 *         release M
 * \endverbatim
 */

#include <set>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>      // std::future, std::promise

#include "aftimer.h"

// There is a single thread_pool object
class worker;
class thread_pool {
    /*** neither copying nor assignment is implemented ***/
    thread_pool(const thread_pool &)=delete;
    thread_pool &operator=(const thread_pool &)=delete;

public:
    typedef std::vector<class worker *> worker_vector;
    worker_vector       workers {};
    std::mutex                          M;
    std::condition_variable	        TO_MAIN;
    std::condition_variable	        TO_WORKER;
    std::atomic<int>                    freethreads {0};

    // bulk_extractor specialiations
    class scanner_set &ss;		// one for all the threads; fs and fr are threadsafe
    std::queue<class sbuf_t *> work_queue  {};	// work to be done - here it is just a list of sbufs.
    aftimer		       main_wait_timer {};	// time spend waiting
    int                         mode {0}; // 0=running; 1 = waiting for workers to finish

    thread_pool(size_t num_workers, scanner_set &ss_);
    ~thread_pool();
    void wait_for_tasks();              // wait until there are no tasks running
    void push_task(sbuf_t *sbuf);

    // Status for callers
    int get_free_count();
    size_t get_thread_count();
    size_t get_tasks_queued();
};

// there is a worker object for each thread
class worker {
    thread_pool         &tp;		       // my thread pool
    void                *run();                               // run the worker
    aftimer		worker_wait_timer {};  // time the worker spent
public:
    const uint32_t id;
    static void * start_worker( void *arg );
    worker(class thread_pool &tp_, uint32_t id_): tp(tp_),id(id_){} // the worker
};


#endif
