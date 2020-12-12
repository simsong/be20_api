#ifndef HISTOGRAM_DEF_H
#define HISTOGRAM_DEF_H

#include <string>
#include <regex>
#include <cstdio>
#include <string>
#include <iostream>

#include "unicode_escape.h"

/**
 * histogram_def defines the histograms that will be made by a feature recorder.
 * If the mhistogram is set, the histogram is generated when features are recorded
 * and kept in memory. If mhistogram is not set, the histogram is generated when the feature recorder is closed.
 */

struct histogram_def {
    struct flags_t {
        flags_t(const flags_t &a){
            this->lowercase = a.lowercase;
            this->numeric   = a.numeric;
        };

        flags_t &operator=(const flags_t &a){
            this->lowercase = a.lowercase;
            this->numeric   = a.numeric;
            return *this;
        };

        bool operator<(const flags_t &a) const {
            if (this->lowercase < a.lowercase) return true;
            if (this->lowercase > a.lowercase) return false;
            if (this->numeric   < a.numeric) return true;
            return false;
        }

        bool operator==(const flags_t &a) const {
            return (this->lowercase == a.lowercase) && (this->numeric   == a.numeric);
        }


        flags_t(){};
        flags_t(bool lowercase_,bool numeric_): lowercase(lowercase_),numeric(numeric_){}
        bool lowercase {false};         // make all flags lowercase
        bool numeric  {false};           // extract digits only
    };

    /**
     * @param feature- the feature file to histogram (no .txt)
     * @param re     - the regular expression to extract.
     * @param require- require this string on the line (usually in context)
     * @param suffix - the suffix to add to the histogram file after feature name before .txt
     * @param flags  - any flags (see above)
     */

    histogram_def(const std::string &feature_, // which feature file to use
                  const std::string &pattern_, // which pattern to abstract
                  const std::string &suffix_,  // which suffix to add to the feature file name for the histogram
                  const struct flags_t &flags_): // flags - see below
        feature(feature_),pattern(pattern_),require(),
        suffix(suffix_),reg(pattern_),flags(flags_) { }

    std::string feature {};     /* feature file name */
    std::string pattern {};     /* regular expression used to extract feature substring from feature. "" means use the entire feature*/
    std::string require {};         /* text required somewhere on the feature line. Sort of like grep. used for IP histograms */
    std::string suffix {};   /* suffix to append to histogram report name */

    /* the compiled regular expression. Since it is compiled, it need not be used in comparisions */
    mutable std::regex  reg {};

    /* flags */
    struct flags_t flags {};


    /* default copy construction and assignment */
    histogram_def(const histogram_def &a) {
        this->feature = a.feature;
        this->pattern = a.pattern;
        this->require = a.require;
        this->suffix  = a.suffix;
        this->reg     = a.reg;
        this->flags   = a.flags;
    };

    /* assignment operator */
    histogram_def &operator=(const histogram_def &a) {
        this->feature = a.feature;
        this->pattern = a.pattern;
        this->require = a.require;
        this->suffix  = a.suffix;
        this->reg     = a.reg;
        this->flags   = a.flags;
        return *this;
    }

    bool operator==(const histogram_def &a) const {
        return
            (this->feature == a.feature) &&
            (this->pattern == a.pattern) &&
            (this->require == a.require) &&
            (this->suffix  == a.suffix) &&
            (this->flags   == a.flags);
    }

    bool operator!=(const histogram_def &a) const {
        return !(*this == a);
    }

    /* comparator, so we can have a functioning map and set classes.'
     * ignores reg.
     */
    bool operator<(const histogram_def &a) const {
        if (this->feature < a.feature) return true;
        if (this->feature > a.feature) return false;
        if (this->pattern < a.pattern) return true;
        if (this->pattern > a.pattern) return false;
        if (this->require < a.require) return true;
        if (this->require > a.require) return false;
        if (this->suffix < a.suffix) return true;
        if (this->suffix > a.suffix) return false;
        if (this->flags < a.flags) return true;
        return false;
    }

    /* Match and extract:
     * If the string matches this histogram, return true and optionally
     * set match to Extract and match: Does this string match
     */

    bool match(std::u32string u32key, std::string *displayString = nullptr) const {
        if ( flags.lowercase ){
            u32key = utf32_lowercase( u32key );
        }

        if ( flags.numeric ) {
            u32key = utf32_extract_numeric( u32key );
        }

        /* TODO: When we have the ability to do regular expressions in utf32, do that here.
         * We don't have that, so do the rest in utf8 */

        /* Convert match string to u8key */
        std::string u8key = convert_utf32_to_utf8( u32key );


        std::cerr << "u8key="<<u8key<<"\n";

        /* If a string is required and it is not present, return */
        if (require.size() > 0 && u8key.find_first_of(require)==std::string::npos){
            std::cerr << "fail1\n";
            return false;
        }

        /* Check for pattern */
        if (pattern.size() > 0){
            std::smatch m;
            std::regex_search( u8key, m, reg);
            if (m.empty()==true){       // match does not exist
                std::cerr << "fail2  pattern=" << pattern << "\n";
                return false;           // regex not found
            }
            u8key = m.str();
            std::cerr << "m=" << m.str() << " \n";
        }

        if (displayString) {
            *displayString = u8key;
        }
        return true;
    }
    bool match(std::string u32key, std::string *displayString = nullptr) const {
        return match( convert_utf8_to_utf32( u32key), displayString);
    }
};

#endif
