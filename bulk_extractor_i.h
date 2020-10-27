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



#endif
