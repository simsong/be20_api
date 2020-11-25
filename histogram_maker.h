#ifndef HISTOGRAM_MAKER_H
#define HISTOGRAM_MAKER_H

/** A simple class for making histograms of strings.
 */

class HistogramMaker  {
public:;

    struct HistogramTally {
        uint32_t count      {0};    // total strings seen
        uint32_t count16    {0};	// total utf16 strings seen
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

    };

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    struct ReportElement {

	ReportElement(std::string aValue):value(aValue){ };
	std::string         value;		// UTF-8
	struct HistogramTally      tally {};

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
    };

    std::map<std::string, struct HistogramTally> h {}; // the histogram
    const struct histogram_def &def;                          // the definition we are making
    uint32_t debug_histogram_malloc_fail_frequency {};    // for debugging, make malloc fail sometimes

    HistogramMaker(const struct histogram_def &def_):def(def_){}
    void clear(){ h.clear(); }        //
    void add(const std::string &key);	// adds a string to the histogram count

    /** A FrequencyReportVector is a vector of report elements when the report is generated.
     */
    typedef std::vector<ReportElement *> FrequencyReportVector;
    /** makeReport() makes a report and returns a
     * FrequencyReportVector.
     */
    FrequencyReportVector *makeReport(int topN=0) const; // returns just the topN; 0 means all
    virtual ~HistogramMaker(){};
};

#endif
