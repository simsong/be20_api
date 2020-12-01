/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/**
 * unicode_escape.cpp:
 * Escape unicode that is not valid.
 *
 * References:
 * http://www.ietf.org/rfc/rfc3987.txt
 * http://en.wikipedia.org/wiki/UTF-8
 *
 * @author Simson Garfinkel
 *
 * The software provided here is released by the Naval Postgraduate
 * School, an agency of the U.S. Department of Navy.  The software
 * bears no warranty, either expressed or implied. NPS does not assume
 * legal liability nor responsibility for a User's use of the software
 * or the results of such use.
 *
 * Please note that within the United States, copyright protection,
 * under Section 105 of the United States Code, Title 17, is not
 * available for any work of the United States Government and/or for
 * any works created by United States Government employees. User
 * acknowledges that this software contains work which was created by
 * NPS government employees and is therefore in the public domain and
 * not subject to copyright.
 */

#include <cstdio>
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdint>

#include "config.h"
#include "unicode_escape.h"
#include "utf8.h"

std::string hexesc(unsigned char ch)
{
    char buf[10];
    snprintf(buf,sizeof(buf),"\\x%02X",ch);
    return std::string(buf);
}

/** returns true if this is a UTF8 continuation character */
bool utf8cont(unsigned char ch)
{
    return ((ch&0x80)==0x80) &&  ((ch & 0x40)==0);
}

/**
 * After a UTF-8 sequence is decided, this function is called
 * to determine if the character is invalid. The UTF-8 spec now
 * says that if a UTF-8 decoding produces an invalid character, or
 * a surrogate, it is not valid. (There were some nasty security
 * vulnerabilities that were exploited before this came out.)
 * So we do a lot of checks here.
 */
bool valid_utf8codepoint(uint32_t unichar)
{
    // Check for invalid characters in the bmp
    switch(unichar){
    case 0xfffe: return false;          // reversed BOM
    case 0xffff: return false;
    default:
        break;
    }
    if(unichar >= 0xd800 && unichar <=0xdfff) return false; // high and low surrogates
    if(unichar < 0x10000) return true;  // looks like it is in the BMP

    // check some regions outside the bmp

    // Plane 1:
    if(unichar > 0x13fff && unichar < 0x16000) return false;
    if(unichar > 0x16fff && unichar < 0x1b000) return false;
    if(unichar > 0x1bfff && unichar < 0x1d000) return false;

    // Plane 2
    if(unichar > 0x2bfff && unichar < 0x2f000) return false;

    // Planes 3--13 are unassigned
    if(unichar >= 0x30000 && unichar < 0xdffff) return false;

    // Above Plane 16 is invalid
    if(unichar > 0x10FFFF) return false;        // above plane 16?

    return true;                        // must be valid
}

/**
 * validateOrEscapeUTF8
 * Input: UTF8 string (possibly corrupt)
 * Input: do_escape, indicating whether invalid encodings shall be escaped.
 * Note:
 *    - if not escaping but an invalid encoding is present and DEBUG_PEDANTIC is set, then assert() is called.
 *    - DO NOT USE wchar_t because it is 16-bits on Windows and 32-bits on Unix.
 * Output:
 *   - UTF8 string.  If do_escape is set, then corruptions are escaped in \xFF notation where FF is a hex character.
 */

