/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/*
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

#define DEBUG_PEDANTIC    0x0001        // check values more rigorously
#define DEBUG_PRINT_STEPS 0x0002        // prints as each scanner is started
#define DEBUG_SCANNER     0x0004        // dump all feature writes to stderr
#define DEBUG_NO_SCANNERS 0x0008        // do not run the scanners
#define DEBUG_DUMP_DATA   0x0010        // dump data as it is seen
#define DEBUG_DECODING    0x0020        // debug decoders in scanner
#define DEBUG_INFO        0x0040        // print extra info
#define DEBUG_EXIT_EARLY  1000          // just print the size of the volume and exis 
#define DEBUG_ALLOCATE_512MiB 1002      // Allocate 512MiB, but don't set any flags 

/* We need netinet/in.h or windowsx.h */
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#include <assert.h>

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

/**
 * \addtogroup plugin_module
 * @{
 */

/**
 * \file
 * bulk_extractor scanner plug_in architecture.
 *
 * Scanners are called with two parameters:
 * A reference to a scanner_params (SP) object.
 * A reference to a recursion_control_block (RCB) object.
 * 
 * On startup, each scanner is called with a special SP and RCB.
 * The scanners respond by setting fields in the SP and returning.
 * 
 * When executing, once again each scanner is called with the SP and RCB.
 * This is the only file that needs to be included for a scanner.
 *
 * \li \c phase_startup - scanners are loaded and register the names of the feature files they want.
 * \li \c phase_scan - each scanner is called to analyze 1 or more sbufs.
 * \li \c phase_shutdown - scanners are given a chance to shutdown
 */

#include "sbuf.h"
#include "utf8.h"
#include "utils.h"                      // for gmtime_r

#include <vector>
#include <set>
#include <map>



/* Network includes */

/****************************************************************
 *** pcap.h --- If we don't have it, fake it. ---
 ***/
#ifdef HAVE_NETINET_IF_ETHER_H
# include <netinet/if_ether.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NET_ETHERNET_H
# include <net/ethernet.h>              // for freebsd
#endif


#if defined(HAVE_LIBPCAP)
#  ifdef HAVE_DIAGNOSTIC_REDUNDANT_DECLS
#    pragma GCC diagnostic ignored "-Wredundant-decls"
#  endif
#  if defined(HAVE_PCAP_PCAP_H)
#    include <pcap/pcap.h>
#    define GOT_PCAP
#  endif
#  if defined(HAVE_PCAP_H) && !defined(GOT_PCAP)
#    include <pcap.h>
#    define GOT_PCAP
#  endif
#  if defined(HAVE_WPCAP_PCAP_H) && !defined(GOT_PCAP)
#    include <wpcap/pcap.h>
#    define GOT_PCAP
#  endif
#  ifdef HAVE_DIAGNOSTIC_REDUNDANT_DECLS
#    pragma GCC diagnostic warning "-Wredundant-decls"
#  endif
#else
#  include "pcap_fake.h"
#endif

/**
 * \class scanner_params
 * The scanner params class is the primary way that the bulk_extractor framework
 * communicates with the scanners. 
 * @param sbuf - the buffer to be scanned
 * @param feature_names - if fs==0, add to feature_names the feature file types that this
 *                        scanner records.. The names can have a /c appended to indicate
 *                        that the feature files should have context enabled. Do not scan.
 * @param fs   - where the features should be saved. Must be provided if feature_names==0.
 **/


#include "atomic_set_map.h"
#include "regex_vector.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "sbuf.h"
#include "word_and_context_list.h"
#include "unicode_escape.h"
#include "plugin.h"                     // plugin system must be included last

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
