
#ifndef FEATURE_RECORDER_H
#define FEATURE_RECORDER_H

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

#include "atomic_set_map.h"
#include "pos0.h"
#include "sbuf.h"
#include "histogram.h"

#if defined(HAVE_SQLITE3_H)
#include <sqlite3.h>
#endif

/**
 * \addtogroup bulk_extractor_APIs
 * @{
 */

/**
 * feature_recorder.h:
 *
 * System for recording features from the scanners into the feature files.
 *
 * There is one feature_recorder per feature file. It is used both to record
 * the features and to perform the histogram calculation.
 * (That should probably be moved to a different class.) It also also previously
 * had the ability to do a merge sort, but we took that out because it was
 * not necessary.
 *
 * The feature recorders can also check the global alert_list to see
 * if the feature should be written to the alert file. It's opened on
 * demand and immediately flushed and closed.  A special mutex is used
 * to protect it.
 *
 * Finally, the feature recorder supports the global stop_list, which
 * is a list of features that are not written to the main file but are
 * written to a stop list.  That is implemented with a second
 * feature_recorder.
 *
 * There is one feature_recorder_set per process.
 * The file assumes that bulk_extractor.h is being included.
 */

class feature_recorder {
    static void truncate_at(std::string &line, char ch) {
        size_t pos = line.find(ch);
        if(pos != std::string::npos) line.resize(pos);
    };

public:;

    /* The main public interface:
     * Note that feature_recorders exist in a feature_recorder_set and have a name.
     */
    feature_recorder(class feature_recorder_set &fs, const std::string &name);
    virtual        ~feature_recorder();
    virtual void   set_flag(uint32_t flags_);
    virtual void   unset_flag(uint32_t flags_);
    void           enable_memory_histograms();              // only called from feature_recorder_set
    bool           flag_set(uint32_t f)    const {return flags & f;}
    bool           flag_notset(uint32_t f) const {return !(flags & f);}
    uint32_t       get_flags()             const {return flags;}
    virtual const std::string &get_outdir() const; // cannot be inline becuase it accesses fs

    class  feature_recorder_set &fs;              // the set in which this feature_recorder resides
    const  std::string name {};                   // name of this feature recorder
    bool   validateOrEscapeUTF8_validate { true };     // should we validate or escape the HTML?


    /* default copy construction and assignment are meaningless and not implemented */
    static std::thread::id main_thread_id;
    feature_recorder(const feature_recorder &)=delete;
    feature_recorder &operator=(const feature_recorder &)=delete;

    static uint32_t debug;              // are we debugging?
    uint32_t        flags {0};          // flags for this feature recorder
    /****************************************************************/

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
    static  void set_debug( uint32_t ndebug ){ debug=ndebug; }
    typedef std::string offset_t;

    /**
     * \name Flags that control scanners
     * @{
     * These flags control scanners.  Set them with set_flag().
     */
    /** Disable this recorder. */
    static const int FLAG_DISABLED         = 0x01;      // feature recorder is Disabled
    static const int FLAG_NO_CONTEXT       = 0x02;      // Do not write context.
    static const int FLAG_NO_STOPLIST      = 0x04;      // Do not honor the stoplist/alertlist.
    static const int FLAG_NO_ALERTLIST     = 0x08;      // Do not honor the stoplist/alertlist.
    /**
     * Normally feature recorders automatically quote non-UTF8 characters
     * with \x00 notation and quote "\" as \x5C. Specify FLAG_NO_QUOTE to
     * disable this behavior.
     */
    static const int FLAG_NO_QUOTE         = 0x10;         // do not escape UTF8 codes

    /**
     * Use this flag the feature recorder is sending UTF-8 XML.
     * non-UTF8 will be quoted but "\" will not be escaped.
     */
    static const int FLAG_XML              = 0x20;         // will be sending XML

    /**
     * histogram support.
     */
    static const uint32_t FLAG_NO_FEATURES     = 0x40;  // do not record features (just memory histogram)
    static const uint32_t FLAG_NO_FEATURES_SQL = 0x80;  // do not write features to SQL

    /** @} */
    static const int max_histogram_files = 10;  // don't make more than 10 files in low-memory conditions
    static const std::string histogram_file_header;
    static const std::string feature_file_header;
    static const std::string bulk_extractor_version_header;

