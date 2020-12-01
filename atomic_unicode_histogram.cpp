/**
 * atomic_unicode_histogram.cpp:
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
#include <cwctype>

#include "atomic_unicode_histogram.h"

/* Create a histogram report.
 * @param topN - if >0, return only this many.
 * Return only the topN.
 */
AtomicUnicodeHistogram::auh_t::report AtomicUnicodeHistogram::makeReport(size_t topN) const
{
    auh_t::report rep = h.dump( true );

    /* If we only want some of them, delete the extra */
    if ( (topN > 0)  && ( topN < rep.size()) ){
        rep.resize( topN );
    }
    return rep;
}

/**
 * Takes a string (the key) and adds it to the histogram.
 * @param - key - either a UTF8 or UTF16 string.
 * If the string appears to be UTF16, convert it to UTF-8 and note that it was converted.
 *
 * def.flags.digits - extract the digits first and throw away the rest.
 * def.flags.lower  - also convert to lowercase using Unicode rules.
 */

uint32_t AtomicUnicodeHistogram::debug_histogram_malloc_fail_frequency = 0;
void AtomicUnicodeHistogram::add(std::string key)
{
    if(key.size()==0) return;		// don't deal with zero-length keys

    /**
     * "key" passed in is a const reference.
     * But we might want to change it. So keyToAdd points to what will be added.
     * If we need to change key, we allocate more memory, and make keyToAdd
     * point to the memory that was allocated. This way we only make a copy
     * if we need to make a copy.
     */

    bool found_utf16 = false;           // did we find a utf16?
    bool little_endian=false;           // was it little_endian?
    if (looks_like_utf16(key,little_endian)){
        std::cerr << "looks like utf16\n";
        key = convert_utf16_to_utf8(key, little_endian);
        found_utf16 = true;
        std::cerr << "key=" << key << "\n";
    }

    /* At this point we have UTF-8
     * This is a bit of a kludge:
     * If we are told to lowercase the string, convert it to wchar, lowercase, then turn it back to utf8.
     * Ideally this would be done with ICU, but we do not want to assume we have ICU.
     * https://stackoverflow.com/questions/34433380/lowercase-of-unicode-character
     * https://stackoverflow.com/questions/313970/how-to-convert-stdstring-to-lower-case/24063783
     * https://en.cppreference.com/w/cpp/string/wide/towlower
     * See: http://stackoverflow.com/questions/1081456/wchar-t-vs-wint-t
     */

    if ( def.flags.lowercase ){
        /* If we cannot do a clean utf8->utf16->utf8 conversion, abort and keep original string */
	try {
	    std::wstring key_utf16;
            /* convert utf8->utf16 */
	    utf8::utf8to16( key.begin(), key.end(), std::back_inserter( key_utf16));
            /* lowercase */
	    for (auto it:key_utf16){
		it = std::towlower(it);
	    }
            /* put back */
	    key.clear();
	    utf8::utf16to8(key_utf16.begin(), key_utf16.end(), std::back_inserter( key));
	} catch(const utf8::exception &){
	    /* Exception thrown during utf8 or 16 conversions.
	     * So the string we thought was valid utf8 wasn't valid utf8 afterall.
	     * tempKey will remain unchanged.
	     */
	}
    }

    if ( def.flags.numeric ){
	/* Similar to above, but extract the digits and the plus sign */
	try{
	    std::wstring key_utf16;
	    std::wstring digits_utf16;

            std::cerr <<"a.key=" << key << "\n";

	    utf8::utf8to16( key.begin(), key.end(),std::back_inserter(key_utf16));
	    for (auto it: key_utf16){
		if (std::iswdigit(it) || it==static_cast<uint16_t>('+')){
		    digits_utf16.push_back(it);
		}
	    }
	    /* convert it back */
	    key.clear();		// erase the characters
	    utf8::utf16to8(digits_utf16.begin(), digits_utf16.end(),std::back_inserter(key));
            std::cerr <<"b.key=" << key << "\n";
	} catch(const utf8::exception &){
	    /* Exception during utf8 or 16 conversions*.
	     * So the string wasn't utf8.  Fall back to just extracting the digits
	     */
            std::cerr <<"0.key=" << key << "\n";
            std::string digits;
	    for (auto it:key){
		if (isdigit(it)){
		    digits.push_back(it);
		}
	    }
            key = digits;
            std::cerr <<"1.key=" << key << "\n";
	}
    }

    /* For debugging low-memory handling logic,
     * specify DEBUG_MALLOC_FAIL to make malloc occasionally fail
     */
    if (debug_histogram_malloc_fail_frequency){
        if ((h.size() % debug_histogram_malloc_fail_frequency)==(debug_histogram_malloc_fail_frequency-1)){
            throw std::bad_alloc();
        }
    }

    /* Add the key to the histogram. Note that this is threadsafe */
    h.insertIfNotContains(key, HistogramTally());
    h[key].count++;
    if (found_utf16){
        h[key].count16++;  // track how many UTF16s were converted
        std::cerr << "found_utf16. count=" << h[key].count16 << "\n";
    }
}
