/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/*
 * Tools for working with Unicode
 */

#ifndef UNICODE_ESCAPE_H
#define UNICODE_ESCAPE_H

#include <string>
#include <cstdint>
#include <cstring>

/** \addtogroup bulk_extractor_APIs
 * @{
 */
/** \file */

/* Our standard escaping is \\ for backslash and \x00 for null, etc. */

std::string hexesc(unsigned char ch);   // escape this character
bool utf8cont(unsigned char ch);        // true if a UTF8 continuation character
bool valid_utf8codepoint(uint32_t unichar); // not all unichars are valid codepoints

/* Create safe UTF8 from unsafe UTF8.
 * if validate is true and the others are false, throws an exception with bad UTF8.
 */
std::string validateOrEscapeUTF8(const std::string &input, bool escape_bad_UTF8, bool escape_backslash, bool validate);

/* Guess if this is valid utf16 and return likely endian */
bool looks_like_utf16(const std::string &str,bool &little_endian);

/* These return the string. If no conversion is possible,
 * they throw const utf8::invalid_utf16.
 * catch with 'catch (const utf8::invalid_utf16 &)'
 */

std::string convert_utf16_to_utf8(const std::string &str,bool little_endian); // request specific conversion
std::string convert_utf16_to_utf8(const std::string &str); // guess for best
std::string make_utf8(const std::string &str); // returns valid, escaped UTF8 for utf8 or utf16

#endif
