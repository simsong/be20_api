/**
 * A collection of utility functions that are typically provided,
 * but which are missing in some implementations.
 */

// Just for this module
#define _FILE_OFFSET_BITS 64

/* required per C++ standard */
//#ifndef __STDC_FORMAT_MACROS
//#define __STDC_FORMAT_MACROS
//#endif

#include "config.h"
#include "utils.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <filesystem>

#include <mutex>
#include <sstream>

//#ifdef HAVE_UNISTD_H
//#include <unistd.h>
//#endif

#if 0
#ifndef HAVE_ERR
void err(int eval,const char *fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    vfprintf(stderr,fmt,ap);
    va_end(ap);
    fprintf(stderr,": %s\n",strerror(errno));
    exit(eval);
}
#endif

#ifndef HAVE_ERRX
void errx(int eval,const char *fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    vfprintf(stderr,fmt,ap);
    fprintf(stderr,"%s\n",strerror(errno));
    va_end(ap);
    exit(eval);
}
#endif

#ifndef HAVE_WARN
void	warn(const char *fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    vfprintf(stderr,fmt, args);
    fprintf(stderr,": %s\n",strerror(errno));
}
#endif

#ifndef HAVE_WARNX
void warnx(const char *fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    vfprintf(stderr,fmt,ap);
    va_end(ap);
}
#endif
#endif

/** Extract a buffer...
 * @param buf - the buffer to extract;
 * @param buflen - the size of the page to extract
 * @param pos0 - the byte position of buf[0]
 */

#ifndef HAVE_LOCALTIME_R
/* locking localtime_r implementation */
std::mutex localtime_mutex;
void localtime_r(time_t* t, struct tm* tm) {
    const std::lock_guard<std::mutex> lock(localtime_mutex);
    *tm = *localtime(t);
}
#endif

#ifndef HAVE_GMTIME_R
/* locking gmtime_r implementation */
std::mutex gmtime_mutex;
void gmtime_r(time_t* t, struct tm* tm) {
    if (t && tm) {
        const std::lock_guard<std::mutex> lock(gmtime_mutex);
        struct tm* tmret = gmtime(t);
        if (tmret) {
            *tm = *tmret;
        } else {
            memset(tm, 0, sizeof(*tm));
        }
    }
}
#endif

bool ends_with(const std::string& buf, const std::string& with) {
    size_t buflen = buf.size();
    size_t withlen = with.size();
    return buflen > withlen && buf.substr(buflen - withlen, withlen) == with;
}

bool ends_with(const std::wstring& buf, const std::wstring& with) {
    size_t buflen = buf.size();
    size_t withlen = with.size();
    return buflen > withlen && buf.substr(buflen - withlen, withlen) == with;
}

/****************************************************************/
/* C++ string splitting code from http://stackoverflow.com/questions/236129/how-to-split-a-string-in-c */
std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) { elems.push_back(item); }
    return elems;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

uint64_t scaled_stoi64(const std::string &str)
{
    std::stringstream ss(str);
    uint64_t val;
    ss >> val;
    if(str.find('k')!=std::string::npos  || str.find('K')!=std::string::npos) val *= 1024LL;
    if(str.find('m')!=std::string::npos  || str.find('m')!=std::string::npos) val *= 1024LL * 1024LL;
    if(str.find('g')!=std::string::npos  || str.find('g')!=std::string::npos) val *= 1024LL * 1024LL * 1024LL;
    if(str.find('t')!=std::string::npos  || str.find('T')!=std::string::npos) val *= 1024LL * 1024LL * 1024LL * 1024LL;
    return val;
}
