/**
 *
 * scan_sha1:
 * plug-in demonstration that shows how to write a simple plug-in scanner that calculates
 * the SHA1 of each sbuf. The hash is written to both the XML file and to the sha1 feature file.
 *
 * Don't use this in production systems! It has a histogram that isn't useful for most applications.
 */

#include "config.h" // needed for hash_t

#include <iostream>
#include <sys/types.h>

#include "dfxml_cpp/src/hash_t.h"
#include "scan_sha1_test.h"
#include "scanner_params.h"
#include "scanner_set.h"

feature_recorder *sha1_recorder  = nullptr;
void scan_sha1_test(struct scanner_params& sp) {
    if (sp.phase == scanner_params::PHASE_INIT) {
        /* Create a scanner_info block to register this scanner */
        auto info = new scanner_params::scanner_info(scan_sha1_test, "sha1_test");
        info->author = "Simson L. Garfinkel";
        info->description = "Compute the SHA1 of every sbuf.";
        info->url = "https://digitalcorpora.org/bulk_extractor";
        info->scanner_version = "1.0.0";
        info->pathPrefix = "SHA1";      // just use SHA1
        info->min_sbuf_size = 1;        // we can hash a single byte

        // specify the feature_records that the scanner wants.
        // Note that the feature recorder does not need to be the same name as the scanner
        // scanners may specify any number of feature recorders.
        info->feature_defs.push_back( feature_recorder_def("sha1_bufs") );

        // Note that histogram_defs is a set, so it's okay if this initialization routine is called twice,
        // the histogram only gets inserted once.
        histogram_def hd("test_histogram", "sha1_bufs", "^(.....)", "", "first5", histogram_def::flags_t(true, false));

        info->feature_defs.push_back(feature_recorder_def("sha1_bufs"));
        sp.info = info;
        return;
    }
    if (sp.phase == scanner_params::PHASE_INIT2) {
        sha1_recorder = &sp.ss.named_feature_recorder("sha1_bufs");
    }

    if (sp.phase == scanner_params::PHASE_SCAN) {
        auto hexdigest = sp.sbuf->hash();

        /* Perhaps we want to cache getting the recorders? */
        sha1_recorder->write(sp.sbuf->pos0, hexdigest, ""); // write the hash with no context

        static const std::string hash0("<hashdigest type='SHA1'>");
        static const std::string hash1("</hashdigest>");
        if (sp.sxml) { (*sp.sxml) << hash0 << hexdigest << hash1; }
        return;
    }
}
