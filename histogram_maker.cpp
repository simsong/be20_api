/**
 * histogram.cpp:
 * Maintain a histogram for Unicode strings provided as UTF-8 and UTF-16 encodings.
 * Track number of each coding provided.
 *
 * TK: Reimplement top-n with a priority queue.
 *  http://www.cplusplus.com/reference/queue/priority_queue/
 */

#include "config.h"
#include "unicode_escape.h"
#include "utf8.h"

#include <fstream>
#include <iostream>

#if 0
std::ostream & operator << (std::ostream &os, const HistogramMaker::FrequencyReportVector &rep){
    for(HistogramMaker::FrequencyReportVector::const_iterator i = rep.begin(); i!=rep.end();i++){
        const HistogramMaker::ReportElement &r = *(*i);
	os << "n=" << r.tally.count << "\t" << validateOrEscapeUTF8(r.value, true, true, true);
	if(r.tally.count16>0) os << "\t(utf16=" << r.tally.count16<<")";
	os << "\n";
    }
    return os;
}

HistogramMaker::FrequencyReportVector HistogramMaker::makeReport()  const
{
    const std::lock_guard<std::mutex> lock(Mh);
    FrequencyReportVector rep;
    for(auto it: h){
	rep->push_back(new ReportElement(it->first,it->second));
    }
    sort(rep.begin(),rep.end(),ReportElement::compare);
    return rep;
}

/* This would be better done with a priority queue */
HistogramMaker::FrequencyReportVector *HistogramMaker::makeReport(int topN) const
{
    HistogramMaker::FrequencyReportVector         *r2 = makeReport();	// gets a new report
    HistogramMaker::FrequencyReportVector::iterator i = r2->begin();
    while(topN>0 && i!=r2->end()){	// iterate through the first set
	i++;
	topN--;
    }

    /* Delete the elements we won't use */
    for(HistogramMaker::FrequencyReportVector::iterator j=i;j!=r2->end();j++){
        delete (*j);
    }
    r2->erase(i,r2->end());
    return r2;
}

/**
 * Takes a string (the key) and adds it to the histogram.
 * automatically determines if the key is UTF-16 and converts
 * it to UTF8 if so.
 */

uint32_t HistogramMaker::debug_histogram_malloc_fail_frequency = 0;
void HistogramMaker::add(const std::string &key)
{
    if(key.size()==0) return;		// don't deal with zero-length keys

    /**
     * "key" passed in is a const reference.
     * But we might want to change it. So keyToAdd points to what will be added.
     * If we need to change key, we allocate more memory, and make keyToAdd
     * point to the memory that was allocated. This way we only make a copy
     * if we need to make a copy.
     */

    const std::string *keyToAdd = &key;	// should be a reference, but that doesn't work
    std::string *tempKey = 0;		// place to hold UTF8 key
    bool found_utf16 = false;
    bool little_endian=false;
    if(looks_like_utf16(*keyToAdd,little_endian)){
        tempKey = convert_utf16_to_utf8(*keyToAdd,little_endian);
        if(tempKey){
            keyToAdd = tempKey;
            found_utf16 = true;
        }
    }

    /* If any conversion is necessary AND we have not converted key from UTF-16 to UTF-8,
     * then the original key is still in 'key'. Allocate tempKey and copy key to tempKey.
     */
    if ( flags.lowercase || flags.numeric ){
	if(tempKey==0){
	    tempKey = new std::string(key);
	    keyToAdd = tempKey;
	}
    }


    /* Apply the flags */
    // See: http://stackoverflow.com/questions/1081456/wchar-t-vs-wint-t
    if ( flags.lowercase ){
	/* keyToAdd is UTF-8; convert to UTF-16, downcase, and convert back to UTF-8 */
	try{
	    std::wstring utf16key;
	    utf8::utf8to16(tempKey->begin(),tempKey->end(),std::back_inserter(utf16key));
	    for(std::wstring::iterator it = utf16key.begin();it!=utf16key.end();it++){
		*it = towlower(*it);
	    }
	    /* erase tempKey and copy the utf16 back into tempKey as utf8 */
	    tempKey->clear();		// erase the characters
	    utf8::utf16to8(utf16key.begin(),utf16key.end(),std::back_inserter(*tempKey));
	} catch(const utf8::exception &){
	    /* Exception thrown during utf8 or 16 conversions.
	     * So the string we thought was valid utf8 wasn't valid utf8 afterall.
	     * tempKey will remain unchanged.
	     */
	}
    }
    if ( flags.numeric ){
	/* keyToAdd is UTF-8; convert to UTF-16, extract digits, and convert back to UTF-8 */
	std::string originalTempKey(*tempKey);
	try{
	    std::wstring utf16key;
	    std::wstring utf16digits;
	    utf8::utf8to16(tempKey->begin(),tempKey->end(),std::back_inserter(utf16key));
	    for(std::wstring::iterator it = utf16key.begin();it!=utf16key.end();it++){
		if(iswdigit(*it) || *it==static_cast<uint16_t>('+')){
		    utf16digits.push_back(*it);
		}
	    }
	    /* convert it back */
	    tempKey->clear();		// erase the characters
	    utf8::utf16to8(utf16digits.begin(),utf16digits.end(),std::back_inserter(*tempKey));
	} catch(const utf8::exception &){
	    /* Exception during utf8 or 16 conversions*.
	     * So the string wasn't utf8.  Fall back to just extracting the digits
	     */
	    tempKey->clear();
	    for(std::string::iterator it = originalTempKey.begin(); it!=originalTempKey.end(); it++){
		if(isdigit(*it)){
		    tempKey->push_back(*it);
		}
	    }
	}
    }

    {
        const std::lock_guard<std::mutex> lock(Mh);
        /* For debugging low-memory handling logic,
         * specify DEBUG_MALLOC_FAIL to make malloc occasionally fail
         */
        if(debug_histogram_malloc_fail_frequency){
            if((h.size() % debug_histogram_malloc_fail_frequency)==(debug_histogram_malloc_fail_frequency-1)){
                throw std::bad_alloc();
            }
        }
        h[*keyToAdd].count++;
        if(found_utf16) h[*keyToAdd].count16++;  // track how many UTF16s were converted
    }

    if(tempKey){			     // if we allocated tempKey, free it
	delete tempKey;
    }
}
#endif
