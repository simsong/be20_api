/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef FEATURE_RECORDER_SET_H
#define FEATURE_RECORDER_SET_H

#if defined(HAVE_SQLITE3_H)
#include <sqlite3.h>
#endif

#include "sbuf.h"
#include "feature_recorder.h"
#include "histogram.h"
#include "dfxml/src/dfxml_writer.h"

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
 * Process to set up:
 * 1 - plugin::scanners_process_enable_disable_commands is run to determine which scanners are enabled.
 * 2 - plugin::get_scanner_feature_file_names(feature_file_names) gets a list of the current feature files.
 * 3 - feature_recorder_set is created, given:
 *       - flags
 *       - hash algorithm to use
 *       - input file
 *       - output directory
 *   It initializes the feature recorders and itself.
 */

class word_and_context_list;
class feature_recorder_set {
    //typedef std::map<std::string, class feature_recorder *> feature_recorder_map;

    // neither copying nor assignment is implemented
    feature_recorder_set(const feature_recorder_set &fs)=delete;
    feature_recorder_set &operator=(const feature_recorder_set &fs)=delete;

    friend class feature_recorder;

    //uint32_t              flags {};       // how it was configured
    const std::string     input_fname {}; // input file; copy for convenience.
    const std::string     outdir {};      // where output goes; must know.


    atomic_set<std::string> seen_set {};       // hex hash values of pages that have been seen
    size_t   context_window_default {16};           // global option


    // TK-replace with an atomic_set:
    atomic_map<std::string, class feature_recorder *> frm {};
    //feature_recorder_map  frm {};              // map of feature recorders, name->feature recorder, by name
    mutable std::mutex    Mscanner_stats {};   // locks frm and scanner_stats_map
    feature_recorder      *stop_list_recorder {nullptr}; // where stopped features get written (if there is one)
    //histogram_defs_t      histogram_defs {};   // histograms that are to be created.
    //mutable std::mutex    Min_transaction {};
    //bool                  in_transaction {};
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    sqlite3               *db3 {};              // databse handle opened in SQLITE_OPEN_FULLMUTEX mode
#endif

public:
    /* Debug for feature recorders. This used to be a flag, but Stroustrup (2013) recommends just having
     * a bunch of bools.
     */
    struct flags_t {
        bool debug_utf8 {false};        // make sure that all features written are valid utf-8
        bool no_alert {false};                              // no alert recorder
    } flags;

    /** Constructor:
     * create an emptry feature recorder set. If disabled, create a disabled recorder.
     * @param flags_ = config flags
     * @param hash_algorithm - which algorithm to use for de-duplication
     * @param input_fname_ = where input comes from
     * @param outdir_ = output directory (passed to feature recorders).
     * This clearly needs work.
     */
    feature_recorder_set( const flags_t &flags_,
                          const std::string hash_algorithm,
                          const std::string &input_fname_,
                          const std::string &outdir_);
    virtual ~feature_recorder_set();

    /* the feature recorder set automatically hashes all of the sbuf's that it processes. */
    typedef std::string (*hash_func_t)(const uint8_t *buf,const size_t bufsize);
    struct hash_def {
        hash_def(std::string name_,hash_func_t func_):
            name(name_),func(func_){
        };
        std::string name;                                           // name of hash
        hash_func_t func; // hash function
        static std::string md5_hasher(const uint8_t *buf,size_t bufsize);
        static std::string sha1_hasher(const uint8_t *buf,size_t bufsize);
        static std::string sha256_hasher(const uint8_t *buf,size_t bufsize);
        static hash_func_t hash_func_for_name(const std::string &name);
    };

    // performance Statistics for each scanner*/
    struct pstats {
        double seconds {};
        uint64_t calls {};
    };
    typedef std::map<std::string,struct pstats> scanner_stats_map; // maps scanner name to performance stats

    const word_and_context_list *alert_list {};		/* shold be flagged */
    const word_and_context_list *stop_list {};		/* should be ignored */
    scanner_stats_map      scanner_stats {};

    /** hashing system */
    const  hash_def       hasher;                    // name and function that perform hashing; set by allocator

