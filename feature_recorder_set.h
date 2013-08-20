/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef FEATURE_RECORDER_SET_H
#define FEATURE_RECORDER_SET_H
#include "feature_recorder.h"
#include "cppmutex.h"

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
    // neither copying nor assignment is implemented 
    feature_recorder_set(const feature_recorder_set &fs);
    feature_recorder_set &operator=(const feature_recorder_set &fs);
    uint32_t flags;
public:
    // instance data //
    std::string input_fname;            // input file
    std::string outdir;                 // where output goes
    feature_recorder_map frm;          // map of feature recorders, by name
    cppmutex Mlock;                    // locks frm and scanner_stats_map
private:
    std::set<std::string>seen_set;
    cppmutex  seen_set_lock;
public:
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

    virtual ~feature_recorder_set() {
        for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
            delete i->second;
        }
    }

    /** create a dummy feature_recorder_set with only an alert or disabled recorder and no output directory */
    feature_recorder_set(uint32_t flags_);

    /** Create a properly functioning feature recorder set. */
    feature_recorder_set(const feature_file_names_t &feature_files,
                         const std::string &input_fname,
                         const std::string &outdir,
                         bool create_stop_files);

    void flush_all();
    void close_all();
    bool has_name(string name) const;   /* does the named feature exist? */
    void set_flag(uint32_t f){flags|=f;}
    void create_name(string name,bool create_stop_also);
    void clear_flag(uint32_t f){flags|=f;}
    void add_stats(string bucket,double seconds);
    typedef void (*stat_callback_t)(void *user,const std::string &name,uint64_t calls,double seconds);
    void get_stats(void *user,stat_callback_t stat_callback);

    // Management of previously seen data
public:
    virtual bool check_previously_processed(const uint8_t *buf,size_t bufsize);

    // NOTE:
    // only virtual functions may be called by plugins!
    virtual feature_recorder *get_name(const std::string &name);
    virtual feature_recorder *get_alert_recorder();
};


#endif
