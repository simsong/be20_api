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

#if defined(HAVE_SQLITE3_H)
#include <sqlite3.h>
#endif

class feature_recorder_file : public feature_recorder {
    static void truncate_at(std::string &line, char ch) {
        size_t pos = line.find(ch);
        if(pos != std::string::npos) line.resize(pos);
    };

public:;

    void           enable_memory_histograms();              // only called from feature_recorder_set

    //static std::thread::id main_thread_id;
    //static uint32_t debug;              // are we debugging?

public:
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    struct besql_stmt {
        besql_stmt(const besql_stmt &)=delete;
        besql_stmt &operator=(const besql_stmt &)=delete;
        std::mutex         Mstmt {};
        sqlite3_stmt *stmt {};      // the prepared statement
        besql_stmt(sqlite3 *db3,const char *sql);
        virtual ~besql_stmt();
        void insert_feature(const pos0_t &pos, // insert it into this table!
                            const std::string &feature,const std::string &feature8, const std::string &context);
    };
#endif

    typedef int (dump_callback_t)(void *user,const feature_recorder &fr,const histogram_def &def,
                                  const std::string &feature,const uint64_t &count);
    //static  void set_debug( uint32_t ndebug ){ debug=ndebug; }
    //typedef std::string offset_t;

private:
    std::string  ignore_encoding {};            // encoding to ignore for carving
    std::fstream ios {};                        // where features are written

#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    struct besql_stmt *bs {nullptr};            // prepared beapi sql statement
#endif

    histogram_defs_t      histogram_defs {};    // histograms that are to be created for this feature recorder

    mutable std::mutex Mf {};     // protects the file  & file_number_
    mutable std::mutex Mr {};     // protects the redlist
    mhistograms_t mhistograms {}; // the memory histograms, if we are using them
    uint64_t      mhistogram_limit {}; // how many we want (per feature recorder limit, rather than per histogram)

    feature_recorder       *stop_list_recorder {nullptr}; // where stopped features get written
    std::atomic<int64_t>   file_number_ {};        // starts at 0; gets incremented by carve();
    carve_cache_t          carve_cache {};
public:
    /* these are not threadsafe and should only be called in startup */
    void MAINTHREAD() {
        assert( main_thread_id == std::this_thread::get_id() );
    }

    virtual void   set_memhist_limit(int64_t limit_) {
        MAINTHREAD();
        mhistogram_limit = limit_;
    };
    void set_stop_list_recorder(class feature_recorder *fr){ MAINTHREAD(); stop_list_recorder = fr; }
    void set_carve_ignore_encoding( const std::string &encoding ){ MAINTHREAD();ignore_encoding = encoding;}
    /* End non-threadsafe */

    // add i to file_number and return the result
    // fetch_add() returns the original number
    uint64_t file_number_add(uint64_t i){
        return file_number_.fetch_add(i) + i;
    }

    void   banner_stamp(std::ostream &os,const std::string &header) const; // stamp banner, and header

    /* where stopped items (on stop_list or context_stop_list) get recorded:
     * Cannot be made inline becuase it accesses fs.
     */
    std::string        fname_counter(std::string suffix) const;
    static std::string quote_string(const std::string &feature); // turns unprintable characters to octal escape
    static std::string unquote_string(const std::string &feature); // turns octal escape back to binary characters

    virtual const std::string hash(const unsigned char *buf, size_t bufflen); // hash a block with the hasher

    /* feature file management */
    virtual void open();
    virtual void close();
    virtual void flush();
    static  int  dump_callback_test(void *user,const feature_recorder &fr,
                                    const std::string &str,const uint64_t &count); // test callback for you to use!

    /* TK: The histogram_def should be provided at the beginning, so it can be used for in-memory histograms.
     * The callback needs to have the specific atomic set as the callback as well.
     */
    virtual void add_histogram(const histogram_def &def); // adds a histogram to process

    virtual void dump_histogram_file(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    virtual void dump_histogram_sqlite3(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
#endif
    virtual size_t count_histograms() const;
    virtual void dump_histogram(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
    typedef void (*xml_notifier_t)(const std::string &xmlstring);
    virtual void dump_histograms(void *user,feature_recorder::dump_callback_t cb, xml_notifier_t xml_error_notifier) const;

    /* Methods to get info */
    //uint64_t count() const { return count_; }

};




/** @} */

#endif