    // These must only be changed in the main thread at the start of program execution:
    static uint32_t    opt_max_context_size;
    static uint32_t    opt_max_feature_size;
    static int64_t     offset_add;          // added to every reported offset, for use with hadoop
    static std::string banner_file;         // banner for top of every file
    static std::string extract_feature(const std::string &line);

private:
    std::string  ignore_encoding {};            // encoding to ignore for carving
    std::fstream ios {};                        // where features are written

#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    struct besql_stmt *bs {nullptr};            // prepared beapi sql statement
#endif

protected:;
    histogram_defs_t      histogram_defs {};    // histograms that are to be created for this feature recorder
protected:
    std::atomic<int64_t>      count_ {};                     /* number of records written */

    mutable std::mutex Mf {};      // protects the file  & file_number_
    mutable std::mutex Mr {};                     // protects the redlist
    mhistograms_t mhistograms {};               // the memory histograms, if we are using them
    uint64_t      mhistogram_limit {};          // how many we want (per feature recorder limit, rather than per histogram)


    feature_recorder       *stop_list_recorder {nullptr}; // where stopped features get written
    std::atomic<int64_t>   file_number_ {};        // starts at 0; gets incremented by carve();
    carve_cache_t          carve_cache {};
    size_t                 context_window {0};      // context window
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
    void set_context_window(size_t win)                          { MAINTHREAD(); context_window =  win;}
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
    virtual void dump_histogram(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const;
    typedef void (*xml_notifier_t)(const std::string &xmlstring);
    virtual void dump_histograms(void *user,feature_recorder::dump_callback_t cb, xml_notifier_t xml_error_notifier) const;

    /* Methods to get info */
    uint64_t count() const { return count_; }

    /* Methods to write.
     * write() is the basic write - you say where, and it does it.
     * write_buf() writes from a position within the buffer, with context.
     *             It won't write a feature that starts in the margin.
     * pos0 gives the location and prefix for the beginning of the buffer
     */

    /**
     * write() actually does the writing to the file.
     * It uses locks and is threadsafe.
     * Callers therefore do not need locks.
     */
    virtual void write(const std::string &str);

    /**
     * support for writing features
     */

    void quote_if_necessary(std::string &feature,std::string &context);

    // only virtual functions may be called by plug-ins
    // printf() prints to the feature file.
    virtual void printf(const char *fmt_,...) __attribute__((format(printf, 2, 3)));
    //
    // write a feature and its context; the feature may be in the context, but doesn't need to be.
    // write() calls write0() after histogram, quoting, and stoplist processing
    // write0() calls write0_sqlite3() if sqlwriting is enabled
    virtual void write0(const pos0_t &pos0,const std::string &feature,const std::string &context);
private:
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    virtual void write0_sqlite3(const pos0_t &pos0,const std::string &feature,const std::string &context);
#endif
    static const char *db_insert_stmt;
public:

    /****************************************************************
     *** External entry points.
     ****************************************************************/

    // write a feature and its context; the feature may be in the context, but doesn't need to be.
    // entries processed by write below will be processed by histogram system
    virtual void write(const pos0_t &pos0,const std::string &feature,const std::string &context);

    // write a feature located at a given place within an sbuf.
    // Context is written automatically
    virtual void write_buf(const sbuf_t &sbuf,size_t pos,size_t len); /* writes with context */

    /**
     * support for carving.
     * Carving writes the filename to the feature file; the context is the file's hash using the provided function.
     * Automatically de-duplicates.
     */
    enum carve_mode_t {
        CARVE_NONE=0,
        CARVE_ENCODED=1,
        CARVE_ALL=2};
#define CARVE_MODE_DESCRIPTION "0=carve none; 1=carve encoded; 2=carve all"
    carve_mode_t carve_mode { CARVE_ENCODED};
    typedef      std::string (*hashing_function_t)( const sbuf_t &sbuf); // returns a hex value
    void         set_carve_mode(carve_mode_t aMode){ MAINTHREAD();carve_mode=aMode;}

    // Carve a file; returns filename of carved file or empty string if nothing carved
    virtual std::string carve(const sbuf_t &sbuf,size_t pos,size_t len,
                              const std::string &ext); // appended to forensic path
    // Carve a record; returns filename of records file or empty string if nothing carved
    virtual std::string carve_records(const sbuf_t &sbuf, size_t pos, size_t len,
                                      const std::string &name);
    // Write a data;
    virtual std::string write_data(unsigned char *data, size_t len, const std::string &filename);

    // Set the time of the carved file to iso8601 file
    virtual void set_carve_mtime(const std::string &fname, const std::string &mtime_iso8601);
};




/** @} */

#endif