std::string validateOrEscapeUTF8(const std::string &input,
                                 bool escape_bad_utf8,
                                 bool escape_backslash,
                                 bool validateOrEscapeUTF8_validate)
{
    // skip the validation if not escaping and not DEBUG_PEDANTIC
    if (escape_bad_utf8==false && escape_backslash==false && validateOrEscapeUTF8_validate==false){
        return input;
    }

    // validate or escape input
    std::string output;
    for(std::string::size_type i = 0; i< input.length(); ) {
        uint8_t ch = (uint8_t)input.at(i);

        // utf8 1 byte prefix (0xxx xxxx)
        if((ch & 0x80)==0x00){          // 00 .. 0x7f
            if(ch=='\\' && escape_backslash){   // escape the escape character as \x92
                output += hexesc(ch);
                i++;
                continue;
            }

            if( ch < ' '){              // not printable are escaped
                output += hexesc(ch);
                i++;
                continue;
            }
            output += ch;               // printable is not escaped
            i++;
            continue;
        }

        // utf8 2 bytes  (110x xxxx) prefix
        if(((ch & 0xe0)==0xc0)  // 2-byte prefix
           && (i+1 < input.length())
           && utf8cont((uint8_t)input.at(i+1))){
            uint32_t unichar = (((uint8_t)input.at(i) & 0x1f) << 6) | (((uint8_t)input.at(i+1) & 0x3f));

            // check for valid 2-byte encoding
            if(valid_utf8codepoint(unichar)
               && ((uint8_t)input.at(i)!=0xc0)
               && (unichar >= 0x80)){
                output += (uint8_t)input.at(i++);       // byte1
                output += (uint8_t)input.at(i++);       // byte2
                continue;
            }
        }

        // utf8 3 bytes (1110 xxxx prefix)
        if(((ch & 0xf0) == 0xe0)
           && (i+2 < input.length())
           && utf8cont((uint8_t)input.at(i+1))
           && utf8cont((uint8_t)input.at(i+2))){
            uint32_t unichar = (((uint8_t)input.at(i) & 0x0f) << 12)
                | (((uint8_t)input.at(i+1) & 0x3f) << 6)
                | (((uint8_t)input.at(i+2) & 0x3f));

            // check for a valid 3-byte code point
            if(valid_utf8codepoint(unichar)
               && unichar>=0x800){
                output += (uint8_t)input.at(i++);       // byte1
                output += (uint8_t)input.at(i++);       // byte2
                output += (uint8_t)input.at(i++);       // byte3
                continue;
            }
        }

        // utf8 4 bytes (1111 0xxx prefix)
        if((( ch & 0xf8) == 0xf0)
           && (i+3 < input.length())
           && utf8cont((uint8_t)input.at(i+1))
           && utf8cont((uint8_t)input.at(i+2))
           && utf8cont((uint8_t)input.at(i+3))){
            uint32_t unichar =( (((uint8_t)input.at(i) & 0x07) << 18)
                                |(((uint8_t)input.at(i+1) & 0x3f) << 12)
                                |(((uint8_t)input.at(i+2) & 0x3f) <<  6)
                                |(((uint8_t)input.at(i+3) & 0x3f)));

            if(valid_utf8codepoint(unichar) && unichar>=0x1000000){
                output += (uint8_t)input.at(i++);       // byte1
                output += (uint8_t)input.at(i++);       // byte2
                output += (uint8_t)input.at(i++);       // byte3
                output += (uint8_t)input.at(i++);       // byte4
                continue;
            }
        }

        if (escape_bad_utf8) {
            // Just escape the next byte and carry on
            output += hexesc((uint8_t)input.at(i++));
        } else {
            // fatal if we are debug pedantic, otherwise just ignore
            // note: we shouldn't be here anyway, since if we are not escaping and we are not
            // pedantic we should have returned above
            if(validateOrEscapeUTF8_validate){
                std::ofstream os("bad_unicode.txt");
                os << input << "\n";
                os.close();
                throw std::runtime_error("INTERNAL ERROR: bad unicode stored in bad_unicode.txt\n");
            }
        }
    }
    return output;
}

/* static */
bool looks_like_utf16(const std::string &str,bool &little_endian)
{
    if((uint8_t)str[0]==0xff && (uint8_t)str[1]==0xfe){
	little_endian = true;
	return true; // begins with FFFE
    }
    if((uint8_t)str[0]==0xfe && (uint8_t)str[1]==0xff){
	little_endian = false;
	return true; // begins with FFFE
    }
    /* If none of the even characters are NULL and some of the odd characters are NULL, it's UTF-16 */
    uint32_t even_null_count = 0;
    uint32_t odd_null_count = 0;
    for(size_t i=0;i+1<str.size();i+=2){
	if(str[i]==0) even_null_count++;
	if(str[i+1]==0) odd_null_count++;
    }
    if(even_null_count==0 && odd_null_count>1){
	little_endian = true;
	return true;
    }
    if(odd_null_count==0 && even_null_count>1){
	little_endian = false;
	return true;
    }
    return false;
}

/**
 * Converts a utf16 with a byte order to utf8.
 *
 */
/* static */
std::string convert_utf16_to_utf8(const std::string &key,bool little_endian)
{
    /* re-image this string as UTF16*/
    std::wstring utf16;
    for(size_t i=0;i<key.size();i+=2){
        if(little_endian) utf16.push_back(key[i] | (key[i+1]<<8));
        else utf16.push_back(key[i]<<8 | (key[i+1]));
    }
    /* Now convert it to a UTF-8;
     * set tempKey to be the utf-8 string that will be erased.
     */
    std::string tempKey;
    utf8::utf16to8(utf16.begin(),utf16.end(),std::back_inserter(tempKey));
    /* Erase any nulls if present */
    while(tempKey.size()>0) {
        size_t nullpos = tempKey.find('\000');
        if (nullpos==std::string::npos) break;
        tempKey.erase(nullpos,1);
    }
    return tempKey;
}

std::string convert_utf16_to_utf8(const std::string &key)
{
    bool little_endian=false;
    if(looks_like_utf16(key,little_endian)){
        return convert_utf16_to_utf8(key, little_endian);
    }
    throw utf8::invalid_utf16(0);
}

std::string make_utf8(const std::string &str)
{
    try {
        return convert_utf16_to_utf8(str);
    }
    catch (const utf8::invalid_utf16 &){
        return validateOrEscapeUTF8(str, true, true, true);
    }
}
