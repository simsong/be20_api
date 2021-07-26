#ifndef FEATURE_RECORDER_H
#define FEATURE_RECORDER_H

#include <cassert>
#include <cinttypes>

#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <ctime>

#include "atomic_set.h"
#include "atomic_unicode_histogram.h"
#include "histogram_def.h"
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
    std::string name{}; // the name of the feature recorder
    /* These used to be static variables in the feature recorder class. They are more properly here */
    uint32_t max_context_size{1024 * 1024}; // 1024*1024 was in BE1.4
    uint32_t max_feature_size{1024 * 1024}; // 1024*1024 was in BE1.4

    /**
     * Carving support.
     *
     * Carving writes the filename to the feature file and the contents to a file in the directory.
     * The second field of the feature file is the file's hash using the provided function.
     * Automatically de-duplicates.
     * Carving is implemented in the abstract class so all feature recorders have access to it.
     * Carve mode parameters were previously set in the INIT phase of each of the scanners. Now it is set for
     * all feature recorers in the scanner_set after the scanners are initialized and the feature recorders are created.
     */
    enum carve_mode_t {
        CARVE_NONE = 0,    // don't carve at all, even if the carve function is called
        CARVE_ENCODED = 1, // only carve if the data being carved is encoded (e.g. BASE64 or GZIP is in path)
        CARVE_ALL = 2      // carve whenever the carve function is called.
    } default_carve_mode{CARVE_ALL};
    size_t min_carve_size {200};
    size_t max_carve_size {16*1024*1024};

    /**
     * \name Flags that control scanners
     * @{
     * These flags control scanners.  Set them with flag_set().
     */
    /** Disable this recorder. */
    struct flags_t {
        bool disabled{false};     // feature recorder is Disabled
        bool no_context{false};   // Do not write context.
        bool no_stoplist{false};  // Do not honor the stoplist/alertlist.
        bool no_alertlist{false}; // Do not honor the stoplist/alertlist.
        bool no_features{false};  // do not record features (used for in-memory histogram recorder)
                                  /**
                                   * Normally feature recorders automatically quote non-UTF8 characters
                                   * with \x00 notation and quote "\" as \x5C. Specify FLAG_NO_QUOTE to
                                   * disable this behavior.
                                   */
        bool no_quote{false};     // do not escape UTF8 codes

        /**
         * Use this flag the feature recorder is sending UTF-8 XML.
         * non-UTF8 will be quoted but "\" will not be escaped.
         */
        bool xml{false}; // will be sending XML

        bool operator==(const flags_t& a) const {
            return this->disabled == a.disabled && this->no_context == a.no_context &&
                   this->no_stoplist == a.no_stoplist && this->no_alertlist == a.no_alertlist &&
                   this->no_features == a.no_features && this->no_quote == a.no_quote && this->xml == a.xml;
        }
        bool operator!=(const flags_t& a) const { return !(*this == a); }
    } flags{};

    /** @} */
    feature_recorder_def(std::string name_) : name(name_) {
        assert(name != "");
    }                                // flagless constructor
    feature_recorder_def(std::string name_, flags_t flags_) : name(name_), flags(flags_) {
        assert(name != "");
    } // construct with flags
    bool operator==(const feature_recorder_def& a) const {
        return (this->name == a.name) && (this->flags == a.flags) && (this->default_carve_mode == a.default_carve_mode);
    };
    bool operator!=(const feature_recorder_def& a) const { return !(*this == a); }
};

/* New classes for a more object-oriented interface */
struct Feature {
    Feature(const pos0_t& pos_, const std::string& feature_, const std::string& context_)
        : pos(pos_), feature(feature_), context(context_){};
    Feature(std::string pos_, std::string feature_, std::string context_)
        : pos(pos_), feature(feature_), context(context_){};

    const pos0_t pos;
    const std::string feature;
    const std::string context;
};

/* Given an open feature_recorder, returns a class that iterates through it */
class FeatureReader {
    friend class feature_recorder;
    FeatureReader(const class feature_recorder& fr);

public:
    std::fstream infile;
    FeatureReader& operator++();
    FeatureReader begin();
    FeatureReader end();
    Feature operator*();
};

class feature_recorder {
    /* default copy construction and assignment are meaningless and not implemented */
    feature_recorder(const feature_recorder&) = delete;
    feature_recorder& operator=(const feature_recorder&) = delete;

    /* Instance variables available to subclasses: */
protected:
    class feature_recorder_set& fs;                         // the set in which this feature_recorder resides
    virtual const std::filesystem::path get_outdir() const; // cannot be inline because it accesses fs

public:
    ;
    /* The main public interface:
     * Note that feature_recorders exist in a feature_recorder_set and have a name.
     */
    feature_recorder(class feature_recorder_set& fs, const feature_recorder_def def);
    virtual ~feature_recorder();
    virtual void flush();

    const std::string name{}; // name of this feature recorder (copied out of def)
    feature_recorder_def def{"<NONAME>"};
    bool validateOrEscapeUTF8_validate{true}; // should we validate or escape UTF8?

    /* State variables for this feature recorder.
     * Defaults come from scanner_set's scanner_config.
     */
    std::atomic<size_t> context_window{0};
    std::atomic<int64_t> features_written{0};

    /* Special tokens written into the file */
    static inline const std::string MAX_DEPTH_REACHED_ERROR_FEATURE {"process_extract: MAX DEPTH REACHED"};
    static inline const std::string MAX_DEPTH_REACHED_ERROR_CONTEXT {""};
    static inline const std::string CACHED {"<CACHED>"}; // not written

