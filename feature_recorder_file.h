#ifndef FEATURE_RECORDER_FILE_H
#define FEATURE_RECORDER_FILE_H

#include "config.h"

#include <cassert>
#include <cinttypes>

#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <mutex>

#include "feature_recorder.h"
#include "pos0.h"
#include "sbuf.h"

class feature_recorder_file : public feature_recorder {
public:
    inline static const std::string feature_file_header   {"# Feature-File-Version: 1.1\n"};
    inline static const std::string histogram_file_header {"# Histogram-File-Version: 1.1\n"};
    inline static const std::string bulk_extractor_version_header {
        "# " PACKAGE_NAME "-Version: " PACKAGE_VERSION "\n"};

    static std::string unquote_string(const std::string& s);

    feature_recorder_file(class feature_recorder_set& fs, const feature_recorder_def def);
    virtual ~feature_recorder_file();
    virtual void flush() override;
    static const inline int MAX_HISTOGRAM_FILES = 10; // don't make more than 10 files in low-memory conditions
    static bool extract_feature_context(const std::string& line, std::string &feature, std::string &context); // extract feature and context, return true if successful
    static bool isodigit(uint8_t ch){
        return ch>='0' && ch<='7';
    }

private:
    std::mutex Mios{};  // mutex for IOS
    std::fstream ios{}; // where features are written

    void banner_stamp(std::ostream& os, const std::string& header) const; // stamp banner, and header

    //static const std::string histogram_file_header;
    //static const std::string feature_file_header;
    //static const std::string bulk_extractor_version_header;

    virtual void shutdown() override;

public:
    /* these are not threadsafe and should only be called in startup */
    // void set_carve_ignore_encoding( const std::string &encoding ){ MAINTHREAD();ignore_encoding = encoding;}
    /* End non-threadsafe */

    // add i to file_number and return the result
    // fetch_add() returns the original number

    /* where stopped items (on stop_list or context_stop_list) get recorded:
     * Cannot be made inline becuase it accesses fs.
     */
    // virtual const std::string hash(const uint8_t *buf, size_t bufflen); // hash a block with the hasher
    virtual void write0(const std::string& str) override;
    virtual void write0(const pos0_t& pos0, const std::string& feature, const std::string& context) override;

    /* feature file management */
#if 0
    static  int  dump_callback_test(void *user,const feature_recorder &fr,
                                    const std::string &str,const uint64_t &count); // test callback for you to use!

    /* TK: The histogram_def should be provided at the beginning, so it can be used for in-memory histograms.
     * The callback needs to have the specific atomic set as the callback as well.
     */
    virtual void add_histogram(const histogram_def &def); // adds a histogram to process
#endif

    virtual void histogram_add(const struct histogram_def& def) override;   // add a new histogram
    virtual void histograms_add_feature(const std::string& feature, const std::string& context);
    virtual void histogram_write0(AtomicUnicodeHistogram& h); // actually write this histogram
    virtual void histogram_write(AtomicUnicodeHistogram& h) override; // write this histogram

    // virtual void dump_histogram_file(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
    // virtual size_t count_histograms() const;
    // virtual void dump_histogram(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
    // typedef void (*xml_notifier_t)(const std::string &xmlstring);
    // virtual void dump_histograms(void *user,feature_recorder::dump_callback_t cb, xml_notifier_t xml_error_notifier)
    // const;

    /* Methods to get info */
    // uint64_t count() const { return count_; }
    // virtual void generate_histogram(std::ostream &os, const struct histogram_def &def);
};

/** @} */

#endif
