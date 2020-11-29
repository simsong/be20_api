#ifndef HISTOGRAM_MAKER_H
#define HISTOGRAM_MAKER_H

/** A simple class for making histograms of strings. Strings are analyzed to see if they contain UTF16 and, if they do, that is separately tallied as thei UTF-8 component.
 * Histogram maker implement:
 * - Counting
 * - Determining how much memory is in use by histogram.
 * - Writing histogram to a stream (for example, when memory is filled.)
 * - Merging multiple histogram files to a single file.
 */

#include <atomic>

class HistogramMaker  {
public:;
    struct HistogramTally {
        HistogramTally(const HistogramTally &a){
            this->count   = a.count;
            this->count16 = a.count16;
        }
        HistogramTally &operator=(const HistogramTally &a){
            this->count   = a.count;
            this->count16 = a.count16;
            return *this;
        }

        uint32_t count      {0}; // total strings seen
        uint32_t count16    {0}; // total utf16 strings seen
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

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    struct ReportElement {
	ReportElement(std::string aValue):value(aValue){ };
	std::string                value {};		// UTF-8
	struct HistogramMaker::HistogramTally      tally {};

        bool operator==(const ReportElement &a) const {
            return (this->value == a.value) && (this->tally == a.tally);
        }
        bool operator!=(const ReportElement &a) const {
            return !(*this == a);
        }
        bool operator<(const ReportElement &a) const {
            if (this->value < a.value) return true;
            if ((this->value == a.value) &&
                (this->tally < a.tally)) return true;
            return false;
        }

	static bool compare(const ReportElement &e1,const ReportElement &e2) {
            return e1 < e2;
	}
	virtual ~ReportElement(){};
        size_t bytes() const{
            return sizeof(*this) + value.size();
        }                 // number of bytes used by object
    };
    /* A FrequencyReportVector is a vector of report elements when the report is generated.*/
    typedef std::vector<ReportElement *> FrequencyReportVector;

private:
    /* The histogram: */
    std::map<std::string, struct HistogramMaker::HistogramTally> h {}; // the histogram
    mutable std::mutex Mh;                            // protecting mutex
    const struct histogram_def &def;                   // the definition we are making
    uint32_t debug_histogram_malloc_fail_frequency {}; // for debugging, make malloc fail sometimes

public:

    HistogramMaker(const struct histogram_def &def_):def(def_){}
    void clear(){ h.clear(); }        //
    void add(const std::string &key);	// adds a string to the histogram count
    static size_t addSizes(const ReportElement &a, const ReportElement b){
        return a.bytes() + b.bytes();
    }
    size_t bytes(){            // returns the total number of bytes of the histogram,.
        size_t count = sizeof(*this);
        for(auto it:h){
            count += sizeof(it.first) + it.first.size() + it.second.bytes();
        }
        return count;
    }

    /** makeReport() makes a report and returns a
     * FrequencyReportVector.
     */
    FrequencyReportVector *makeReport(int topN=0) const; // returns just the topN; 0 means all
    virtual ~HistogramMaker(){};
};

#endif
