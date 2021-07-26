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


mt_scanner_set::mt_scanner_set(scanner_config& sc_,
                               const feature_recorder_set::flags_t& f_,
                               class dfxml_writer* writer_):
    scanner_set(sc_, f_, writer_)
{
    std::cerr << "mt_scanner_set::mt_scanner_set(): this=" << this << " pool=" << pool << "\n";
}


void *mt_scanner_set::info() const
{
    std::cerr << "mt_scanner_set::info() this=" << this << " pool=" << (void *)pool << "\n";
    return (void *)pool;
}
