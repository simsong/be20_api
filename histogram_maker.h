#ifndef HISTOGRAM_MAKER_H
#define HISTOGRAM_MAKER_H

/** A simple class for making histograms of strings.
 */

class HistogramMaker  {

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    struct HistogramTally {
	uint32_t count      {0};		// total strings seen
	uint32_t count16    {0};	// total utf16 strings seen
	HistogramTally(){};
	virtual ~HistogramTally(){};
    };

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    struct ReportElement {
        ReportElement(){}
	ReportElement(std::string aValue,HistogramTally aTally):value(aValue),tally(aTally){ }
	std::string         value {};		// UTF-8
	HistogramTally      tally {};
	static bool compare_ref(const ReportElement &e1,const ReportElement &e2) {
	    if (e1.tally.count > e2.tally.count) return true;
	    if (e1.tally.count < e2.tally.count) return false;
	    return e1.value < e2.value;
	}
	static bool compare(const ReportElement *e1,const ReportElement *e2) {
	    if (e1->tally.count > e2->tally.count) return true;
	    if (e1->tally.count < e2->tally.count) return false;
	    return e1->value < e2->value;
	}
	virtual ~ReportElement(){};
    };

    std::map<std::string, struct HistogramTally> h {}; // the histogram
    const struct histogram_def &def;                          // the definition we are making

    uint32_t debug_histogram_malloc_fail_frequency {};    // for debugging, make malloc fail sometimes

    static std::string *convert_utf16_to_utf8(const std::string &str);
    static std::string *convert_utf16_to_utf8(const std::string &str,bool little_endian);
    static std::string *make_utf8(const std::string &key);

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
