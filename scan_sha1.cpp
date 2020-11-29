/**
 *
 * scan_sha1:
 * plug-in demonstration that shows how to write a simple plug-in scanner that calculates
 * the SHA1 of each sbuf. The hash is written to both the XML file and to the md5 feature file.
 */

#include "config.h"

#include <iostream>
#include <sys/types.h>

#include "scan_sha1.h"
#include "scanner_params.h"
#include "scanner_set.h"
#include "dfxml/src/hash_t.h"



void  scan_sha1(struct scanner_params &sp)
{

    if(sp.phase==scanner_params::PHASE_INIT){
        static scanner_params::scanner_info info;
        info.scanner     = scan_sha1;
        info.name        = "sha1";
        info.author      = "Simson L. Garfinkel";
        info.description = "Compute the SHA1 of every sbuf.";
        info.url         = "https://digitalcorpora.org/bulk_extractor";
        info.scanner_version = "1.0.0";
        info.pathPrefix  = "SHA1";
        //info.flags       = scanner_params::scanner_info::SCANNER_DEFAULT_DISABLED;
        // specify the featur recorders we want.
        // Note that the feature recorder does not need to be the same name as the scanner
        info.feature_names.insert("sha1_bufs");

        histogram_def hd("sha1","^(.....)","nope",histogram_def::flags_t(true,false));

        info.histogram_defs.insert(hd);
        sp.register_info(info);
        return;
    }

    if(sp.phase==scanner_params::PHASE_SCAN){
        auto hexdigest = dfxml::sha1_generator::hash_buf(sp.sbuf.buf,sp.sbuf.bufsize).hexdigest();

        /* Perhaps we want to cache getting the recorders? */
        feature_recorder &sha1_recorder = sp.named_feature_recorder("sha1_bufs");
        sha1_recorder.write(sp.sbuf.pos0, hexdigest, ""); // write the hash with no context

	static const std::string hash0("<hashdigest type='SHA1'>");
	static const std::string hash1("</hashdigest>");
	if (sp.sxml){
            (*sp.sxml) << hash0 << hexdigest << hash1;
        }
	return;
    }
}
