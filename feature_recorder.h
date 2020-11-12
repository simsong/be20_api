/*
 * abstract feature recorder
 */

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

#include "pos0.h"
#include "sbuf.h"

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


/* New classes for a more object-oriented interface */
struct Feature {
    Feature(const pos0_t &pos_, const std::string &feature_, const std::string & context_):
        pos(pos_), feature(feature_), context(context_){};
    const pos0_t pos;
    const std::string feature;
    const std::string context;
};

/* Given an open feature_recorder, returns a class that iterates through it */
class FeatureReader {
    friend class feature_recorder;
    FeatureReader(const class feature_recorder &fr);
public:
    std::fstream infile;
    FeatureReader &operator++();
    FeatureReader begin();
    FeatureReader end();
    Feature operator*();
};


class feature_recorder {

public:;
    /* represent a feature */

    /* The main public interface:
     * Note that feature_recorders exist in a feature_recorder_set and have a name.
     */
    feature_recorder(class feature_recorder_set &fs, const std::string &name);
    virtual        ~feature_recorder();

    /* default copy construction and assignment are meaningless and not implemented */
    feature_recorder(const feature_recorder &)=delete;
    feature_recorder &operator=(const feature_recorder &)=delete;

    /* Main state variables */
    class  feature_recorder_set &fs; // the set in which this feature_recorder resides
    const  std::string name {};      // name of this feature recorder.
    bool   validateOrEscapeUTF8_validate { true };     // should we validate or escape UTF8?

    virtual const std::string &get_outdir() const;      // cannot be inline becuase it accesses fs

    /* State variables for this feature recorder */
    std::atomic<size_t>       context_window {0};      // context window for this feature recorder
    //void set_context_window(size_t win)                          { context_window =  win;}
    std::atomic<int64_t>      count {};                     /* number of records written */

    /* Flag System */
    uint32_t        flags {0};      // flags for this feature recorder
    virtual void   set_flag(uint32_t flags_);
    virtual void   unset_flag(uint32_t flags_);
    bool           flag_set(uint32_t f)    const {return flags & f;}
    bool           flag_notset(uint32_t f) const {return !(flags & f);}
    uint32_t       get_flags()             const {return flags;}

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
    static constexpr int max_histogram_files = 10;  // don't make more than 10 files in low-memory conditions
    static const std::string histogram_file_header;
    static const std::string feature_file_header;
    static const std::string bulk_extractor_version_header;


    static std::string MAX_DEPTH_REACHED_ERROR_FEATURE;
    static std::string MAX_DEPTH_REACHED_ERROR_CONTEXT;


    // These must only be changed in the main thread at the start of program execution:

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

    static const std::string CARVE_MODE_DESCRIPTION;
    std::atomic<carve_mode_t> carve_mode { CARVE_ENCODED};
    typedef      std::string (*hashing_function_t)( const sbuf_t &sbuf); // returns a hex value
    ;;void         set_carve_mode(carve_mode_t aMode){ carve_mode=aMode;}

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

#endif
