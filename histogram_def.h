#ifndef HISTOGRAM_DEF_H
#define HISTOGRAM_DEF_H

#include <string>
#include <regex>

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


    /**
     * @param feature- the feature file to histogram (no .txt)
     * @param re     - the regular expression to extract. 
     * @param require- require this string on the line (usually in context)
     * @param suffix - the suffix to add to the histogram file after feature name before .txt
     * @param flags  - any flags (see above)
     */

    histogram_def(const std::string &feature_,
                  const std::string &pattern_,
                  const std::string &suffix_,
                  const struct flags_t &flags_):
        feature(feature_),pattern(pattern_),require(),
        suffix(suffix_),reg(pattern),flags(flags_) {
    }

    /* feature file name */
    std::string feature {};

    /* regular expression used to extract feature substring from feature. "" means use the entire feature*/
    std::string pattern {};

    /* text required somewhere on the feature line. Sort of like grep. used for IP histograms */
    std::string require {};

    /* suffix to append to histogram report name */
    std::string suffix {};

    /* the compiled regular expression. Since it is compiled, it need not be used in comparisions */
    mutable std::regex  reg {};

    /* flags */
    struct flags_t flags {};
};

#endif
