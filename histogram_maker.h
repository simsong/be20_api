class HistogramMaker  {
    /** A HistogramMap holds the histogram while it is being computed.
     */
    typedef std::map<std::string, struct histogramTally> HistogramMap;
    HistogramMap h {};			// holds the histogram
    struct histogram_def def;

public:
    static uint32_t debug_histogram_malloc_fail_frequency;    // for debugging, make malloc fail sometimes

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    struct histogramTally {
	uint32_t count;		// total strings seen
	uint32_t count16;	// total utf16 strings seen
	histogramTally():count(0),count16(0){};
	virtual ~histogramTally(){};
    };

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    struct ReportElement {
	ReportElement(std::string aValue,histogramTally aTally):value(aValue),tally(aTally){ }
	std::string   value;		// UTF-8
	histogramTally      tally;
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

    /**
     * Determine if a string probably has utf16.
     */
    static bool looks_like_utf16(const std::string &str,bool &little_endian);

    /* These all allocate a string that must be freed */

    static std::string *convert_utf16_to_utf8(const std::string &str);
    static std::string *convert_utf16_to_utf8(const std::string &str,bool little_endian);
    static std::string *make_utf8(const std::string &key);

    HistogramMaker(){}
    void clear(){h.clear();}
    void add(const std::string &key);	// adds a string to the histogram count

    /** A FrequencyReportVector is a vector of report elements when the report is generated.
     */
    typedef std::vector<ReportElement *> FrequencyReportVector;
    /** makeReport() makes a report and returns a
     * FrequencyReportVector.
     */
    FrequencyReportVector *makeReport() const;	// return a report with all of them
    FrequencyReportVector *makeReport(int topN) const; // returns just the topN
    virtual ~HistogramMaker(){};
};
