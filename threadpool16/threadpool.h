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
    int 	                        workers;
    std::mutex                          M;
    std::condition_variable	        TO_MAIN;
    std::condition_variable	        TO_WORKER;
    std::atomic<int>                    freethreads {0};

    // bulk_extractor specialiations
    class scanner_set &ss;		// one for all the threads; fs and fr are threadsafe
    std::queue<class sbuf_t *> tasks  {};	// work to be done - here it is just a list of sbufs.
    aftimer		 tp_wait_timer {};	// time spend waiting

    thread_pool(int num_workers, scanner_set &ss_);
    ~thread_pool();
    int get_free_count() {
        std::lock_guard<std::mutex> lock(M);
        return freethreads;
    };
    size_t get_thread_count() {
        std::lock_guard<std::mutex> lock(M);
        return workers;
    }
    size_t get_tasks_queued() {
        std::lock_guard<std::mutex> lock(M);
        return tasks.size();
    }
    void wait_for_tasks()   {
        //std::cerr << "thread_pool::wait_for_tasks  tasks.size()=" << tasks.size() << std::endl;
        std::unique_lock<std::mutex> lock(M);
        //std::cerr << "thread_pool::wait_for_tasks  got lock tasks.size()=" << tasks.size() << std::endl;
        // wait until a thread is free (doesn't matter which)
        while (tasks.size() > 0 ){
            //std::cerr << "wait_for_tasks. tasks.size()==" << tasks.size() << "\n";
            TO_MAIN.wait( lock );
        }
    };

    void push_task(sbuf_t *sbuf) {
        //std::cerr << "thread_pool::push_tasks " << sbuf << std::endl;
        std::unique_lock<std::mutex> lock(M);
        //std::cerr << "thread_pool::push_tasks got lock " << sbuf << std::endl;
        // wait until a thread is free (doesn't matter which)
        while (freethreads==0){
            tp_wait_timer.start();
            //std::cerr << "thread_pool::push_tasks TO_MAIN.wait " << std::endl;
            TO_MAIN.wait( lock );
            tp_wait_timer.stop();
        }
        // Now that there is a free worker, send it the task
        //std::cerr << "push task=" << sbuf << std::endl;
        tasks.push( sbuf );
        //std::cerr << "tasks.size=" << tasks.size() << std::endl;
        TO_WORKER.notify_one();
    };
};

// there is a worker object for each thread
class worker {
    thread_pool            &tp;		// my thread pool
    void *run();                        // run the worker
public:
    static void * start_worker(void *arg); // create and start the worker
    worker(class thread_pool *tp_): tp(*tp_){} // the worker
    aftimer		worker_wait_timer {};	// time the worker spent
};


#endif
