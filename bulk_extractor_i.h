/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * bulk_extractor_i.h:
 *
 * The master config file for bulk_extractor.
 *
 * By design, this file can be read without reading config.h
 * #include "config.h" must appear as the first line of your .cpp file.
 */

#ifndef PACKAGE_NAME
#error bulk_extractor_i.h included before config.h
#endif

#ifndef __cplusplus
#error bulk_extractor_i.h requires C++
#endif

#ifndef BULK_EXTRACTOR_I_H
#define BULK_EXTRACTOR_I_H

#define BE13_API_VERSION  2.0

/* required per C++ standard */

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

#include <unistd.h>

#include <vector>
#include <set>
#include <map>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif



/* We need netinet/in.h or windowsx.h */
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#if defined(MINGW) || defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#ifndef WIN32
#define WIN32
#endif
#endif

#if defined(WIN32) || defined(__MINGW32__)
#  include <winsock2.h>
#  include <windows.h>
#  include <windowsx.h>
#endif

/* If byte_order hasn't been defined, assume its intel */

#if defined(WIN32) || !defined(__BYTE_ORDER)
#  define __LITTLE_ENDIAN 1234
#  define __BIG_ENDIAN    4321
#  define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#if (__BYTE_ORDER == __LITTLE_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#  error Invalid __BYTE_ORDER
#endif


const int DEBUG_PEDANTIC    = 0x0001;   // check values more rigorously
const int DEBUG_PRINT_STEPS = 0x0002;   // prints as each scanner is started
const int DEBUG_SCANNER     = 0x0004;   // dump all feature writes to stderr
const int DEBUG_NO_SCANNERS = 0x0008;   // do not run the scanners
const int DEBUG_DUMP_DATA   = 0x0010;   // dump data as it is seen
const int DEBUG_DECODING    = 0x0020;   // debug decoders in scanner
const int DEBUG_INFO        = 0x0040;   // print extra info
const int DEBUG_EXIT_EARLY  = 1000;     // just print the size of the volume and exis
const int DEBUG_ALLOCATE_512MiB = 1002;      // Allocate 512MiB, but don't set any flags

#ifdef HAVE_SQLITE3_H
#include <sqlite3.h>
#define BEAPI_SQLITE3 sqlite3
#define BEAPI_SQLITE3_STMT sqlite3_stmt
#else
#define BEAPI_SQLITE3 void
#define BEAPI_SQLITE3_STMT void
#endif

/**
 * \addtogroup plugin_module
 * @{
 */

/* Order matters! */
#include "histogram.h"
#include "dfxml/src/dfxml_writer.h"
#include "dfxml/src/hash_t.h"
#include "aftimer.h"
#include "atomic_set_map.h"
#include "regex_vector.h"
#include "sbuf.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "packet_info.h"
#include "sbuf_private.h"
#include "sbuf_stream.h"
#include "scanner_config.h"
#include "scanner_set.h"
#include "word_and_context_list.h"
#include "unicode_escape.h"
#include "utils.h"                      // for gmtime_r
#include "utf8.h"


inline std::string itos(int i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string dtos(double d){ std::stringstream ss; ss << d;return ss.str();}
inline std::string utos(unsigned int i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string utos(uint64_t i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string utos(uint16_t i){ std::stringstream ss; ss << i;return ss.str();}
inline std::string safe_utf16to8(std::wstring s){ // needs to be cleaned up
    std::string utf8_line;
    try {
        utf8::utf16to8(s.begin(),s.end(),back_inserter(utf8_line));
    } catch(const utf8::invalid_utf16 &){
        /* Exception thrown: bad UTF16 encoding */
        utf8_line = "";
    }
    return utf8_line;
}

inline std::wstring safe_utf8to16(std::string s){ // needs to be cleaned up
    std::wstring utf16_line;
    try {
        utf8::utf8to16(s.begin(),s.end(),back_inserter(utf16_line));
    } catch(const utf8::invalid_utf8 &){
        /* Exception thrown: bad UTF16 encoding */
        utf16_line = L"";
    }
    return utf16_line;
}

// truncate string at the matching char
inline void truncate_at(std::string &line, char ch) {
    size_t pos = line.find(ch);
    if(pos != std::string::npos) line.resize(pos);
}

#ifndef HAVE_ISXDIGIT
inline int isxdigit(int c)
{
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}
#endif

/* Useful functions for scanners */
#define ONE_HUNDRED_NANO_SEC_TO_SECONDS 10000000
#define SECONDS_BETWEEN_WIN32_EPOCH_AND_UNIX_EPOCH 11644473600LL
/*
 * 11644473600 is the number of seconds between the Win32 epoch
 * and the Unix epoch.
 *
 * http://arstechnica.com/civis/viewtopic.php?f=20&t=111992
 * gmtime_r() is Linux-specific. You'll find a copy in util.cpp for Windows.
 */

inline std::string microsoftDateToISODate(const uint64_t &time)
{
    time_t tmp = (time / ONE_HUNDRED_NANO_SEC_TO_SECONDS) - SECONDS_BETWEEN_WIN32_EPOCH_AND_UNIX_EPOCH;

    struct tm time_tm;
    gmtime_r(&tmp, &time_tm);
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &time_tm); // Zulu time
    return std::string(buf);
}

/* Convert Unix timestamp to ISO format */
inline std::string unixTimeToISODate(const uint64_t &t)
{
    struct tm time_tm;
    time_t tmp=t;
    gmtime_r(&tmp, &time_tm);
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &time_tm); // Zulu time
    return std::string(buf);
}

/* Many internal windows and Linux structures require a valid printable name in ASCII */
inline bool validASCIIName(const std::string &name)
{
    for ( auto ch:name){
        if (ch & 0x80)  return false; // high bit should not be set
        if (ch < ' ')   return false;  // should not be control character
        if (ch == 0x7f) return false; // DEL is not printable
    }
    return true;
}

#endif
