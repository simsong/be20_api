/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * beregex.h:
 * 
 * simple cover for regular expression class.
 * The class allocates and frees the strings 
 */

#ifndef BEREGEX_H
#define BEREGEX_H

#ifdef HAVE_TRE_TRE_H
# include <tre/tre.h>
#else
# ifdef HAVE_REGEX_H
#  include <regex.h>
# endif
#endif

#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <string.h>
#include <vector>
#include <set>
#include <regex>

/**
 * The regex_vector is a vector of character regexes with a few additional convenience functions.
 * We might want to change this to handle ASCII, UTF-16 and UTF-8 characters simultaneously.
 */
class regex_vector {
    std::vector<std::regex> regex_chars;
    regex_vector(const regex_vector &) = delete;
    regex_vector &operator=(const regex_vector &) = delete;
 public:
    static bool has_metachars(const std::string &str);       // is this a regular expression with meta characters?
    const std::string regex_engine();                // which engine is in use

    void push_back( const std::regex &val) {
        regex_chars.push_back(val);
    }

    auto size() {
        return regex_chars.size();
    }

    /**
     * Read regular expressions from a file: returns 0 if successful, -1 if failure.
     * @param fname - the file to read.
     */
    int  readfile(const std::string &fname); // read a file of regexes, one per line


    /** Run Return true if any of the regexes match.
     * check() is threadsafe. 
     * @param probe  - the string we are searching.
     * returns a match structure
     */

    const std::smatch search_all(const std::string &probe) const;
};


#endif
