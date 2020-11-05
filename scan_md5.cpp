/**
 *
 * scan_md5:
 * plug-in demonstration that shows how to write a simple plug-in scanner that calculates
 * the MD5 of each file..
 */

#include "config.h"

#include <iostream>
#include <sys/types.h>

#include "scan_md5.h"
#include "scanner_params.h"
#include "scanner_set.h"
#include "dfxml/src/hash_t.h"



void  scan_md5(const struct scanner_params &sp)
{

    if(sp.phase==scanner_params::PHASE_INIT){
        static scanner_params::scanner_info info;
        info.scanner = scan_md5;
        info.name   = "md5";
        info.author = "Simson L. Garfinkel";
        info.flags  = scanner_params::scanner_info::SCANNER_DEFAULT_DISABLED;
        std::cerr << "about to call sp.register_info\n";
        //std::cerr << "addr=" << sp.register_info << "\n";
        sp.ss.register_info(&info);
        return;
    }

    if(sp.phase==scanner_params::PHASE_SCAN){
	static const std::string hash0("<hashdigest type='MD5'>");
	static const std::string hash1("</hashdigest>");
	if(sp.sxml){
            (*sp.sxml) << hash0 << dfxml::md5_generator::hash_buf(sp.sbuf.buf,sp.sbuf.bufsize).hexdigest() << hash1;
        }
	return;
    }
}
