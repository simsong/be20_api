/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "regex_vector.h"

/* rewritten to use C++11's regex */
const std::string regex_vector::regex_engine() { return std::string("RE2"); }

/* Only certain characters are assumed to be a regular expression. These characters are
 * coincidently never in email addresses.
 */
bool regex_vector::has_metachars(const std::string& str) {
    for (auto& it : str) {
        switch (it) {
        case '*':
        case '[':
        case '(':
        case '?': return true;
        }
    }
    return false;
}

/**
 * perform a search for a single hit. If there is a group and something is found,
 * set *found to be what was found, *offset to be the starting offset, and *len to be
 * the length. Note that this only handles a single group.
 */
bool regex_vector::search_all(const std::string& probe, std::string* found, size_t* offset, size_t* len) const {
    for (RE2 *re: regex_comps) {
        re2::StringPiece sp;
        if (RE2::PartialMatch( probe, *re, &sp) ){
            if (found)  *found  = std::string(sp.data(), sp.size());
            if (offset) *offset = sp.data() - probe.data(); // this is so gross
            if (len)    *len    = sp.length();
            return true;
        }
    }
    return false;
}

int regex_vector::readfile(const std::string& fname) {
    std::ifstream f(fname.c_str());
    if (f.is_open()) {
        while (!f.eof()) {
            std::string line;
            getline(f, line);

            /* remove the last character while it is a \n or \r */
            if (line.size() > 0 && (((*line.end()) == '\r') || (*line.end()) == '\n')) { line.erase(line.end()); }

            /* Create a regular expression and add it */
            push_back(line);
        }
        f.close();
        return 0;
    }
    return -1;
}

void regex_vector::dump(std::ostream& os) const {
    for (auto const& it : regex_strings) { os << it << "\n"; }
}

std::ostream& operator<<(std::ostream& os, const class regex_vector& rv) {
    rv.dump(os);
    return os;
}
