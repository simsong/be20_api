#include "mt_scanner_set.h"

#include <thread>
#include <sys/types.h>
#include <time.h>

#if defined(HAVE_SYS_SYSCTL_H) && defined(HAVE_SYS_VMMETER_H)
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#define BSD_STYLE
#endif


#ifdef BSD_STYLE
int show_free_bsd(std::ostream &os)
{
    int rc;
    u_int page_size;
    struct vmtotal vmt;
    size_t vmt_size, uint_size;
    size_t val64, val64_size;

    vmt_size = sizeof(vmt);
    uint_size = sizeof(page_size);
    val64_size = sizeof(val64);

    val64=0;
    rc = sysctlbyname("hw.memsize", &val64, &val64_size, NULL, 0);
    if (rc==0) {
        os << "memsize: " << val64 << "\n";
    }

    val64=0;
    rc = sysctlbyname("vm.memory_pressure", &val64, &val64_size, NULL, 0);
    if (rc==0) {
        os << "memory_pressure: " << val64 << "\n";
    }

    rc = sysctlbyname("vm.vmtotal", &vmt, &vmt_size, NULL, 0);
    if (rc ==0){
        rc = sysctlbyname("vm.stats.vm.v_page_size", &page_size, &uint_size, NULL, 0);
        if (rc ==0 ){
            os << "Free memory       : " <<  vmt.t_free * page_size << "\n";
            os << "Available memory  : " << vmt.t_avm * page_size << "\n";
        }
    }
    return 0;
}
#endif


/*
 * Called in the worker thread to process the sbuf.
 */
void mt_scanner_set::work_unit::process_ss_sbuf() const
{
    std::cerr << std::this_thread::get_id() << " mt_scanner_set::work_unit::process " << *sbuf << "\n";
    ss.decrement_queue_stats(sbuf);     // it was removed from the queue
    ss.process_sbuf(sbuf);              // deletes the sbuf
}

mt_scanner_set::mt_scanner_set(const scanner_config& sc_,
                               const feature_recorder_set::flags_t& f_,
                               class dfxml_writer* writer_):
    scanner_set(sc_, f_, writer_)
{
}


void mt_scanner_set::thread_set_status(const std::string &status)
{
    status_map[std::this_thread::get_id()] = status;
}

void mt_scanner_set::decrement_queue_stats(sbuf_t *sbufp)
{
    if (sbufp->depth()==0){
        depth0_sbufs_in_queue -= 1;
        assert(depth0_sbufs_in_queue>=0);
        depth0_bytes_in_queue -= sbufp->bufsize;
    }
    sbufs_in_queue -= 1;
    assert(sbufs_in_queue>=0);
    bytes_in_queue -= sbufp->bufsize;
}

void mt_scanner_set::process_sbuf(sbuf_t *sbufp)
{
    thread_set_status(sbufp->pos0.str() + " process_sbuf");
    scanner_set::process_sbuf(sbufp);
}

void mt_scanner_set::schedule_sbuf(sbuf_t *sbufp)
{
    std::cerr << "schedule_sbuf " << *sbufp << "\n";
    /* Run in same thread? */
    if (pool==nullptr || (sbufp->depth() > 0 && sbufp->bufsize < SAME_THREAD_SBUF_SIZE)) {
        std::cerr << std::this_thread::get_id()
                  << " same thread: " << *sbufp << "\n";
        process_sbuf(sbufp);
        return;
    }

    std::cerr << std::this_thread::get_id()
              << "                enqueue: " << *sbufp <<  "\n";
    if (sbufp->depth()==0) {
        depth0_sbufs_in_queue += 1;
        depth0_bytes_in_queue += sbufp->bufsize;
    }
    sbufs_in_queue += 1;
    bytes_in_queue += sbufp->bufsize;

    /* Run in a different thread */
    struct work_unit wu(*this, sbufp);
    //pool->push_task( [wu]{ wu.process_ss_sbuf(); } );
    pool->push_task( [this, sbufp]{ this->process_sbuf(sbufp); } );
}

void mt_scanner_set::delete_sbuf(sbuf_t *sbufp)
{
    thread_set_status(sbufp->pos0.str() + " delete_sbuf");
    scanner_set::delete_sbuf(sbufp);
}

void mt_scanner_set::launch_workers(int count)
{
    pool = new thread_pool(count);
}

void mt_scanner_set::join()
{
    if (pool != nullptr) {
        pool->wait_for_tasks();
    }
}

void mt_scanner_set::notify_thread()
{
    while(true){
        time_t rawtime = time (0);
        struct tm *timeinfo = localtime (&rawtime);
        std::cerr << asctime(timeinfo) << "\n";
        print_tp_status();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void mt_scanner_set::run_notify_thread()
{
    notifier = new std::thread(&mt_scanner_set::notify_thread, this);
}


/*
 * Print the status of each thread in the threadpool.
 */
void mt_scanner_set::print_tp_status()
{
    if (pool==nullptr) return;

    std::cout << "---enter print_tp_status----------------\n";
    //std::cerr << "thread count " << tp->thread_count() << "\n";
    //std::cerr << "active count " << tp->active_count() << "\n";
    //std::cerr << "task count " << tp->task_count() << "\n";
    std::cerr << "depth 0 sbufs in queue " << depth0_sbufs_in_queue << "\n";
    std::cerr << "depth 0 bytes in queue " << depth0_bytes_in_queue << "\n";
    std::cerr << "sbufs in queue " << sbufs_in_queue << "\n";
    std::cerr << "bytes in queue " << bytes_in_queue << "\n";
#ifdef BSD_STYLE
    show_free_bsd(std::cerr);
#endif
    //tp->dump_status(std::cerr);
    std::cout << "---exit print_tp_status----------------\n";
}
