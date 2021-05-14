#ifndef FEATURE_RECORDER_H
#define FEATURE_RECORDER_H

#include <cinttypes>
#include <cassert>

#include <string>
#include <set>
#include <map>
#include <thread>
#include <iostream>
#include <fstream>
#include <atomic>
#include <memory>

#include "pos0.h"
#include "sbuf.h"
#include "atomic_set.h"
#include "histogram_def.h"
#include "atomic_unicode_histogram.h"

/**
 * \addtogroup bulk_extractor_APIs
 * @{
 */

/**
 * feature_recorder.h:
 *
 * System for recording features from the scanners into the feature files.
 *
 * feature_recorder_set - holds active feature recorders and shared state.
 *
 * There is one feature_recorder per feature file. It is used both to record
 * the features and to perform the in-memory histogram calculation.
 *
 * Global Alert List - The feature recorders can also check the global alert_list to see
 * if the feature should be written to the alert file. It's opened on
 * demand and immediately flushed and closed, so that changes are
 * immediately reflected in the file, even under windows.  A special
 * mutex is used to protect it.
 *
 * Global Stop List - Each the feature recorder supports the global
 * stop_list, which is a list of features that are not written to the
 * main file but are written to a stop list.  That is implemented with
 * a second feature_recorder.
 *
 * Histogram - New in BE2.0, the histograms are built on-the-fly as features are recorded.
 * If memory runs below the LOW_MEMORY_THRESHOLD defined in the feature_recorder_sert,
 * the largest feature recorder is written to disk and a new feature_recorder histogram is started.
 *
 * When the feature_recorder_set shuts down, all remaining histograms are written to the disk.
 * Then, if there is any case where multiple histogram files were written, a merge-sort is performed.
 */

struct feature_recorder_def {
    std::string name;                   // the name of the feature recorder
    /**
     * \name Flags that control scanners
     * @{
     * These flags control scanners.  Set them with flag_set().
     */
    /** Disable this recorder. */
    struct flags_t {
        bool  disabled;           // feature recorder is Disabled
        bool  no_context;         // Do not write context.
        bool  no_stoplist;        // Do not honor the stoplist/alertlist.
        bool  no_alertlist;       // Do not honor the stoplist/alertlist.
        bool  no_features;         // do not record features (used for in-memory histogram recorder)
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

    } flags {};

    /** @} */
};


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

    /* Instance variables available to subclasses: */
protected:
    class  feature_recorder_set &fs; // the set in which this feature_recorder resides
    virtual const std::string &get_outdir() const;      // cannot be inline because it accesses fs

public:;
    /* The main public interface:
     * Note that feature_recorders exist in a feature_recorder_set and have a name.
     */
    feature_recorder(class feature_recorder_set &fs, const feature_recorder_def def);
    virtual        ~feature_recorder();

    const  std::string name {};      // name of this feature recorder.
    feature_recorder_def::flags_t flags;
    bool   validateOrEscapeUTF8_validate { true };     // should we validate or escape UTF8?

    /* State variables for this feature recorder */
    std::atomic<size_t>       context_window {0};      // context window for this feature recorder
    std::atomic<int64_t>      features_written {0};


    /* Special tokens written into the file */
    static const std::string MAX_DEPTH_REACHED_ERROR_FEATURE;
    static const std::string MAX_DEPTH_REACHED_ERROR_CONTEXT;

    /* quoting functions */
    static std::string quote_string(const std::string &feature); // turns unprintable characters to octal escape
    static std::string unquote_string(const std::string &feature); // turns octal escape back to binary characters
    static std::string extract_feature(const std::string &line); // remove the feature from a feature file line

    /* Hash an SBuf using the current hasher. If we want to hash less than a sbuf, make a child sbuf */
    const std::string hash(const sbuf_t &sbuf) const;

    /* quote feature and context if they are not valid utf8 and if it is important to do so based on flags above.
     * note - modifies arguments!
     */
    void quote_if_necessary(std::string &feature,std::string &context) const;

    /* Called when the scanner set shutdown */
    virtual void shutdown();

    /* File management */

    /* fname_in_outdir(suffix, count):
     * returns a filename in the outdir in the format {feature_recorder}_{suffix}{count}.txt,
     * If count==NO_COUNT, count is omitted.
     * If count==NEXT_COUNT, create a zero-length file and return that file's name (we use the file system for atomic locks)
     */

    const std::string fname_in_outdir(std::string suffix, int count) const; // returns the name of a dir in the outdir
    enum count_mode_t {
        NO_COUNT=0,
        NEXT_COUNT = -1
    };

    /* Writing features */

    /**
     * write0() actually does the writing to the file.
     * It must be threadsafe (either uses locks or goes to a DBMS)
     * Callers therefore do not need locks.
     * It is only implemented in the subclasses.
     */
    virtual void write0(const std::string &str);
    virtual void write0(const pos0_t &pos0,const std::string &feature,const std::string &context);

    /* Methods used by scanners to write.
     * write() is the basic write - you say where, and it does it.
     * write_buf() writes from a position within the buffer, with context.
     *             It won't write a feature that starts in the margin.
     * pos0 gives the location and prefix for the beginning of the buffer
     *
     * higher-level write a feature and its context; the feature may be in the context, but doesn't need to be.
     * entries processed by write below will be processed by histogram system
     */
    virtual void write(const pos0_t &pos0,const std::string &feature,const std::string &context);

    /* write_buf():
     * write a feature located at a given place within an sbuf.
     * Context is written automatically
     */
    virtual void write_buf(const sbuf_t &sbuf, size_t pos, size_t len); /* writes with context */

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
        CARVE_ALL=2
    };

    std::atomic<carve_mode_t>  carve_mode { CARVE_ENCODED};
    std::atomic<int64_t>       carved_file_count {0}; // starts at 0; gets incremented by carve();
    atomic_set<std::string>    carve_cache {};        // hashes of files that have been cached, so the same file is not carved twice
    std::string  do_not_carve_encoding {};            // do not carve files with this encoding.
    static const std::string   CARVE_MODE_DESCRIPTION;
    static const std::string   NO_CARVED_FILE;

    // Carve data or a record to a file; returns filename of carved file or empty string if nothing carved
    // if mtime>0, set the file's mtime to be mtime
    // if offset>0, start with that offset
    // if offset>0, carve that many bytes, even into the margin (otherwise stop at the margin)
    virtual std::string carve_data(const sbuf_t &sbuf, const std::string &ext,
                                   const time_t mtime=0,
                                   const size_t offset=0,
                                   const size_t len=0 );
    virtual std::string carve_records(const sbuf_t &sbuf, size_t offset, size_t len, const std::string &name);

    /*
     * Each feature_recorder can have multiple histograms. They are generated on-the-fly for the file-based feature-recorder,
     * and generated in SQL for the SQL-based feature-recorder.
     */
    std::vector<std::unique_ptr<AtomicUnicodeHistogram>> histograms {};

    /* These must be specialized */
    virtual void histogram_flush(AtomicUnicodeHistogram &h) = 0; // flush a specific histogram
    virtual void histograms_add_feature(const std::string &feature); // propose a feature to all of the histograms

    virtual size_t histogram_count() { return histograms.size();}     // how many histograms it has
    virtual void histogram_add(const struct histogram_def &def); // add a new histogram
    virtual bool histogram_flush_largest();     // flushes largest histogram. returns false if no histogram could be flushed.
    virtual void histogram_flush_all(); // flushes all histograms
    //virtual void histogram_merge(const struct histogram_def &def); // merge sort on this histogram
    //virtual void histogram_merge_all();                            // merge sort on all histograms
};

#endif
