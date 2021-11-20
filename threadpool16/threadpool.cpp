#include "config.h"
#include "threadpool.h"
#include "scanner_set.h"

#ifndef BARAKSH_THREADPOOL
thread_pool::thread_pool(size_t num_workers, scanner_set &ss_): ss(ss_)
{
    std::cerr << "a. workers.size() = " << workers.size() << std::endl;
    for (size_t i=0; i < num_workers; i++){
        std::cerr << "b. workers.size() = " << workers.size() << std::endl;
        std::unique_lock<std::mutex> lock(M);
        class worker *w = new worker(*this,i);
        workers.insert(w);
        std::cerr << "c. workers.size() = " << workers.size() << std::endl;
        new std::thread( &worker::start_worker, static_cast<void *>(w) );
    }
};


thread_pool::~thread_pool()
{
    /* We previously sent the termination message to all of the sub-threads here.
     * However, their terminating caused wacky problems with the malloc library.
     * So we just leave them floating around now. Doesn't matter much, because
     * the main process will die soon enough.
     */
    std::cerr << "~thread_pool() 1 " << std::endl;
}

void thread_pool::wait_for_tasks()
{
    //std::cerr << "thread_pool::wait_for_tasks  tasks.size()=" << tasks.size() << std::endl;
    std::unique_lock<std::mutex> lock(M);
    //std::cerr << "thread_pool::wait_for_tasks  got lock tasks.size()=" << tasks.size() << std::endl;
    // wait until a thread is free (doesn't matter which)
    while (work_queue.size() > 0 ){
        //std::cerr << "wait_for_tasks. tasks.size()==" << tasks.size() << "\n";
        TO_WORKER.notify_one();         // wake up a worker in case one is sleeping
        TO_MAIN.wait( lock );
    }
};


void thread_pool::join()
{
    /* First send a kill message to each active thread. */
    size_t num_threads = get_thread_count(); // get the count with lock
    for(size_t i=0;i < num_threads;i++){
        push_task(nullptr);             // tell a thread to die
    }

    // This is a spin lock until there are no more workers.
    int count=0;
    while (get_thread_count()>0){
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ));
        if (debug) {
            std::cerr << "sleeping " << ++count << std::endl;
            debug_pool(std::cerr);
        }
    }
}

void thread_pool::push_task(sbuf_t *sbuf)
{
    std::unique_lock<std::mutex> lock(M);
    while (freethreads==0){               // if there are no free threads, wait.
        main_wait_timer.start();
        //TO_WORKER.notify_one();         // if a worker is sleeping, wake it up
        TO_MAIN.wait( lock );
        main_wait_timer.stop();
    }
    work_queue.push( sbuf );
    //freethreads--;
    TO_WORKER.notify_one();
};


int thread_pool::get_free_count() const
{
    std::lock_guard<std::mutex> lock(M);
    return freethreads;
};

size_t thread_pool::get_thread_count() const
{
    std::lock_guard<std::mutex> lock(M);
    return workers.size();
}

size_t thread_pool::get_tasks_queued() const
{
    std::lock_guard<std::mutex> lock(M);
    return work_queue.size();
}


void thread_pool::debug_pool(std::ostream &os) const
{
    os
        << " thread_count: " << get_thread_count()
        << " free_count: "   << get_free_count()
        << " tasks_queued: " << get_tasks_queued()
        << std::endl;
}

/* Launch the worker. It's kept on the per-thread stack.
 */
void * worker::start_worker(void *arg)
{
    worker *w = static_cast<class worker *>(arg);
    return w->run();
}


/* Run the worker.
 * Note that we used to throw internal errors, but this caused problems with some versions of GCC.
 * Now we simply return when there is an error.
 */
void *worker::run()
{
    if (tp.debug) std::cerr << "worker " << std::this_thread::get_id() << " starting " << std::endl;
    while(true){
	/* Get the lock, then wait for the queue to be empty.
	 * If it is not empty, wait for the lock again.
	 */
        sbuf_t *sbuf  = nullptr;
        {
            std::unique_lock<std::mutex> lock( tp.M );
            if (tp.debug) std::cerr << "worker " << std::this_thread::get_id() << " has lock " << std::endl;
            worker_wait_timer.start();  // waiting for work
            tp.freethreads++;           // this thread is free
            while ( tp.work_queue.size()==0 ){   // wait until something is in the task queue
                if (tp.debug) std::cerr << "worker " << std::this_thread::get_id() << " waiting " << std::endl;
                /* I didn't get any work; go to sleep */
                //std::cerr << std::this_thread::get_id() << " #1 tp.tasks.size()=" << tp.tasks.size() << std::endl;
                tp.TO_MAIN.notify_one(); // if main is sleeping, wake it up
                tp.TO_WORKER.wait( lock );
                //std::cerr << std::this_thread::get_id() << " #2 tp.tasks.size()=" << tp.tasks.size() << std::endl;
            }
            worker_wait_timer.stop();   // no longer waiting

            /* Worker still has the lock */
            sbuf = tp.work_queue.front();    // get the task
            tp.work_queue.pop();           // remove it
            tp.freethreads--;           // no longer free
            /* release the lock */
        }
        //std::cerr << std::this_thread::get_id() << " task=" << task << std::endl;
	if (sbuf==nullptr) {                  // special code to exit thread
            //tp.TO_MAIN.notify_one();          // tell the master that one is gone
            break;
        }
        tp.ss.process_sbuf( sbuf );     // deletes the sbuf
        {
            std::unique_lock<std::mutex> lock( tp.M );
            tp.freethreads++;        // and now the thread is free!
            tp.TO_MAIN.notify_one(); // tell the master that we are free!
        }
    }
    if (tp.debug) std::cerr << std::this_thread::get_id() << " exiting "<< std::endl;
    {
        std::unique_lock<std::mutex> lock(tp.M);
        tp.workers.erase(this);
    }

    return nullptr;
}
#endif
