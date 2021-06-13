/****************************************************************
 *** utils.h
 ***
 *** To use utils.c/utils.h, be sure this is in your configure.ac file:
      m4_include([be13_api/be13_configure.m4])
 ***
 ****************************************************************/

#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "utf8.h"

bool ends_with(const std::string& buf, const std::string& with);
bool ends_with(const std::wstring& buf, const std::wstring& with);
std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems);
std::vector<std::string> split(const std::string& s, char delim);
inline void truncate_at(std::string& line, char ch) {
    size_t pos = line.find(ch);
    if (pos != std::string::npos) line.resize(pos);
};

#ifndef HAVE_LOCALTIME_R
#ifdef __MINGW32__
#undef localtime_r
#endif
void localtime_r(time_t* t, struct tm* tm);
#endif

#ifndef HAVE_GMTIME_R
#ifdef __MINGW32__
#undef gmtime_r
#endif
void gmtime_r(time_t* t, struct tm* tm);
#endif

int64_t get_filesize(int fd);

#ifndef HAVE_ISHEXNUMBER
inline int ishexnumber(int c) {
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f': return 1;
    }
    return 0;
}
#endif

inline std::string safe_utf16to8(std::wstring s) { // needs to be cleaned up
    std::string utf8_line;
    try {
        utf8::utf16to8(s.begin(), s.end(), back_inserter(utf8_line));
    } catch (const utf8::invalid_utf16&) {
        /* Exception thrown: bad UTF16 encoding */
        utf8_line = "";
    }
    return utf8_line;
}

// This needs to be cleaned up:
inline std::wstring safe_utf8to16(std::string s) {
    std::wstring utf16_line;
    try {
        utf8::utf8to16(s.begin(), s.end(), back_inserter(utf16_line));
    } catch (const utf8::invalid_utf8&) {
        /* Exception thrown: bad UTF16 encoding */
        utf16_line = L"";
    }
    return utf16_line;
}

#ifndef HAVE_ISXDIGIT
inline int isxdigit(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
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

inline std::string microsoftDateToISODate(const uint64_t& time) {
    time_t tmp = (time / ONE_HUNDRED_NANO_SEC_TO_SECONDS) - SECONDS_BETWEEN_WIN32_EPOCH_AND_UNIX_EPOCH;

    struct tm time_tm;
    gmtime_r(&tmp, &time_tm);
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &time_tm); // Zulu time
    return std::string(buf);
}

/* Convert Unix timestamp to ISO format */
inline std::string unixTimeToISODate(const uint64_t t) {
    struct tm time_tm;
    time_t tmp = t;
    gmtime_r(&tmp, &time_tm);
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &time_tm); // Zulu time
    return std::string(buf);
}

/* Many internal windows and Linux structures require a valid printable name in ASCII */
inline bool validASCIIName(const std::string name) {
    for (auto ch : name) {
        if (ch & 0x80) return false;  // high bit should not be set
        if (ch < ' ') return false;   // should not be control character
        if (ch == 0x7f) return false; // DEL is not printable
    }
    return true;
}

inline std::filesystem::path NamedTemporaryDirectory() {
    char tmpl[] = "/tmp/be_dirXXXXXX";
    if (mkdtemp(tmpl) == nullptr) { throw std::runtime_error("NamedTemporaryDirectory: Cannot create directory"); }
    return std::filesystem::path(tmpl);
}

inline bool directory_empty(std::filesystem::path path) {
    namespace fs = std::filesystem;
    if (fs::is_directory(path)) {
        for (const auto& it : fs::directory_iterator(path)) {
            (void)it;
            return false;
        }
    }
    return true;
}

#endif
