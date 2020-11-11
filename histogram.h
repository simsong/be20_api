#ifndef HISTOGRAM_H
#define HISTOGRAM_H

/**
 * \addtogroup internal_interfaces
 * @{
 */

/* C++ Histogram classes.
 *
 * Eventually this may become a single class
 */

#include <vector>
#include <map>
#include <regex>

#include "atomic_set_map.h"

/**
 * histogram_def defines the histograms that will be made by a feature recorder.
 * If the mhistogram is set, the histogram is generated when features are recorded
 * and kept in memory. If mhistogram is not set, the histogram is generated when the feature recorder is closed.
 */

struct histogram_def {
    /**
     * @param feature- the feature file to histogram (no .txt)
     * @param re     - the regular expression to extract
     * @param require- require this string on the line (usually in context)
     * @param suffix - the suffix to add to the histogram file after feature name before .txt
     * @param flags  - any flags (see above)
     */

    histogram_def(std::string feature_,std::string pattern_,std::string suffix_="",uint32_t flags_=0):
        feature(feature_),pattern(pattern_),require(),suffix(suffix_),flags(flags_),reg(pattern){}
    const std::string feature;      /* feature file name */
    const std::string pattern;      /* regular expression used to extract feature substring from feature. "" means use the entire feature*/
    const std::string require;      /* text required somewhere on the feature line. Sort of like grep. used for IP histograms */
    const std::string suffix;       /* suffix to append to histogram report name */
    const uint32_t    flags;        // see above
    const std::regex  reg;          // the compiled regular expression.
};

/* NOTE:
 * 1 - This typedef must remain outside the the feature_recorder due
 *     to historical reasons and cannot be made a vector
 * 2 - Do not make historam_def const!  It breaks some compilers.
 */

typedef  std::set<histogram_def> histogram_defs_t; // a set of histogram definitions



/**
 * \class CharClass
 * Examine a block of text and count the number of characters
 * in various ranges. This is useful for determining if a block of
 * bytes is coded in BASE16, BASE64, etc.
 */

class CharClass {
public:
    uint32_t range_0_9 {0};			// a range_0_9 character
    uint32_t range_A_Fi {0};		// a-f or A-F
    uint32_t range_g_z  {0};			// g-z
    uint32_t range_G_Z  {0};			// G-Z
    CharClass(){}
    void add(const uint8_t ch){
	if (ch>='a' && ch<='f') range_A_Fi++;
	if (ch>='A' && ch<='F') range_A_Fi++;
	if (ch>='g' && ch<='z') range_g_z++;
	if (ch>='G' && ch<='Z') range_G_Z++;
	if (ch>='0' && ch<='9') range_0_9++;
    }
    void add(const uint8_t *buf,size_t len){
	for (size_t i=0;i<len;i++){
	    add(buf[i]);
	}
    }
};


/**
 * \file histogram.h
 * Unicode histogram
 *
 * The basis of a string-based correlator and many other features.
 * Uses C++ STL for sorting and string handling.
 *
 * Summer 2011: Now is UTF-8/UTF-16 aware. All strings are stored as UTF-8.
 * Detects UTF-16 in an add and automatically converts to UTF-8.
 * Keeps track of UTF-16 count separately from UTF-8 count.
 *
 * Oct 2011: Apparently you are not supposed to subclass the STL container classes.
 */


class HistogramMaker  {
public:
    static const int FLAG_LOWERCASE= 0x01;
    static const int FLAG_NUMERIC  = 0x02;                    // digits only
    static uint32_t debug_histogram_malloc_fail_frequency;    // for debugging, make malloc fail sometimes

    /** The ReportElement is used for creating the report of histogram frequencies.
     * It can be thought of as the histogram bin.
     */
    class histogramTally {
    public:
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
	const std::string   value;		// UTF-8
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

private:
    /** A HistogramMap holds the histogram while it is being computed.
     */
    typedef std::map<std::string,histogramTally> HistogramMap;
    HistogramMap h;			// holds the histogram
    uint32_t     flags;			// see above
public:

    /**
     * Determine if a string probably has utf16.
     */
    static bool looks_like_utf16(const std::string &str,bool &little_endian);

    /* These all allocate a string that must be freed */

    static std::string *convert_utf16_to_utf8(const std::string &str);
    static std::string *convert_utf16_to_utf8(const std::string &str,bool little_endian);
    static std::string *make_utf8(const std::string &key);

    HistogramMaker(uint32_t flags_):h(),flags(flags_){}
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


/* carve object cache */
typedef atomic_set<std::string> carve_cache_t;

/* in-memory histograms */
typedef atomic_histogram<std::string,uint64_t> mhistogram_t;             // memory histogram
typedef std::map<histogram_def,mhistogram_t *> mhistograms_t;

/*** END OF HISTOGRAM STUFF ***/

std::ostream & operator <<(std::ostream &os,const HistogramMaker::FrequencyReportVector &rep);

inline bool operator <(const histogram_def &h1,const histogram_def &h2)  {
    if ( h1.feature<h2.feature ) return true;
    if ( h1.feature>h2.feature ) return false;
    if ( h1.pattern<h2.pattern ) return true;
    if ( h1.pattern>h2.pattern ) return false;
    if ( h1.suffix<h2.suffix ) return true;
    if ( h1.suffix>h2.suffix ) return false;
    return false;                       /* equal */
};

inline bool operator !=(const histogram_def &h1,const histogram_def &h2)  {
    return h1.feature!=h2.feature || h1.pattern!=h2.pattern || h1.suffix!=h2.suffix;
};

#endif
