/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * bulk_extractor_i.h:
 *
 * The master config file for bulk_extractor.
 *
 * By design, this file can be read without reading config.h
 * #include "config.h" must appear as the first line of your .cpp file.
 */

#if 0

#ifndef PACKAGE_NAME
#error bulk_extractor_i.h included before config.h
#endif

#ifndef __cplusplus
#error bulk_extractor_i.h requires C++
#endif

#ifndef BULK_EXTRACTOR_I_H
#define BULK_EXTRACTOR_I_H

#define BE13_API_VERSION 2.0

#if 0
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
#include <vector>
#endif

//#include <unistd.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

/* We need netinet/in.h or windowsx.h */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if defined(MINGW) || defined(__MINGW__) || defined(__MINGW32__) || defined(__MINGW64__)
#ifndef WIN32
#define WIN32
#endif
#endif

#if defined(WIN32) || defined(__MINGW32__)
#include <windows.h>
#include <windowsx.h>
#include <winsock2.h>
#endif

/* If byte_order hasn't been defined, assume its intel */

#if defined(WIN32) || !defined(__BYTE_ORDER)
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

#if (__BYTE_ORDER == __LITTLE_ENDIAN) && (__BYTE_ORDER == __BIG_ENDIAN)
#error Invalid __BYTE_ORDER
#endif

#endif

/**
 * \addtogroup plugin_module
 * @{
 */

#include "aftimer.h"
#include "atomic_map.h"
#include "atomic_set.h"
#include "dfxml/src/dfxml_writer.h"
#include "dfxml/src/hash_t.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "packet_info.h"
#include "regex_vector.h"
#include "sbuf.h"
#include "sbuf_private.h"
#include "scanner_config.h"
#include "scanner_set.h"
#include "unicode_escape.h"
#include "utf8.h"
#include "utils.h" // for gmtime_r
#include "word_and_context_list.h"
//#include "sbuf_stream.h"

#endif
