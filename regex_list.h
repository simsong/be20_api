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
 * The regex_list maintains a list of regular expressions.
 * The list can be read out of a file.
 * check() returns true if the provided string is inside the list
 * This should be combined with the word_and_context_list
 */
class regex_list {
    std::vector<std::regex> patterns;
 public:
    static bool is_regex(const std::string &str); // is this a regular expression?
    const char *regex_engine();                   // which engine is in use
    regex_list():patterns(){}

    size_t size(){                      // number of regexes
        return patterns.size();
    }
    /**
     * Read a file; returns 0 if successful, -1 if failure.
     * @param fname - the file to read.
     */
    void add_regex(const std::string &pat); // add a single regex to the list
    int  readfile(const std::string &fname); // read a file of regexes, one per line
    /** Return true if any of the regexes match.
     * check() is threadsafe. 
     */
    bool check(const std::string &probe,std::string *found, size_t *offset,size_t *len) const;
};


#endif
