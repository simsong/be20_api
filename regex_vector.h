/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * regex_vector.h:
 * 
 * simple cover for regular expression class.
 */

#ifndef REGEX_VECTOR_H
#define REGEX_VECTOR_H

#include <cassert>
#include <cstring>

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <regex>

/**
 * The regex_vector is a vector of character regexes with a few additional convenience functions.
 * We might want to change this to handle ASCII, UTF-16 and UTF-8 characters simultaneously.
 */
class regex_vector {
    std::vector<std::string> regex_strings; // the original regex strings
    std::vector<std::regex>  regex_chars;
    regex_vector(const regex_vector &) = delete;
    regex_vector &operator=(const regex_vector &) = delete;

 public:
    regex_vector():regex_strings(),regex_chars(){};
    // is this a regular expression with meta characters?
    static bool has_metachars(const std::string &str);       
    const std::string regex_engine();                // which engine is in use

    /* Add a string */
    void push_back( const std::string &val) {
        regex_strings.push_back(val);
        regex_chars.push_back(std::regex(val, std::regex_constants::icase));
        assert( regex_strings.size() == regex_chars.size() );
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
     * search_all() is threadsafe. 
     * @param probe  - the string we are searching.
     * *found - set to the found string if something is found.
     */

    bool search_all(const std::string &probe, std::string *found) const;
    void dump(std::ostream &os) const;
};

std::ostream & operator << (std::ostream &os, const class regex_vector &rv);

#endif
