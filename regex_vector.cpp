/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "regex_vector.h"

/* rewritten to use C++11's regex */
const std::string regex_vector::RE_ENGINE {"RE_ENGINE"};
const std::string regex_vector::regex_engine()
{
#ifdef HAVE_RE2
    if (engine_enabled("RE2")) {
        return std::string("RE2");
    }
#endif
#ifdef HAVE_PCRE
    if (engine_enabled("PCRE")){
        return std::string("PCRE");
    }
#endif
    return std::string("STD::REGEX");
}

regex_vector::~regex_vector()
{
    clear();
}


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

void regex_vector::push_back(const std::string& val) {
    RE2::Options options;
    options.set_case_sensitive(false);

#ifdef HAVE_RE2
    if (engine_enabled("RE2")){
        regex_strings.push_back(val);
        RE2 *re = new RE2(std::string("(") + val + std::string(")"), options);
        if (!re->ok()){
            std::cerr << "RE2 compilation failed error: " << re->error() << " compiling: " << val << std::endl;
            throw std::runtime_error(std::string("RE2 compilation failed"));
        }
        re2_regex_comps.push_back( re );
        return;
    }
#endif
#ifdef HAVE_PCRE
    if (engine_enabled("PCRE")){
        const char *error = nullptr;
        int erroffset = 0;
        auto re = pcre_compile( val.c_str(), // pattern
                                0,           // default options
                                &error,      // for error message
                                &erroffset,  // for error offset,
                                NULL);       // use default error tables
        if (re == nullptr) {
            std::cerr << "PCRE compliation failed at offset " << erroffset << ": " << error << "  compiling: " << val << std::endl;
            throw std::runtime_error(std::string("PCRE compilation failed"));
        }
        pcre_extra *extra = pcre_study(re, PCRE_STUDY_JIT_COMPILE, &error);
        pcre_jit_stack *jit_stack = pcre_jit_stack_alloc(32*1024, 16*1024*1024);
        pcre_assign_jit_stack(extra, NULL, jit_stack);
        pcre_regex_comps.push_back( p2(re,extra,jit_stack) );
        return;
    }
#endif
    regex_chars.push_back(std::regex(val, std::regex_constants::icase));
}

void regex_vector::clear() {
    regex_strings.clear();
#ifdef HAVE_RE2
    for (RE2 *re: re2_regex_comps) {
        delete re;
    }
    re2_regex_comps.clear();
#endif
#ifdef HAVE_PCRE
    for (auto const &it: pcre_regex_comps) {
        pcre_free(it.re);
        pcre_free_study(it.extra);
        pcre_jit_stack_free(it.jit_stack);
    }
    pcre_regex_comps.clear();
#endif
    regex_chars.clear();
}

size_t regex_vector::size() const {
    size_t sz = 0;
#ifdef HAVE_RE2
    sz += re2_regex_comps.size();
#endif
#ifdef HAVE_PCRE
    sz += pcre_regex_comps.size();
#endif
    sz += regex_chars.size();
    return sz;
}

/**
 * perform a search for a single hit. If there is a group and something is found,
 * set *found to be what was found, *offset to be the starting offset, and *len to be
 * the length. Note that this only handles a single group.
 */
bool regex_vector::search_all(const std::string& probe, std::string* found, size_t* offset, size_t* len) const {
#ifdef HAVE_RE2
    for (RE2 *re: re2_regex_comps) {
        re2::StringPiece sp;
        if (RE2::PartialMatch( probe, *re, &sp) ){
            if (found)  *found  = std::string(sp.data(), sp.size());
            if (offset) *offset = sp.data() - probe.data(); // this is so gross
            if (len)    *len    = sp.length();
            return true;
        }
    }
#endif
#ifdef HAVE_PCRE
    const int MAX_PCRE_SIZE = 1024;
    const int PCRE_WINDOW = 512;
    for (auto &it : pcre_regex_comps) {
        for (auto probe_offset=0;probe_offset < probe.size(); probe_offset+=MAX_PCRE_SIZE) {
            auto window_start = probe.c_str()+probe_offset;
            auto window_len   = probe.size()-probe_offset;
            if (window_len > MAX_PCRE_SIZE+PCRE_WINDOW){
                window_len = MAX_PCRE_SIZE+PCRE_WINDOW;
            }
            size_t const OVECCOUNT=3;       // must be divisble by three
            int ovector[OVECCOUNT];
            int rc = pcre_exec(it.re,         // the compiled pattern
                               it.extra,       // no extra data
                               window_start, // the subject
                               window_len,  // the length of the subject,
                               0,             // start at offset 0 in the subject
                               0,             // default options
                               ovector,       // output vector for substring information
                               OVECCOUNT);
            if (rc>=0){
                size_t substring_start = ovector[0];
                size_t substring_len   = ovector[1] - ovector[0];
                if (found)  *found  = probe.substr(probe_offset+substring_start, substring_len);
                if (offset) *offset = probe_offset + substring_start;
                if (len)    *len    = substring_len;
                return true;
            }
        }
    }
#endif
    /* default to std::regex */
    const int MAX_STD_SIZE = 1024;
    const int STD_WINDOW = 128;
    for (auto &it : regex_chars ) {
        for (auto probe_offset=0;probe_offset < probe.size(); probe_offset+=MAX_STD_SIZE) {
            std::string short_probe = probe.substr(probe_offset, MAX_STD_SIZE+STD_WINDOW);
            std::smatch sm;
            std::regex_search(short_probe, sm, it);
            if (sm.size() > 0) {
                if (found) *found = sm.str();
                if (offset) *offset = probe_offset + sm.position();
                if (len) *len = sm.length();
                return true;
            }
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
    for (auto const& it : regex_strings) {
        os << it << "\n";
    }
}

std::ostream& operator<<(std::ostream& os, const class regex_vector& rv) {
    rv.dump(os);
    return os;
}
