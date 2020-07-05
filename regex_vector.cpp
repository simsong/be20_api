/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"

#include <sys/types.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#include "regex_vector.h"

/* rewritten to use C++11's regex */

const std::string regex_vector::regex_engine(){
    return std::string("std-c++11");
}

/* Only certain characters are assumed to be a regular expression. These characters are
 * coincidently never in email addresses.
 */
bool regex_vector::has_metachars(const std::string &str)
{
    for ( auto &it : str ) {
        switch(it){
        case '*':
        case '[':
        case '(':
        case '?':
            return true;
        }
    }
    return false;
}

/**
 * perform a search for a single hit. If there is a group and something is found,
 * set *found to be what was found, *offset to be the starting offset, and *len to be
 * the length. Note that this only handles a single group.
 */
const std::smatch regex_vector::search_all(const std::string &probe) const
{
    for ( auto it : regex_chars ) {
        std::smatch sm;
        std::regex_match( probe, sm, it);
        if (sm.size()>0) {
            return sm;
        }
    }
    return std::smatch();               // no match
}

int regex_vector::readfile(const std::string &fname)
{
    std::ifstream f(fname.c_str());
    if(f.is_open()){
        while(!f.eof()){
            std::string line;
            getline(f,line);

            /* remove the last character while it is a \n or \r */
            if(line.size()>0 && (((*line.end())=='\r') || (*line.end())=='\n')){
                line.erase(line.end());
            }
            
            /* Create a regular expression and add it */
            regex_chars.push_back(std::regex(line));
        }
        f.close();
        return 0;
    }
    return -1;
}

