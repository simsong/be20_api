/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef FEATURE_RECORDER_SET_H
#define FEATURE_RECORDER_SET_H

#include "feature_recorder.h"
#include "cppmutex.h"
#include "dfxml/src/dfxml_writer.h"
#include "dfxml/src/hash_t.h"
#include "word_and_context_list.h"
#include <map>
#include <set>

/** \addtogroup internal_interfaces
 * @{
 */
/** \file */

/* histogram_def should be within the feature_recorder_set class. Oh well. */
class histogram_def {
 public:
    /**
     * @param feature- the feature file to histogram (no .txt)
     * @param re     - the regular expression to extract
     * @param require- require this string on the line (usually in context)
     * @param suffix - the suffix to add to the histogram file after feature name before .txt
     * @param flags  - any flags (see above)
     */

    histogram_def(string feature_,string re_,string suffix_,uint32_t flags_=0):
        feature(feature_),pattern(re_),require(),suffix(suffix_),flags(flags_){}
    histogram_def(string feature_,string re_,string require_,string suffix_,uint32_t flags_=0):
        feature(feature_),pattern(re_),require(require_),suffix(suffix_),flags(flags_){}
    string feature;                     /* feature file */
    string pattern;                     /* extract pattern; "" means use entire feature */
    string require;
    string suffix;                      /* suffix to append; "" means "histogram" */
    uint32_t flags;                     // defined in histogram.h
};

typedef  set<histogram_def> histograms_t;

inline bool operator <(class histogram_def h1,class histogram_def h2)  {
    if (h1.feature<h2.feature) return true;
    if (h1.feature>h2.feature) return false;
    if (h1.pattern<h2.pattern) return true;
    if (h1.pattern>h2.pattern) return false;
    if (h1.suffix<h2.suffix) return true;
    if (h1.suffix>h2.suffix) return false;
    return false;                       /* equal */
};

inline bool operator !=(class histogram_def h1,class histogram_def h2)  {
    return h1.feature!=h2.feature || h1.pattern!=h2.pattern || h1.suffix!=h2.suffix;
};


/**
 * \class feature_recorder_set
 * A singleton class that holds a set of recorders.
 * This used to be done with a set, but now it's done with a map.
 * 
 */

typedef std::map<string,class feature_recorder *> feature_recorder_map;
typedef std::set<string>feature_file_names_t;
class feature_recorder_set {
    // neither copying nor assignment is implemented 
    feature_recorder_set(const feature_recorder_set &fs);
    feature_recorder_set &operator=(const feature_recorder_set &fs);
    uint32_t flags;
    atomic_set<std::string> seen_set;   // hex hash values of pages that have been seen
    std::string           input_fname;      // input file
    std::string           outdir;           // where output goes
    feature_recorder_map  frm;              // map of feature recorders, by name
    cppmutex              map_lock;         // locks frm and scanner_stats_map
    const histograms_t    *histogram_defs;  // histograms that are to be created.
public:
    struct pstats {
        double seconds;
        uint64_t calls;
    };
    typedef map<std::string,struct pstats> scanner_stats_map;

    const word_and_context_list *alert_list;		/* shold be flagged */
    const word_and_context_list *stop_list;		/* should be ignored */
    scanner_stats_map     scanner_stats;

    static const string   ALERT_RECORDER_NAME;  // the name of the alert recorder
    static const string   DISABLED_RECORDER_NAME; // the fake disabled feature recorder
    /* flags */
    static const uint32_t ONLY_ALERT=0x01;      // always return the alert recorder
    static const uint32_t SET_DISABLED=0x02;    // the set is effectively disabled; for path-printer
    static const uint32_t CREATE_STOP_LIST_RECORDERS=0x04; // 

    virtual ~feature_recorder_set() {
        for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
            delete i->second;
        }
    }

    std::string get_input_fname() const {return input_fname;}
    std::string get_outdir() const {return outdir;}
    void set_stop_list(const word_and_context_list *alist){stop_list=alist;}
    void set_alert_list(const word_and_context_list *alist){alert_list=alist;}

    /** create an emptry feature recorder set. If disabled, create a disabled recorder. */
    feature_recorder_set(uint32_t flags_);

    /** Initialize a feature_recorder_set. Previously this was a constructor, but it turns out that
     * virtual functions for the create_name_factory aren't honored in constructors.
     */
    void init(const feature_file_names_t &feature_files,
              const std::string &input_fname,const std::string &outdir,
              const histograms_t *histogram_defs);

    void    flush_all();
    void    close_all();
    bool    has_name(string name) const;           /* does the named feature exist? */
    void    set_flag(uint32_t f){flags|=f;}         
    void    clear_flag(uint32_t f){flags|=f;}

    typedef void (*xml_notifier_t)(const std::string &xmlstring);
    void    process_histograms(xml_notifier_t xml_error_notifier);


    virtual feature_recorder *create_name_factory(const std::string &outdir_,
                                                  const std::string &input_fname_,const std::string &name_);
    virtual void create_name(const std::string &name,bool create_stop_also);
    virtual const std::string &get_outdir(){ return outdir;}

    void    add_stats(string bucket,double seconds);
    typedef void (*stat_callback_t)(void *user,const std::string &name,uint64_t calls,double seconds);
    void    get_stats(void *user,stat_callback_t stat_callback);
    void    dump_name_count_stats(dfxml_writer &writer);

    // Management of previously seen data
    virtual bool check_previously_processed(const uint8_t *buf,size_t bufsize);

    // NOTE:
    // only virtual functions may be called by plugins!
    virtual feature_recorder *get_name(const std::string &name);
    virtual feature_recorder *get_alert_recorder();
};


#endif
