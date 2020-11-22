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

/* NOTE:
 * 1 - This typedef must remain outside the the feature_recorder due
 *     to historical reasons and cannot be made a vector
 * 2 - Do not make historam_def const!  It breaks some compilers.
 */

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


/* in-memory histograms */
typedef atomic_histogram<std::string,uint64_t> mhistogram_t;             // memory histogram
typedef std::map<histogram_def,mhistogram_t *> mhistograms_t;

//std::ostream & operator <<(std::ostream &os,const HistogramMaker::FrequencyReportVector &rep);

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
