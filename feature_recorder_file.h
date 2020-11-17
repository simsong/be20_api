#ifndef FEATURE_RECORDER_FILE_H
#define FEATURE_RECORDER_FILE_H

#include <cinttypes>
#include <cassert>

#include <string>
#include <regex>
#include <set>
#include <map>
#include <thread>
#include <iostream>
#include <fstream>
#include <atomic>

#include "feature_recorder.h"
#include "atomic_set_map.h"
#include "pos0.h"
#include "sbuf.h"
#include "histogram.h"

class feature_recorder_file : public feature_recorder {
    static void truncate_at(std::string &line, char ch) {
        size_t pos = line.find(ch);
        if (pos != std::string::npos) line.resize(pos);
    };

public:;
    feature_recorder_file(class feature_recorder_set &fs, const std::string &name);
    virtual        ~feature_recorder_file();


    //void           enable_memory_histograms();              // only called from feature_recorder_set
    //static std::thread::id main_thread_id;

    //typedef int (dump_callback_t)(void *user,const feature_recorder &fr,const histogram_def &def,
    //const std::string &feature,const uint64_t &count);
    //static  void set_debug( uint32_t ndebug ){ debug=ndebug; }
    //typedef std::string offset_t;

private:
    std::mutex   Mios {};                        // mutex for IOS
    std::fstream ios {};                        // where features are written

    /*
     * histograms for this feature_recorder, populated by the scanners.
     */
    histogram_defs_t      histogram_defs {};

    mhistograms_t mhistograms {}; // the memory histograms, if we are using them
    std::atomic<uint64_t>      mhistogram_limit {}; // how many we want (per feature recorder limit, rather than per histogram)
    void   banner_stamp(std::ostream &os,const std::string &header) const; // stamp banner, and header
#if 0
    uint64_t file_number_add(uint64_t i){
        return file_number_.fetch_add(i) + i;
    }
#endif
    static std::string quote_string(const std::string &feature); // turns unprintable characters to octal escape
    static std::string unquote_string(const std::string &feature); // turns octal escape back to binary characters

    static const std::string histogram_file_header;
    static const std::string feature_file_header;
    static const std::string bulk_extractor_version_header;


public:
    /* these are not threadsafe and should only be called in startup */
    //void set_carve_ignore_encoding( const std::string &encoding ){ MAINTHREAD();ignore_encoding = encoding;}
    /* End non-threadsafe */

    // add i to file_number and return the result
    // fetch_add() returns the original number


    /* where stopped items (on stop_list or context_stop_list) get recorded:
     * Cannot be made inline becuase it accesses fs.
     */
    virtual const std::string hash(const unsigned char *buf, size_t bufflen); // hash a block with the hasher
    virtual void write(const std::string &str);

    /* feature file management */
    //virtual void open();
    //virtual void close();
    //virtual void flush();
#if 0
    static  int  dump_callback_test(void *user,const feature_recorder &fr,
                                    const std::string &str,const uint64_t &count); // test callback for you to use!

    /* TK: The histogram_def should be provided at the beginning, so it can be used for in-memory histograms.
     * The callback needs to have the specific atomic set as the callback as well.
     */
    virtual void add_histogram(const histogram_def &def); // adds a histogram to process
#endif

    //virtual void dump_histogram_file(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
    virtual size_t count_histograms() const;
    //virtual void dump_histogram(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
    typedef void (*xml_notifier_t)(const std::string &xmlstring);
    //virtual void dump_histograms(void *user,feature_recorder::dump_callback_t cb, xml_notifier_t xml_error_notifier) const;

    /* Methods to get info */
    //uint64_t count() const { return count_; }

};




/** @} */

#endif
