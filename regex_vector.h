/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * regex_vector.h:
 *
 * Now this covers Google's RE2 library.
 * Note:
 *  1 - RE2 and the objects are not move insertable, so we need to manually manage creating and deleting them.
 *  2 - RE2's PartialMatch function wont' return the position of a match unless it is wrapped in a group " () ",
        so we do that.
 */

#ifndef REGEX_VECTOR_H
#define REGEX_VECTOR_H

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <re2/re2.h>

/**
 * The regex_vector is a vector of character regexes with a few additional convenience functions.
 * We might want to change this to handle ASCII, UTF-16 and UTF-8 characters simultaneously.
 */
class regex_vector {
    std::vector<std::string> regex_strings; // the original regex strings
    std::vector<RE2 *> regex_comps;         // the compiled regular expressions
    regex_vector(const regex_vector&) = delete;
    regex_vector& operator=(const regex_vector&) = delete;

public:
    regex_vector() : regex_strings(), regex_comps(){};
    ~regex_vector() {
        clear();
    }

    // is this a regular expression with meta characters?
    static bool has_metachars(const std::string& str);
    const std::string regex_engine(); // which engine is in use

    /* Add a string */
    void push_back(const std::string& val) {
        RE2::Options options;
        options.set_case_sensitive(false);

        regex_strings.push_back(val);
        RE2 *re = new RE2(std::string("(") + val + std::string(")"), options);
        if (!re->ok()){
            std::cerr << "regex error: " << re->error() << std::endl;
            assert(false);
        }
        regex_comps.push_back( re );
    }

    // Empty the vectors. For the compiled, be sure to delete them
    void clear() {
        regex_strings.clear();
        for (RE2 *re: regex_comps) {
            delete re;
        }
        regex_comps.clear();
    }
    auto size() { return regex_comps.size(); }

    /**
     * Read regular expressions from a file: returns 0 if successful, -1 if failure.
     * @param fname - the file to read.
     */
    int readfile(const std::string& fname); // read a file of regexes, one per line

    /** Run Return true if any of the regexes match.
     * search_all() is threadsafe.
     * @param probe  - the string we are searching.
     * *found - set to the found string if something is found.
     */

    bool search_all(const std::string& probe,
                    std::string* found,
                    size_t* offset = nullptr,
                    size_t* len = nullptr) const;
    void dump(std::ostream& os) const;
};

std::ostream& operator<<(std::ostream& os, const class regex_vector& rv);

#endif
