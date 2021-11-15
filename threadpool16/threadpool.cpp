#include "config.h"
#include "threadpool.h"
#include "scanner_set.h"

#ifndef BARAKSH_THREADPOOL
thread_pool::thread_pool(size_t num_workers, scanner_set &ss_): workers(num_workers), ss(ss_)
{
    for (size_t i=0; i < num_workers; i++){
        std::unique_lock<std::mutex> lock(M);
        class worker *w = new worker(*this,i);
        workers.push_back(w);
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
#if 1
    size_t num_threads = get_thread_count();
    for(size_t i=0;i < num_threads;i++){
            push_task(nullptr);
    }
#endif
    std::cerr << "~thread_pool() 2" << std::endl;
    wait_for_tasks();                   // wait until no more tasks.
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


void thread_pool::push_task(sbuf_t *sbuf)
{
    std::unique_lock<std::mutex> lock(M);
    while (freethreads==0){         // if there are no free threads, wait.
        main_wait_timer.start();
        //TO_WORKER.notify_one();         // if a worker is sleeping, wake it up
        TO_MAIN.wait( lock );
        main_wait_timer.stop();
    }
    work_queue.push( sbuf );
    freethreads--;
    TO_WORKER.notify_one();
};


int thread_pool::get_free_count()
{
    std::lock_guard<std::mutex> lock(M);
    return freethreads;
};

size_t thread_pool::get_thread_count()
{
    std::lock_guard<std::mutex> lock(M);
    return workers.size();
}

size_t thread_pool::get_tasks_queued()
{
    std::lock_guard<std::mutex> lock(M);
    return work_queue.size();
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
    while(true){
	/* Get the lock, then wait for the queue to be empty.
	 * If it is not empty, wait for the lock again.
	 */
        sbuf_t *sbuf  = nullptr;
        {
            std::unique_lock<std::mutex> lock( tp.M );
            worker_wait_timer.start();  // waiting for work
            tp.freethreads++;           // this thread is free
            while ( tp.work_queue.size()==0 ){   // wait until something is in the task queue
                /* I didn't get any work; go to sleep */
                //std::cerr << std::this_thread::get_id() << " #1 tp.tasks.size()=" << tp.tasks.size() << std::endl;
                //tp.TO_MAIN.notify_one(); // if main is sleeping, wake it up
                tp.TO_WORKER.wait( lock );
                //std::cerr << std::this_thread::get_id() << " #2 tp.tasks.size()=" << tp.tasks.size() << std::endl;
            }
            worker_wait_timer.stop();   // no longer waiting

            /* Worker still has the lock */
            sbuf = tp.work_queue.front();    // get the task
            tp.work_queue.pop();           // remove it
            //tp.freethreads--;           // no longer free
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
    return nullptr;
}
#endif
