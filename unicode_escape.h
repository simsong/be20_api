/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef UNICODE_ESCAPE_H
#define UNICODE_ESCAPE_H

#include <string>
#include <cstdint>

/** \addtogroup bulk_extractor_APIs
 * @{
 */
/** \file */

std::string hexesc(unsigned char ch);
bool utf8cont(unsigned char ch);
bool valid_utf8codepoint(uint32_t unichar);
std::string validateOrEscapeUTF8(const std::string &input, bool escape_bad_UTF8,bool escape_backslash,bool validateOrEscapeUTF8_validate);

#endif
