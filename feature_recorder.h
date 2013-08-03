/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef FEATURE_RECORDER_H
#define FEATURE_RECORDER_H

/**
 * \addtogroup bulk_extractor_APIs
 * @{
 */

/**
 * feature_recorder.h:
 *
 * System for recording features from the scanners into the feature files.
 *
 * This module defines three classes:
 * class feature_recorder - the individual recorders.
 * class feature_recorder_set - the set of recorders.
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
 
using namespace std;
#include <string>
#include <cstdarg>
#include <fstream>
#include <set>
#include <cassert>

#include <pthread.h>

#include "regex.h"
#include "cppmutex.h"

class feature_recorder {
    static uint32_t debug;              // are we debugging?
    static pthread_t main_threadid;     // main threads ID
    static void MAINTHREAD(){// function that can only be called from main thread
        assert(main_threadid==pthread_self());
    };                
    uint32_t flags;                     // flags for this feature recorder
    bool histogram_enabled;             /* do we automatically histogram? */
    /*** neither copying nor assignment is implemented                         ***
     *** We do this by making them private constructors that throw exceptions. ***/
    class not_impl: public exception {
        virtual const char *what() const throw() {
            return "copying feature_recorder objects is not implemented.";
        }
    };
    feature_recorder(const feature_recorder &fr) __attribute__((__noreturn__)) :
        flags(0),histogram_enabled(false),
        outdir(),input_fname(),name(),ignore_encoding(),count_(0),ios(),context_window_before(),context_window_after(),Mf(),Mr(),
        stop_list_recorder(0),file_number(0),carve_mode()
        {
        throw new not_impl();
    }
    const feature_recorder &operator=(const feature_recorder &fr){ throw new not_impl(); }
    /****************************************************************/

public:
    static void set_main_threadid(){main_threadid=pthread_self();};             // set the main 
    static void set_debug(uint32_t ndebug){debug=ndebug;}
    typedef string offset_t;

    /**
     * \name Flags that control scanners
     * @{
     * These flags control scanners.  Set them with set_flag().
     */
    /** Disable this recorder. */
    static const int FLAG_DISABLED=0x01;        // Disabled
    static const int FLAG_NO_CONTEXT=0x02;        // Do not write context.
    static const int FLAG_NO_STOPLIST=0x04;      // Do not honor the stoplist/alertlist.
    static const int FLAG_NO_ALERTLIST=0x04;    // Do not honor the stoplist/alertlist.
    /**
     * Normally feature recorders automatically quote non-UTF8 characters
     * with \x00 notation and quote "\" as \x5C. Specify FLAG_NO_QUOTE to
     * disable this behavior.
     */
    static const int FLAG_NO_QUOTE=0x08;        // do not escape UTF8 codes

    /**
     * Use this flag the feature recorder is sending UTF-8 XML.
     * non-UTF8 will be quoted but "\" will not be escaped.
     */
    static const int FLAG_XML    = 0x10; // will be sending XML

    /** @} */
    static const int max_histogram_files = 10;  // don't make more than 10 files in low-memory conditions
    static const std::string histogram_file_header;
    static const std::string feature_file_header;
    static const std::string bulk_extractor_version_header;

    // These must only be changed in the main thread:
    static uint32_t opt_max_context_size;
    static uint32_t opt_max_feature_size;
    static int64_t offset_add;          // added to every reported offset, for use with hadoop
    static std::string banner_file;          // banner for top of every file
    static std::string extract_feature(const std::string &line);

    feature_recorder(string outdir,string input_fname,string name);
    virtual ~feature_recorder();

    virtual void set_flag(uint32_t flags_){flags|=flags_;}

    static size_t context_window_default; // global option
    const std::string outdir;                // where output goes (could be static, I guess 
    const std::string input_fname;           // image we are analyzing
    const std::string name;                  /* name of this feature recorder */
private:
    std::string ignore_encoding;       // encoding to ignore for carving
    int64_t count_;                      /* number of records written */
    std::fstream ios;                   /* where features are written */
    size_t context_window_before;       // context window
    size_t context_window_after;       // context window

    cppmutex Mf;                        /* protects the file */
    cppmutex Mr;                        /* protects the redlist */
public:

    /* these are not threadsafe and should only be called in startup */
    void set_context_window(size_t win){
        MAINTHREAD();
        context_window_before = win;
        context_window_after = win;
    }
    void set_context_window_before(size_t win){ MAINTHREAD(); context_window_before = win;}
    void set_context_window_after(size_t win){ MAINTHREAD(); context_window_after = win; }
    void set_carve_ignore_encoding(const std::string &encoding){ MAINTHREAD();ignore_encoding = encoding;}
    /* End non-threadsafe */

    void   banner_stamp(std::ostream &os,const std::string &header); // stamp BOM, banner, and header

    /* where stopped items (on stop_list or context_stop_list) get recorded: */
    class feature_recorder *stop_list_recorder; // where stopped features get written
    std::string fname_counter(string suffix);
    static std::string quote_string(const std::string &feature); // turns unprintable characters to octal escape
    static std::string unquote_string(const std::string &feature); // turns octal escape back to binary characters

    /* feature file management */
    void open();
    void close();                       
    void flush();
    void make_histogram(const class histogram_def &def);
    
    /* Methods to get info */
    uint64_t count(){return count_;}

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
    void write(const std::string &str);

    /**
     * support for writing features
     */

    // only virtual functions may be called by plug-ins
    // printf() prints to the feature file.
    virtual void printf(const char *fmt_,...) __attribute__((format(printf, 2, 3)));
    // 
    // write a feature and its context; the feature may be in the context, but doesn't need to be.
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
    int64_t      file_number;            /* starts at 0; gets incremented by carve(); for binning */
    carve_mode_t carve_mode;
    typedef      std::string (*hashing_function_t)(const sbuf_t &sbuf); // returns a hex value
    void         set_carve_mode(carve_mode_t aMode){MAINTHREAD();carve_mode=aMode;}

    // Carve a file; returns filename of carved file or empty string if nothing carved
    virtual std::string carve(const sbuf_t &sbuf,size_t pos,size_t len, 
                              const std::string &ext, // appended to forensic path
                              const struct be13::hash_def &hasher);
    // Set the time of the carved file to iso8601 file
    virtual void set_carve_mtime(const std::string &fname, const std::string &mtime_iso8601);

    /**
     * EXPERIMENTAL!
     * support for tagging blocks with their type.
     * typically 'len' is the sector size, but it need not be.
     */
    virtual void write_tag(const pos0_t &pos0,size_t len,const std::string &tagName);
    virtual void write_tag(const sbuf_t &sbuf,const std::string &tagName){
        write_tag(sbuf.pos0,sbuf.pagesize,tagName);
    }

};

/** @} */

#endif
