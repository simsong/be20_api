#include "config.h"
#include "threadpool.h"
#include "scanner_set.h"

#ifndef BARAKSH_THREADPOOL
thread_pool::thread_pool(int num_workers, scanner_set &ss_): workers(num_workers), ss(ss_)
{
    for (int i=0; i<workers; i++){
        new std::thread( &worker::start_worker, (void *)this );
    }
};


thread_pool::~thread_pool()
{
    /* Remove the workers from the work queue and kill each one. */
    for (int i=0; i < workers; i++ ){
        push_task(nullptr);
    }
    wait_for_tasks();                   // wait until no more tasks.
}

/* Launch the worker. It's kept on the per-thread stack.
 */
void *worker::start_worker(void *arg){
    worker w(static_cast<thread_pool *>(arg));
    w.run();
    return nullptr;
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
        sbuf_t *task  = nullptr;
        {
            std::unique_lock<std::mutex> lock( tp.M );
            /* At this point the worker has the lock */
            worker_wait_timer.start();  // waiting for work
            tp.freethreads++;           // this thread is free
            while (tp.tasks.empty()){   // wait until something is in the task queue
                /* I didn't get any work; go back to sleep */
                //std::cerr << std::this_thread::get_id() << " #1 tp.tasks.size()=" << tp.tasks.size() << std::endl;
                tp.TO_MAIN.notify_one(); // if main is sleeping, wake it up
                tp.TO_WORKER.wait( lock );
                //std::cerr << std::this_thread::get_id() << " #2 tp.tasks.size()=" << tp.tasks.size() << std::endl;
            }
            /* Worker still has the lock */
            task = tp.tasks.front();    // get the task
            tp.tasks.pop();           // remove it
            worker_wait_timer.stop();   // no longer waiting
            tp.freethreads--;           // no longer free
            /* release the lock */
        }
        //std::cerr << std::this_thread::get_id() << " task=" << task << std::endl;
	if (task==nullptr) {                  // special code to exit thread
            tp.TO_MAIN.notify_one();          // tell the master that one is gone
            return nullptr;
        }
        tp.ss.process_sbuf( task );
        tp.freethreads++;        // and now the thread is free!
        tp.TO_MAIN.notify_one(); // tell the master that we are free!
    }
}
#endif
