/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdarg>
#include <regex>

#include "feature_recorder_file.h"
#include "feature_recorder_set.h"
#include "unicode_escape.h"
#include "utils.h"
#include "word_and_context_list.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 65536
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef DEBUG_PEDANTIC
#define DEBUG_PEDANTIC 0x0001 // check values more rigorously
#endif

/**
 * Create a feature recorder object. Each recorder records a certain
 * kind of feature.  Features are stored in a file. The filename is
 * permutated based on the total number of threads and the current
 * thread that's recording. Each thread records to a different file,
 * and thus a different feature recorder, to avoid locking
 * problems.
 *
 * @param feature_recorder_set &fs - common information for all of the feature recorders
 * @param name         - the name of the feature being recorded.
 */

/*
 * constructor. Not it is called with the feature_recorder_set to which it belongs.
 *
 */
// TODO - make it register itself with the feature recorder set. and do the stuff that's in init.
feature_recorder_file::feature_recorder_file(class feature_recorder_set& fs_, const feature_recorder_def def_)
    : feature_recorder(fs_, def_) {
    /* If the feature recorder set is disabled, just return. */
    if (fs.flags.disabled) return;

    /* If there is no output directory, just return */
    // if ( fs.get_outdir() == scanner_config::NO_OUTDIR ) return;

    // if ( fs.flag_set(feature_recorder_set::DISABLE_FILE_RECORDERS)) return;

    /* Open the file recorder for output.
     * If the file exists, seek to the end and find the last complete line, and start there.
     */
    const std::lock_guard<std::mutex> lock(Mios);
    std::filesystem::path fname = fname_in_outdir("", NO_COUNT);
    ios.open(fname.c_str(), std::ios_base::in | std::ios_base::out | std::ios_base::ate);
    if (ios.is_open()) { // opened existing file
        ios.seekg(0L, std::ios_base::end);
        while (ios.is_open()) {
            /* Get current position.
             * If we are at the beginning, just return.
             */
            if (int(ios.tellg()) == 0) {
                ios.seekp(0L, std::ios_base::beg);
                return;
            }
            /* backup one character and see if the next character is a newline */
            ios.seekg(-1, std::ios_base::cur);              // backup to once less than the end of the file
            if (ios.peek() == '\n') {                       // we are finally on the \n
                ios.seekg(1L, std::ios_base::cur);          // move the getting one forward
                ios.seekp(ios.tellg(), std::ios_base::beg); // put the putter at the getter location
                // count_ = 1;                            // greater than zero
                return;
            }
        }
    }
    /* Just open the stream for output */
    ios.open(fname.c_str(), std::ios_base::out);
    if (!ios.is_open()) {
        std::cerr << "*** feature_recorder_file::open Cannot open feature file for writing " << fname << ":"
                  << strerror(errno) << "\n";
        throw std::invalid_argument("cannot open feature file for writing");
    }
}

/* Exiting: make sure that the stream is closed.
 */
feature_recorder_file::~feature_recorder_file() {
    if (ios.is_open()) { ios.close(); }
}

void feature_recorder_file::banner_stamp(std::ostream& os, const std::string& header) const {
    int banner_lines = 0;
    if (fs.banner_filename.size() > 0) {
        std::ifstream i(fs.banner_filename.c_str());
        if (i.is_open()) {
            std::string line;
            while (getline(i, line)) {
                if (line.size() > 0 && ((*line.end() == '\r') || (*line.end() == '\n'))) {
                    line.erase(line.end()); /* remove the last character while it is a \n or \r */
                }
                os << "# " << line << "\n";
                banner_lines++;
            }
            i.close();
        }
    }
    if (banner_lines == 0) { os << "# BANNER FILE NOT PROVIDED (-b option)\n"; }

    os << bulk_extractor_version_header;
    os << "# Feature-Recorder: " << name << "\n";

    if (!fs.get_input_fname().empty()) { os << "# Filename: " << fs.get_input_fname().string() << "\n"; }
    if (feature_recorder_file::debug != 0) {
        os << "# DEBUG: " << debug << " (";
        if (feature_recorder_file::debug & DEBUG_PEDANTIC) os << " DEBUG_PEDANTIC ";
        os << ")\n";
    }
    os << header;
}

