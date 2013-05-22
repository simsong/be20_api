/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef FEATURE_RECORDER_SET_H
#include "feature_recorder.h"

/** \addtogroup internal_interfaces
 * @{
 */
/** \file */

/**
 * \class feature_recorder_set
 * A singleton class that holds a set of recorders.
 * This used to be done with a set, but now it's done with a map.
 * 
 */
#include <map>
#include <set>

typedef std::map<string,class feature_recorder *> feature_recorder_map;
typedef std::set<string>feature_file_names_t;
class feature_recorder_set {
private:
    /*** neither copying nor assignment is implemented ***
     *** We do this by making them private constructors that throw exceptions. ***/
    class not_impl: public exception {
        virtual const char *what() const throw() {
            return "copying feature_recorder objects is not implemented.";
        }
    };
    feature_recorder_set(const feature_recorder_set &fs) __attribute__((__noreturn__)) :
        flags(0),input_fname(),outdir(),frm(),Mlock(),scanner_stats(){ throw new not_impl(); }
    const feature_recorder_set &operator=(const feature_recorder_set &fs){ throw new not_impl(); }
    uint32_t flags;
public:
    // instance data //
    static feature_recorder *alert_recorder;
    std::string input_fname;            // input file
    std::string outdir;                 // where output goes
    feature_recorder_map  frm;          // map of feature recorders
    cppmutex Mlock;            // can be locked even in a const function
    class pstats {
    public:
        double seconds;
        uint64_t calls;
    };
    typedef map<string,class pstats> scanner_stats_map;
    scanner_stats_map scanner_stats;

    static const string   ALERT_RECORDER_NAME;  // the name of the alert recorder
    static const string   DISABLED_RECORDER_NAME; // the fake disabled feature recorder
    static const uint32_t SET_DISABLED=0x02;    // the set is effectively disabled; for path-printer
    static const uint32_t ONLY_ALERT=0x01;      // always return the alert recorder

    /** Create a properly functioning feature recorder set. */
    feature_recorder_set(const feature_file_names_t &feature_files,
                         const std::string &input_fname,
                         const std::string &outdir,
                         bool create_stop_files);

    /** create a dummy feature_recorder_set with no output directory */
    feature_recorder_set(uint32_t flags_):flags(flags_),input_fname(),outdir(),frm(),Mlock(),scanner_stats(){ }
    virtual ~feature_recorder_set() {
        for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
            delete i->second;
        }
    }

    static void get_alert_recorder_name(feature_file_names_t &feature_file_names);

    void flush_all();
    void close_all();
    bool has_name(string name) const;   /* does the named feature exist? */
    void set_flag(uint32_t f){flags|=f;}
    void create_name(string name,bool create_stop_also);
    void clear_flag(uint32_t f){flags|=f;}
    void add_stats(string bucket,double seconds);
    void dump_stats(class xml &xml);

    // NOTE:
    // only virtual functions may be called by plugins!
    virtual feature_recorder *get_name(const std::string &name);
    virtual feature_recorder *get_alert_recorder();
};


#endif
