/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef FEATURE_RECORDER_SET_H
#define FEATURE_RECORDER_SET_H

#include <map>
#include <set>
#include <mutex>

#include "feature_recorder.h"
#include "dfxml/src/dfxml_writer.h"
#include "dfxml/src/hash_t.h"
#include "word_and_context_list.h"

/** \addtogroup internal_interfaces
 * @{
 */
/** \file */

/**
 * \class feature_recorder_set
 * The feature_recorder_set is an object that controls output. It knows where the output goes (outdir),
 * the various feature recorders that write to that output, and provides for synchronization. 
 * It also has the factory method for new feature_recorders. Therefore if you want a different feature_recorder,
 * this set should be subclassed as well.
 *
 * NOTE: plugins can only call virtual functions!
 *
 */

typedef std::map<std::string,class feature_recorder *> feature_recorder_map;
typedef std::set<std::string>feature_file_names_t;
class feature_recorder_set {
    friend class feature_recorder;

    // neither copying nor assignment is implemented 
    feature_recorder_set(const feature_recorder_set &fs)=delete;
    feature_recorder_set &operator=(const feature_recorder_set &fs)=delete;
    uint32_t flags;
    atomic_set<std::string> seen_set;       // hex hash values of pages that have been seen
    const std::string     input_fname;      // input file
    const std::string     outdir;           // where output goes
    // TK-replace with an atomic_set:
    feature_recorder_map  frm;              // map of feature recorders, name->feature recorder
    mutable std::mutex    Mscanner_stats;         // locks frm and scanner_stats_map
    histogram_defs_t      histogram_defs;   // histograms that are to be created.
    mutable std::mutex    Min_transaction;
    bool                  in_transaction;
#ifdef BEAPI_SQLITE3
    BEAPI_SQLITE3         *db3;             // opened in SQLITE_OPEN_FULLMUTEX mode
#endif
    bool                  init_called;

public:
    /** create an emptry feature recorder set. If disabled, create a disabled recorder. */
    feature_recorder_set( uint32_t flags_, const std::string hash_algorithm, 
                          const std::string &input_fname_, const std::string &outdir_);
    virtual ~feature_recorder_set();

    /* the feature recorder set automatically hashes all of the sbuf's that it processes. */
    typedef std::string (*hash_func_t)(const uint8_t *buf,const size_t bufsize);
    class hash_def {
    public:;
        hash_def(std::string name_,hash_func_t func_):name(name_),func(func_){};
        std::string name;                                             // name of hash
        hash_func_t func; // hash function
        static std::string md5_hasher(const uint8_t *buf,size_t bufsize);
        static std::string sha1_hasher(const uint8_t *buf,size_t bufsize);
        static std::string sha256_hasher(const uint8_t *buf,size_t bufsize);
        static hash_func_t hash_func_for_name(const std::string &name);
    };

    // performance Statistics for each scanner*/
    struct pstats {
        double seconds;
        uint64_t calls;
    };
    typedef std::map<std::string,struct pstats> scanner_stats_map; // maps scanner name to performance stats

    const word_and_context_list *alert_list;		/* shold be flagged */
    const word_and_context_list *stop_list;		/* should be ignored */
    scanner_stats_map      scanner_stats;

    /** hashing system */
    const  hash_def       hasher;        // name and function that perform hashing.

    static const std::string   ALERT_RECORDER_NAME;  // the name of the alert recorder
    static const std::string   DISABLED_RECORDER_NAME; // the fake disabled feature recorder
    static const std::string   NO_INPUT;  // 'filename' indicator that the FRS has no input file
    static const std::string   NO_OUTDIR; // 'dirname' indicator that the FRS produces no file output

    std::string   get_input_fname()           const { return input_fname;}
    virtual const std::string &get_outdir() const { return outdir;}
    void          set_stop_list(const word_and_context_list *alist){stop_list=alist;}
    void          set_alert_list(const word_and_context_list *alist){alert_list=alist;}

    /** Initialize a feature_recorder_set. Previously this was a constructor, but it turns out that
     * virtual functions for the create_name_factory aren't honored in constructors.
     *
     * init() is called after all of the scanners have been loaded. It
     * tells each feature file about its histograms (among other things)
     */
    void    init(const feature_file_names_t &feature_files); 
    void    flush_all();
    void    close_all();
    bool    has_name(std::string name) const;           /* does the named feature exist? */

    /* feature_recorder_set flags */
    static const uint32_t ONLY_ALERT                = 0x01;  // always return the alert recorder
    static const uint32_t SET_DISABLED              = 0x02;  // the set is effectively disabled; for path-printer
    static const uint32_t CREATE_STOP_LIST_RECORDERS= 0x04;  //
    static const uint32_t MEM_HISTOGRAM             = 0x20;  // enable the in-memory histogram
    static const uint32_t ENABLE_SQLITE3_RECORDERS  = 0x40;  // save features to an SQLITE3 databse
    static const uint32_t DISABLE_FILE_RECORDERS    = 0x80;  // do not save features to file-based recorders
    static const uint32_t NO_ALERT                  = 0x100; // no alert recorder
    void     set_flag(uint32_t f);
    void     unset_flag(uint32_t f);
    bool     flag_set(uint32_t f)     const { return flags & f; }
    bool     flag_notset(uint32_t f)  const { return !(flags & f); }
    uint32_t get_flags()             const { return flags; }

    /* histogram support */

    typedef  void (*xml_notifier_t)(const std::string &xmlstring);
    void     add_histogram(const histogram_def &def); // adds it to a local set or to the specific feature recorder
    void     dump_histograms(void *user,feature_recorder::dump_callback_t cb, xml_notifier_t xml_error_notifier) const;

    /* support for communiucating with feature recorders */
    virtual feature_recorder *create_name_factory(const std::string &name_);
    virtual void create_name(const std::string &name,bool create_stop_also);

    void    add_stats(const std::string &bucket,double seconds);
    typedef int (*stat_callback_t)(void *user,const std::string &name,uint64_t calls,double seconds);
    void    get_stats(void *user,stat_callback_t stat_callback) const;
    void    dump_name_count_stats(dfxml_writer &writer) const;

    /****************************************************************
     *** SQLite3 interface
     ****************************************************************/

#ifdef BEAPI_SQLITE3
    virtual void db_send_sql(BEAPI_SQLITE3 *db3,const char **stmts, ...) ;
    virtual BEAPI_SQLITE3 *db_create_empty(const std::string &name) ;
    void     db_create_table(const std::string &name) ;
    void     db_create() ;
    void     db_transaction_begin() ;
    void     db_transaction_commit() ;               // commit current transaction
    void     db_close() ;             // 
#endif

    /****************************************************************
     *** External Functions
     ****************************************************************/
    
    // Management of previously seen data
    virtual bool check_previously_processed(const uint8_t *buf,size_t bufsize);

    // NOTE:
    virtual feature_recorder *get_name(const std::string &name) const;
    virtual feature_recorder *get_alert_recorder() const;
    virtual void get_feature_file_list(std::vector<std::string> &ret); // clears ret and fills with a list of feature file names
};


#endif
