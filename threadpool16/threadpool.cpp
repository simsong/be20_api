#include "threadpool.h"
#include "aftimer.h"

/* Run the worker.
 * Note that we used to throw internal errors, but this caused problems with some versions of GCC.
 * Now we simply return when there is an error.
 */
void *worker::run()
{
    tp.freethreads++;
    while(true){
	/* Get the lock, then wait for the queue to be empty.
	 * If it is not empty, wait for the lock again.
	 */
        sbuf_t *task  = nullptr;
        {
            std::unique_lock<std::mutex> lock( tp.M );

            /* At this point the worker has the lock */
            worker_wait_timer.start();
            while (tp.tasks.empty()){
                /* I didn't get any work; go back to sleep */
                tp.TO_WORKER.wait( lock );
            }
            /* Worker still has the lock */
            worker_wait_timer.stop();

            /* Get the task */
            task = tp.tasks.front();
            tp.tasks.pop();
            tp.freethreads--;                         // no longer free
            /* release the lock */
        }
	if (task==nullptr) {                  // special code to exit thread
                break;
        }
        tp.ss.process_sbuf( task );
        tp.freethreads++;                       // and now the thread is free!
        tp.TO_MAIN.notify_one(); // tell the master that we are free!
    }
    return 0;
}
