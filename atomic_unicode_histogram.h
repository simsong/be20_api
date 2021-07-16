#ifndef ATOMIC_UNICODE_HISTOGRAM_H
#define ATOMIC_UNICODE_HISTOGRAM_H

/** A simple class for making histograms of strings.
 * Histograms are kept in printable UTF-8 representation, not in UTF32 internally.
 * In part this us due to the legacy code base.
 * This part this allows the scanners to determine what the printout should look like, rather than having
 * to pass presentation flags.
 *
 * Histogram maker implement:
 * - Counting
 * - Determining how much memory is in use by histogram.
 * - Writing histogram to a stream (for example, when memory is filled.)
 * - Merging multiple histogram files to a single file.
 *
 * Note - case transitions and text extraction is performed in UTF-32.
 *      - regular expression are then run on the UTF-8. (Not the best, but it works for now.)
 */

#include "atomic_map.h"
#include "histogram_def.h"
#include "unicode_escape.h"
#include <atomic>

struct AtomicUnicodeHistogram {
    static uint32_t debug_histogram_malloc_fail_frequency; // for debugging, make malloc fail sometimes
    struct HistogramTally {
        uint32_t count{0};   // total strings seen
        uint32_t count16{0}; // total utf16 strings seen
        HistogramTally(const HistogramTally& a) {
            this->count = a.count;
            this->count16 = a.count16;
        }
        HistogramTally& operator=(const HistogramTally& a) {
            this->count = a.count;
            this->count16 = a.count16;
            return *this;
        }

        HistogramTally(){};
        virtual ~HistogramTally(){};

        bool operator==(const HistogramTally& a) const { return this->count == a.count && this->count16 == a.count16; };
        bool operator!=(const HistogramTally& a) const { return !(*this == a); }
        bool operator<(const HistogramTally& a) const {
            return (this->count < a.count) || ((this->count == a.count && (this->count16 < a.count16)));
        }
        size_t bytes() const { return sizeof(*this); }
    };

    /* A FrequencyReportVector is a vector of report elements when the report is generated.*/
    typedef atomic_map<std::string, struct AtomicUnicodeHistogram::HistogramTally> auh_t;
    typedef std::vector<auh_t::item> FrequencyReportVector;

    AtomicUnicodeHistogram(const struct histogram_def& def_) : def(def_) {}
    virtual ~AtomicUnicodeHistogram(){};

    void clear();                     // empties the histogram
    void add(const std::string& key); // adds Unicode string to the histogram count
    size_t size() const;              // returns the size of the histogram, whatever that means

    /** makeReport() makes a report and returns a
     * FrequencyReportVector.
     */
    std::vector<auh_t::item> makeReport(size_t topN=0);          // returns items of <count,key>
    const struct histogram_def def;            // the definition we are making

private:
    auh_t h {}; // the histogram
};

std::ostream& operator<<(std::ostream& os, const AtomicUnicodeHistogram::FrequencyReportVector& rep);
std::ostream& operator<<(std::ostream& os, const AtomicUnicodeHistogram::auh_t::item& e);

#endif