/* statics */
const std::string feature_recorder_file::feature_file_header("# Feature-File-Version: 1.1\n");
const std::string feature_recorder_file::histogram_file_header("# Histogram-File-Version: 1.1\n");
const std::string feature_recorder_file::bulk_extractor_version_header("# " PACKAGE_NAME "-Version: " PACKAGE_VERSION
                                                                       "\n");

void feature_recorder_file::flush()    { ios.flush(); }
void feature_recorder_file::shutdown() { ios.flush(); }

/**
 * We now have three kinds of histograms:
 * 1 - Traditional post-processing histograms specified by the histogram library
 *   1a - feature-file based traditional ones
 *   1b - SQL-based traditional ones.
 * 2 - In-memory histograms (used primarily by beapi)
 */

/****************************************************************
 *** WRITING SUPPORT
 ****************************************************************/

/* write to the file.
 * this is the only place where writing happens.
 * so it's an easy place to do utf-8 validation in debug mode.
 */
void feature_recorder_file::write0(const std::string& str) {
    feature_recorder::write0(str); // call super class
    if (fs.flags.pedantic && (utf8::find_invalid(str.begin(), str.end()) != str.end())) {
        std::cerr << "******************************************\n";
        std::cerr << "feature recorder: " << name << "\n";
        std::cerr << "invalid utf-8 in write: " << str << "\n";
        throw std::runtime_error("Invalid utf-8 in write");
    }

    /* this is where the writing happens. lock the output and write */
    if (fs.flags.disabled) { return; }

    const std::lock_guard<std::mutex> lock(Mios);
    if (ios.is_open()) {
        /* If there is no banner, add it */
        if (ios.tellg() == 0) {
            banner_stamp(ios, feature_file_header);
        }

        /* Output the feature */
        ios << str << '\n';
        if (ios.fail()) {
            throw std::runtime_error("Disk full. Free up space and re-restart.");
        }
    }
}

/**
 * Combine the pos0, feature and context into a single line and write it to the feature file.
 * This must be called for every feature
 *
 * @param feature - The feature, which is valid UTF8 (but may not be exactly the bytes on the disk)
 * @param context - The context, which is valid UTF8 (but may not be exactly the bytes on the disk)
 *
 * Interlocking is done in write().
 */

void feature_recorder_file::write0(const pos0_t& pos0, const std::string& feature, const std::string& context) {
    feature_recorder::write0(pos0, feature, context); // call super to increment counter
    if (fs.flags.disabled) { return; }
    std::stringstream ss;
    ss << pos0.shift(fs.offset_add).str() << '\t' << feature;
    if ((def.flags.no_context == false) && (context.size() > 0)) {
        ss << '\t' << context;
    }
    write0(ss.str());                                 // and do the actual write
}

/****************************************************************
 *** FILE-BASED HISOTGRAMS
 ****************************************************************/

/** Flush a specific histogram.
 * This is how the feature recorder triggers the histogram to be written.
 */
void feature_recorder_file::histogram_flush(AtomicUnicodeHistogram& h) {
    /* Get the next filename */
    auto fname = fname_in_outdir(h.def.suffix, NEXT_COUNT);
    std::fstream hfile;
    hfile.open(fname.c_str(), std::ios_base::out);
    if (!hfile.is_open()) {
        throw std::runtime_error("Cannot open feature histogram file " + fname.string());
    }

    bool first = true;
    auto r = h.makeReport(0); // sorted and clear
    for(const auto &it : r){
        if ( first ) {
            banner_stamp( hfile, histogram_file_header );
            first = false;
        }
        hfile << it;
    }
    hfile.close();
}
