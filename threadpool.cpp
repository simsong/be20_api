#include "config.h"
#include "threadpool.h"
#include "scanner_set.h"

thread_pool::thread_pool(size_t num_workers, scanner_set &ss_): ss(ss_)
{
    for (size_t i=0; i < num_workers; i++){
        std::unique_lock<std::mutex> lock(M);
        class worker *w = new worker(*this,i);
        workers.insert(w);
        threads.insert(new std::thread( &worker::start_worker, static_cast<void *>(w) ));
    }
};


thread_pool::~thread_pool()
{
    /* We previously sent the termination message to all of the sub-threads here.
     * However, their terminating caused wacky problems with the malloc library.
     * So we just leave them floating around now. Doesn't matter much, because
     * the main process will die soon enough.
     */
    for (auto &it : threads ){
        it->join();
        delete it;
    }
}

bool d2=false;
void thread_pool::wait_for_tasks()
{
    if(d2) std::cerr << "thread_pool::wait_for_tasks  work_queue.size()=" << work_queue.size() << std::endl;
    std::unique_lock<std::mutex> lock(M);
    if(d2) std::cerr << "thread_pool::wait_for_tasks  got lock work_queue.size()=" << work_queue.size() << std::endl;
    // wait until a thread is free (doesn't matter which)
    while (work_queue.size() > 0 ){
        if(d2) std::cerr << "thread_pool::wait_for_tasks work_queue.size()==" << work_queue.size() << "\n";
        TO_WORKER.notify_one();         // wake up a worker in case one is sleeping
        TO_MAIN.wait( lock );
    }
};


void thread_pool::join()
{
    /* First send a kill message to each active thread. */
    size_t num_threads = get_worker_count(); // get the count with lock
    for(size_t i=0;i < num_threads;i++){
        push_task(nullptr);             // tell a thread to die
    }

    // This is a spin lock until there are no more workers. Gross, but it works.
    int count=0;
    while (get_worker_count()>0){
        std::this_thread::sleep_for( std::chrono::milliseconds( shutdown_spin_lock_poll_ms ));
        if (debug || d2) {
            std::cerr << "thread_pool::join sleeping " << ++count << std::endl;
            debug_pool(std::cerr);
        }
    }
}

/*
 * This may be called from any thread.
 * Right now it only works if called by main thread.
 */

void thread_pool::push_task(const sbuf_t *sbuf)
{
    std::unique_lock<std::mutex> lock(M);
    if (main_thread == std::this_thread::get_id()) {
        /* In the main thread, make sure there is a free worker before continiuing */
        while (freethreads==0){               // if there are no free threads, wait.
            main_wait_timer.start();
            //TO_WORKER.notify_one();         // if a worker is sleeping, wake it up
            TO_MAIN.wait( lock );
            main_wait_timer.stop();
        }
    }

    /* Add to the count */
    work_queue.push( new work_unit(sbuf) );
    // this doens't make sense if we can push from any thread:
    //freethreads--;
    TO_WORKER.notify_one();
};


int thread_pool::get_free_count() const
{
    std::lock_guard<std::mutex> lock(M);
    return freethreads;
};

size_t thread_pool::get_worker_count() const
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
        << " worker_count: " << get_worker_count()
        << " free_count: "   << get_free_count()
        << " tasks_queued: " << get_tasks_queued()
        << std::endl;
}

/* Launch the worker. It's kept on the per-thread stack. When it is done, delete it.
 */
void * worker::start_worker(void *arg)
{
    worker *w = static_cast<class worker *>(arg);
    auto ret = w->run();
    delete w;
    return ret;
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
        thread_pool::work_unit wu;
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
            thread_pool::work_unit *wup = tp.work_queue.front();    // get the task
            tp.work_queue.pop();           // remove it
            wu = *wup;
            delete wup;
            tp.freethreads--;           // no longer free
            /* release the lock */
        }
        //std::cerr << std::this_thread::get_id() << " task=" << task << std::endl;
	if (wu.sbuf==nullptr) {                  // special code to exit thread
            //tp.TO_MAIN.notify_one();          // tell the master that one is gone
            break;
        }
        tp.ss.process_sbuf( wu.sbuf );     // deletes the sbuf
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
    tp.total_worker_wait_ns += worker_wait_timer.running_nanoseconds();

    return nullptr;
}
