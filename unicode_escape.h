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

std::string hexesc(unsigned char ch);
bool utf8cont(unsigned char ch);
bool valid_utf8codepoint(uint32_t unichar);
std::string validateOrEscapeUTF8(const std::string &input,
                                 bool escape_bad_UTF8,bool escape_backslash,
                                 bool validateOrEscapeUTF8_validate);

bool looks_like_utf16(const std::string &str,bool &little_endian);

/* These return the string. If no conversion is possible, 
 * they throw const utf8::invalid_utf16.
 * catch with 'catch (const utf8::invalid_utf16 &)'
 */

std::string convert_utf16_to_utf8(const std::string &key,bool little_endian); // request specific conversion
std::string convert_utf16_to_utf8(const std::string &key); // guess for best

//std::string make_utf8(const std::string &key);

#endif