    static const std::string   ALERT_RECORDER_NAME;  // the name of the alert recorder
    static const std::string   DISABLED_RECORDER_NAME; // the fake disabled feature recorder

    std::string   get_input_fname()           const { return input_fname;}
    virtual const std::string &get_outdir()   const { return outdir;}
    void          set_stop_list(const word_and_context_list *alist){stop_list=alist;}
    void          set_alert_list(const word_and_context_list *alist){alert_list=alist;}

    /** Initialize a feature_recorder_set. Previously this was a constructor, but it turns out that
     * virtual functions for the create_name_factory aren't honored in constructors.
     *
     * init() is called after all of the scanners have been loaded. It
     * tells each feature file about its histograms (among other things)
     */
    //typedef std::set<std::string>feature_file_names_t;
    //void    init(const feature_file_names_t &feature_files);
    void    flush_all();
    void    close_all();
    bool    has_name(std::string name) const;           /* does the named feature exist? */

    /* feature_recorder_set flags */
    /* Flags are now implemented as booleans per stroustrup 2013 */
    bool flag_only_alert {false};                                //  always return the alert recorder
    bool flag_set_disabled {false}; //              = 0x02;  // the set is effectively disabled; for path-printer
    bool flag_create_stop_list_crecorders {false}; // static const uint32_t CREATE_STOP_LIST_RECORDERS= 0x04;  //
    //static const uint32_t MEM_HISTOGRAM             = 0x20;  // enable the in-memory histogram
    //static const uint32_t ENABLE_SQLITE3_RECORDERS  = 0x40;  // save features to an SQLITE3 databse
    //static const uint32_t DISABLE_FILE_RECORDERS    = 0x80;  // do not save features to file-based recorders
    //static const uint32_t NO_ALERT                  = 0x100; // no alert recorder
    //void     set_flag(uint32_t f);
    //void     unset_flag(uint32_t f);
    //bool     flag_set(uint32_t f)     const { return flags & f; }
    //bool     flag_notset(uint32_t f)  const { return !(flags & f); }
    //uint32_t get_flags()              const { return flags; }

    /* These used to be static variables in the feature recorder class. They are more properly here */
    uint32_t    opt_max_context_size;
    uint32_t    opt_max_feature_size;
    int64_t     offset_add;          // added to every reported offset, for use with hadoop
    std::string banner_filename;         // banner for top of every file

    /* histogram support */

    typedef  void (*xml_notifier_t)(const std::string &xmlstring);
    void     add_histogram(const histogram_def &def); // adds it to a local set or to the specific feature recorder
    size_t   count_histograms() const;  // counts histograms in all feature recorders
    void     generate_histograms();     // make the histograms in the output directory (and optionally in the database)

    /* support for creating and finding feature recorders */
    /* Previously called create_name().
     * functions must be virtual so they can be called by plug-in.
     */
    virtual void create_named_feature_recorder(const std::string &name,bool create_stop_also);
    virtual feature_recorder *get_name(const std::string &name) const;
    virtual feature_recorder *get_alert_recorder() const;
    virtual void get_feature_file_list(std::vector<std::string> &ret); // clears ret and fills with a list of feature file names

    void    add_stats(const std::string &bucket,double seconds);
    typedef int (*stat_callback_t)(void *user,const std::string &name,uint64_t calls,double seconds);
    void    get_stats(void *user,stat_callback_t stat_callback) const;
    void    dump_name_count_stats(dfxml_writer &writer) const;

    /****************************************************************
     *** DB interface
     ****************************************************************/

#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    virtual  void db_send_sql(sqlite3 *db3,const char **stmts, ...) ;
    virtual  sqlite3 *db_create_empty(const std::string &name) ;
    void     db_create_table(const std::string &name) ;
    void     db_create() ;
    void     db_transaction_begin() ;
    void     db_transaction_commit() ;               // commit current transaction
    void     db_close() ;                            //
#endif

    /****************************************************************
     *** External Functions
     ****************************************************************/

    // Management of previously seen data
    virtual bool check_previously_processed(const sbuf_t &sbuf);

};


#endif