    /* quoting functions */
    //static std::string quote_string(const std::string& feature);   // turns unprintable characters to octal escape
    //static std::string unquote_string(const std::string& feature); // turns octal escape back to binary characters
    //static std::string extract_feature(const std::string& line);   // remove the feature from a feature file line

    /* Hash an SBuf using the current hasher. If we want to hash less than a sbuf, make a child sbuf */
    const std::string hash(const sbuf_t& sbuf) const;

    /* quote feature and context if they are not valid utf8 and if it is important to do so based on flags above.
     * note - modifies arguments!
     */
    void quote_if_necessary(std::string& feature, std::string& context) const;

    /* Called when the scanner set shutdown */
    virtual void shutdown();

    /* File management */

    /* fname_in_outdir(suffix, count):
     * returns a filename in the outdir in the format {feature_recorder}_{suffix}{count}.txt,
     * If count==NO_COUNT, count is omitted.
     * If count==NEXT_COUNT, create a zero-length file and return that file's name (we use the file system for atomic
     * locks)
     */

    // returns the name of a dir in the outdir for this feature recorder
    const std::filesystem::path fname_in_outdir(std::string suffix, int count) const;
    enum count_mode_t { NO_COUNT = 0, NEXT_COUNT = -1 };

    /* Writing features */

    /**
     * write0() actually does the writing to the file.
     * It must be threadsafe (either uses locks or goes to a DBMS)
     * Callers therefore do not need locks.
     * It is only implemented in the subclasses.
     */
    virtual void write0(const std::string& str);
    virtual void write0(const pos0_t& pos0, const std::string& feature, const std::string& context);

    /* Methods used by scanners to write.
     * write() is the basic write - you say where, and it does it.
     * write_buf() writes from a position within the buffer, with context.
     *             It won't write a feature that starts in the margin.
     * pos0 gives the location and prefix for the beginning of the buffer
     *
     * higher-level write a feature and its context; the feature may be in the context, but doesn't need to be.
     * entries processed by write below will be processed by histogram system
     */
    virtual void write(const pos0_t& pos0, const std::string& feature, const std::string& context);

    /* write_buf():
     * write a feature located at a given place within an sbuf.
     * Context is written automatically
     */
    virtual void write_buf(const sbuf_t& sbuf, size_t pos, size_t len); /* writes with context */

    std::atomic<feature_recorder_def::carve_mode_t> carve_mode{feature_recorder_def::CARVE_ALL};
    std::atomic<size_t> min_carve_size {200};
    std::atomic<size_t> max_carve_size {16*1024*1024};
    std::atomic<int64_t> carved_file_count{0}; // starts at 0; gets incremented by carve();
    atomic_set<std::string> carve_cache{};                   // hashes of files that have been cached, so the same file is not carved twice
    std::string do_not_carve_encoding{}; // do not carve files with this encoding.
    static inline const std::string CARVE_MODE_DESCRIPTION {"0=carve none; 1=carve encoded; 2=carve all"};
    static inline const std::string NO_CARVED_FILE {""};

    // Carve data or a record to a file; returns filename of carved file or empty string if nothing carved
    // carve_data - sends to its own file.
    // carve_records - send all to the same file.
    // if mtime>0, set the file's mtime to be mtime
    // if offset>0, start with that offset
    // if offset>0, carve that many bytes, even into the margin (otherwise stop at the margin)

    // 'record carving', which basically means write out the header (now in an sbuf) in addition to the data (an sbuf)
    virtual std::string carve(const sbuf_t& header, const sbuf_t& sbuf, std::string ext, time_t mtime = 0);

    // carve the data in the sbuf to a file that ends with ext. If mtime>0, set the time of the file to be mtime
    virtual std::string carve(const sbuf_t& sbuf, std::string ext, time_t mtime = 0) {
        return carve(sbuf_t(), sbuf, ext, mtime);
    }

    // carve the data in the sbuf to a file that ends with ext, with ISO8601 string as time
    virtual std::string carve(const sbuf_t& sbuf, std::string ext, std::string mtime) {
        struct tm t;
        strptime(mtime.c_str(), "%Y-%m-%d %H:%M:%S", &t);
        time_t t2 = mktime(&t);
        return carve(sbuf_t(), sbuf, ext, t2);
    }

    /*
     * Each feature_recorder can have multiple histograms. They are generated on-the-fly for the file-based
     * feature-recorder, and generated in SQL for the SQL-based feature-recorder.
     */
    std::vector<std::unique_ptr<AtomicUnicodeHistogram>> histograms{};

    /* These must be specialized */
    virtual void histogram_flush(AtomicUnicodeHistogram& h);         // flush a specific histogram
    virtual void histograms_add_feature(const std::string& feature); // propose a feature to all of the histograms

    virtual size_t histogram_count() { return histograms.size(); } // how many histograms it has
    virtual void histogram_add(const struct histogram_def& def);   // add a new histogram
    virtual bool
    histogram_flush_largest();          // flushes largest histogram. returns false if no histogram could be flushed.
    virtual void histogram_flush_all(); // flushes all histograms
    // virtual void histogram_merge(const struct histogram_def &def); // merge sort on this histogram
    // virtual void histogram_merge_all();                            // merge sort on all histograms
};

#endif
