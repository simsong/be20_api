/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/*
 * Tools for working with Unicode
 */

#ifndef UNICODE_ESCAPE_H
#define UNICODE_ESCAPE_H

#include <codecvt>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <iostream>
#include <locale>
#include <string>

#include "utf8.h"

/** \addtogroup bulk_extractor_APIs
 * @{
 */
/** \file */

/* Our standard escaping is \\ for backslash and \x00 for null, etc. */

std::string hexesc(unsigned char ch);       // escape this character
bool utf8cont(unsigned char ch);            // true if a UTF8 continuation character
bool valid_utf8codepoint(uint32_t unichar); // not all unichars are valid codepoints

/* Our internal, testable, somewhat broken Unicode handling */
const std::u32string utf32_lowercase(const std::u32string& str);
const std::u32string utf32_extract_numeric(const std::u32string& str);

struct unicode {
    static const uint16_t INTERLINEAR_ANNOTATION_ANCHOR = 0xFFF9;
    static const uint16_t INTERLINEAR_ANNOTATION_SEPARATOR = 0xFFFA;
    static const uint16_t INTERLINEAR_ANNOTATION_TERMINATOR = 0xFFFB;
    static const uint16_t OBJECT_REPLACEMENT_CHARACTER = 0xFFFC;
    static const uint16_t REPLACEMENT_CHARACTER = 0xFFFD;
    static const uint16_t BOM = 0xFEFF;
};

/* Create safe UTF8 from unsafe UTF8.
 * if validate is true and the others are false, throws an exception with bad UTF8.
 */
std::string validateOrEscapeUTF8(const std::string& input, bool escape_bad_UTF8, bool escape_backslash, bool validate);

/* Guess if this is valid utf16 and return likely endian */
bool looks_like_utf16(const std::string& str, bool& little_endian);

/* These return the string. If no conversion is possible,
 * they throw const utf8::invalid_utf16.
 * catch with 'catch (const utf8::invalid_utf16 &)'
 */

std::string convert_utf16_to_utf8(const std::string& str, bool little_endian); // request specific conversion
std::string convert_utf16_to_utf8(const std::string& str);                     // guess for best

// std::u32string convert_utf16_to_utf32(const std::string &str,bool little_endian); // request specific conversion

// std::u32string convert_utf8_to_utf32(const std::string &str);
// std::string convert_utf32_to_utf8(const std::u32string &str);
// std::string convert_utf32_to_utf8(const std::u32string &str);
std::u32string convert_utf16_to_utf32(const std::string& str);
std::u16string convert_utf32_to_utf16(const std::u32string& str);
std::string make_utf8(const std::string& str); // returns valid, escaped UTF8 for utf8 or utf16

inline const std::u32string utf32_lowercase(const std::u32string& str) {
    std::u32string output;
    for (auto& ch : str) { output.push_back(ch < 0xffff ? tolower(ch) : ch); }
    return output;
}

inline const std::u32string utf32_extract_numeric(const std::u32string& str) {
    std::u32string output;
    for (auto& ch : str) {
        if (iswdigit(ch)) { output.push_back(ch); }
    }
    return output;
}

/* Standards Compliant */
// https://en.cppreference.com/w/cpp/locale/wstring_convert/from_bytes
// https://stackoverflow.com/questions/38688417/utf-conversion-functions-in-c11
inline const std::u16string convert_utf8_to_utf16(const std::string& utf8) {
    return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(utf8.data());
}

inline const std::u32string convert_utf8_to_utf32(const std::string utf8) {
    return std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>{}.from_bytes(utf8);
}

// https://stackoverflow.com/questions/48474227/during-conversation-from-utf32-to-utf8-using-utf8-cpp-i-get-an-error-utf8inva
inline const std::string convert_utf32_to_utf8(const std::u32string& u32s) {
    std::string u8s;
    utf8::utf32to8(u32s.begin(), u32s.end(), std::back_inserter(u8s));
    return u8s;
    // std::wstring_convert<std::codecvt_utf8<int32_t>, int32_t> convert;
    //    auto p = reinterpret_cast<const int32_t *>(s.data());
    //    return convert.to_bytes(p, p + s.size());
    // return std::string();
}

#endif
