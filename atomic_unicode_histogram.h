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
 */

#include <atomic>
#include "atomic_map.h"
#include "histogram_def.h"
#include "unicode_escape.h"

struct AtomicUnicodeHistogram  {
    static uint32_t debug_histogram_malloc_fail_frequency; // for debugging, make malloc fail sometimes
    struct HistogramTally {
        uint32_t count      {0}; // total strings seen
        uint32_t count16    {0}; // total utf16 strings seen
        HistogramTally(const HistogramTally &a){
            this->count   = a.count;
            this->count16 = a.count16;
        }
        HistogramTally &operator=(const HistogramTally &a){
            this->count   = a.count;
            this->count16 = a.count16;
            return *this;
        }

        HistogramTally(){};
        virtual ~HistogramTally(){};

        bool operator==(const HistogramTally &a) const {
            return this->count==a.count && this->count16==a.count16;
        };
        bool operator!=(const HistogramTally &a) const {
            return !(*this == a);
        }
        bool operator<(const HistogramTally &a) const {
            return (this->count < a.count) ||
                ((this->count == a.count && (this->count16 < a.count16)));
        }
        size_t bytes() const { return sizeof(*this);}
    };

    /* A FrequencyReportVector is a vector of report elements when the report is generated.*/
    typedef atomic_map<std::string, struct AtomicUnicodeHistogram::HistogramTally> auh_t;
    typedef std::vector<auh_t::AMReportElement> FrequencyReportVector;

    AtomicUnicodeHistogram(const struct histogram_def &def_):def(def_){}
    virtual ~AtomicUnicodeHistogram(){};

    void clear(){ h.clear(); }    //
    void add(std::string key);  // adds Unicode string to the histogram count
    size_t bytes(){               // returns the total number of bytes of the histogram,.
        size_t count = sizeof(*this);
        for(auto it:h){
            count += sizeof(it.first) + it.first.size() + it.second.bytes();
        }
        return count;
    }

    /** makeReport() makes a report and returns a
     * FrequencyReportVector.
     */
    auh_t::report makeReport(size_t topN=0) const; // returns just the topN; 0 means all
    const struct histogram_def &def;   // the definition we are making

private:
    auh_t h {};                        // the histogram
};

std::ostream & operator << (std::ostream &os, const AtomicUnicodeHistogram::FrequencyReportVector &rep);
std::ostream & operator << (std::ostream &os, const AtomicUnicodeHistogram::auh_t::AMReportElement &e);


#endif
