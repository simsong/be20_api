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
#include "atomic_set_map.h"

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
    /* default copy construction and assignment are meaningless and not implemented */
    feature_recorder(const feature_recorder &)=delete;
    feature_recorder &operator=(const feature_recorder &)=delete;

public:;
    /* The main public interface:
     * Note that feature_recorders exist in a feature_recorder_set and have a name.
     */
    feature_recorder(class feature_recorder_set &fs, const std::string &name);
    virtual        ~feature_recorder();

    /* Main state variables */
    class  feature_recorder_set &fs; // the set in which this feature_recorder resides
    const  std::string name {};      // name of this feature recorder.
    bool   validateOrEscapeUTF8_validate { true };     // should we validate or escape UTF8?

    virtual const std::string &get_outdir() const;      // cannot be inline becuase it accesses fs

    /* State variables for this feature recorder */
    std::atomic<size_t>       context_window {0};      // context window for this feature recorder
    std::atomic<int64_t>      features_written {0};

    /**
     * \name Flags that control scanners
     * @{
     * These flags control scanners.  Set them with flag_set().
     */
    /** Disable this recorder. */
    struct flags_t {
        bool  disabled;           // feature recorder is Disabled
        bool  no_context;         // Do not write context.
        bool  no_stoplist; // Do not honor the stoplist/alertlist.
        bool  no_alertlist; // Do not honor the stoplist/alertlist.
    /**
     * Normally feature recorders automatically quote non-UTF8 characters
     * with \x00 notation and quote "\" as \x5C. Specify FLAG_NO_QUOTE to
     * disable this behavior.
     */
        bool  no_quote;            // do not escape UTF8 codes

    /**
     * Use this flag the feature recorder is sending UTF-8 XML.
     * non-UTF8 will be quoted but "\" will not be escaped.
     */
        bool  xml;                 // will be sending XML

    /**
     * histogram support.
     */
        bool no_features; // do not record features (just memory histogram)

    } flags;
    /** @} */
    static constexpr int max_histogram_files = 10;  // don't make more than 10 files in low-memory conditions

    static const std::string MAX_DEPTH_REACHED_ERROR_FEATURE;
    static const std::string MAX_DEPTH_REACHED_ERROR_CONTEXT;
    static std::string quote_string(const std::string &feature); // turns unprintable characters to octal escape
    static std::string unquote_string(const std::string &feature); // turns octal escape back to binary characters
    static std::string extract_feature(const std::string &line); // remove the feature from a feature file line

    /* Support for hashing */
    //typedef      std::string (*hashing_function_t)( const sbuf_t &sbuf); // returns a hex value
    std::string hash(const sbuf_t &sbuf); // hash an sbuf with the current hashing function and return it



    /* File management */
    // returns a feature recorder filename in the outdir with an optional suffix before the file extension.
    // The suffix is used for histograms.
    const std::string fname_suffix(std::string suffix) const;
    const std::string fname_in_outdir(std::string suffix) const; // returns the name of a dir in the outdir

    // These must only be changed in the main thread at the start of program execution:

    /* Methods to write.
     * write() is the basic write - you say where, and it does it.
     * write_buf() writes from a position within the buffer, with context.
     *             It won't write a feature that starts in the margin.
     * pos0 gives the location and prefix for the beginning of the buffer
     */

    /**
     * write0() actually does the writing to the file.
     * It must be threadsafe (either uses locks or goes to a DBMS)
     * Callers therefore do not need locks.
     */
    virtual void write0(const std::string &str);
    virtual void write0(const pos0_t &pos0,const std::string &feature,const std::string &context);

    // higher-level write a feature and its context; the feature may be in the context, but doesn't need to be.
    // entries processed by write below will be processed by histogram system
    virtual void write(const pos0_t &pos0,const std::string &feature,const std::string &context);

    // write a feature located at a given place within an sbuf.
    // Context is written automatically
    virtual void write_buf(const sbuf_t &sbuf, size_t pos, size_t len); /* writes with context */

    /**
     * support for writing features
     */

    // quote feature and context if they are not valid utf8 and if it is important to do so
    void quote_if_necessary(std::string &feature,std::string &context);

    // printf() prints to the feature file.
    //virtual void printf(const char *fmt_,...) __attribute__((format(printf, 2, 3)));
    //
    // write a feature and its context; the feature may be in the context, but doesn't need to be.
    /****************************************************************
     *** External entry points.
     ****************************************************************/

    /**
     * support for carving.
     * Carving writes the filename to the feature file; the context is the file's hash using the provided function.
     * Automatically de-duplicates.
     * Carving is implemented in the abstract class becuase all feature recorders have access to it.
     * Carving should not be passed on when chaining.
     */
    enum carve_mode_t {
        CARVE_NONE=0,
        CARVE_ENCODED=1,
        CARVE_ALL=2};

    std::atomic<int64_t>   carved_files {0};        // starts at 0; gets incremented by carve();
    atomic_set<std::string>     carve_cache {}; // hashes of files that have been cached, so the same file is not carved twice
    std::string  do_not_carve_encoding {};            // do not carve files with this encoding.
    static const std::string CARVE_MODE_DESCRIPTION;
    std::atomic<carve_mode_t> carve_mode { CARVE_ENCODED};

    // Carve data or a record to a file; returns filename of carved file or empty string if nothing carved
    // if mtime!=0, set the file's mtime to be mtime
    virtual std::string carve(const sbuf_t &sbuf, const std::string &ext, time_t mtime); // appended to forensic path
    // Carve a record; returns filename of records file or empty string if nothing carved
    //virtual std::string carve_records(const sbuf_t &sbuf, size_t pos, size_t len, const std::string &name);
    // Write a data;
    virtual std::string write_data(unsigned char *data, size_t len, const std::string &filename);

    // Set the time of the carved file to iso8601 file
    //virtual void set_carve_mtime(const std::string &fname, const std::string &mtime_iso8601);
};

#endif
