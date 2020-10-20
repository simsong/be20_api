/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef SCANNER_SET_H
#define SCANNER_SET_H

/* This file must be processed after config.h is read.
 * Don't include this file directly; include bulk_extractor_i.h.
 */

/**
 * \file
 * bulk_extractor scanner architecture.
 *
 * The scanner_set class implements loadable scanners from files and
 * keeps track of which are enabled and which are not.
 *
 * Sequence of operations:
 * 1. scanner_config is loaded with any name=value configurations.
 * 2. scanner_set() is created with the config. The scanner_set:
 *     - Loads any scanners from specified directories.
       - Processes all enable/disable commands to determine which scanners are enabled and disabled.
 * 3. Scanners are queried to determine which feature files they write to, and which histograms they created.
 * 4. Data is processed.
 * 5. Scanners are shutdown.
 * 6. Histograms are written out.
 *
 * Scanners are called with two parameters:
 * A reference to a scanner_params (SP) object.
 * A reference to a recursion_control_block (RCB) object.
 *
 * On startup, each scanner is called with a special SP and RCB.
 * The scanners respond by setting fields in the SP and returning.
 *
 * When executing, once again each scanner is called with the SP and RCB.
 * This is the only file that needs to be included for a scanner.
 *
 * \li \c phase_startup - scanners are loaded and register the names of the feature files they want.
 * \li \c phase_scan - each scanner is called to analyze 1 or more sbufs.
 * \li \c phase_shutdown - scanners are given a chance to shutdown
 *
 * The scanner_set references the feature_recorder_set, which is a set of feature_recorder objects.
 *
 * The scanner_set controls running of the scanners. It can run in a single-threaded mode, having a single
 * sbuf processed recursively within a single thread.
 * TODO: or it can be called with a threadpool.
 */


/**
 * scanner_info gets filled in by the scanner to tell the caller about the scanner.
 *
 */
struct scanner_info {
    std::stringstream helpstream; // where scanner info help messages are saved.

    // default copy construction and assignment are meaningless
    // and not implemented
    scanner_info(const scanner_info &i)=delete;
    scanner_info &operator=(const scanner_info &i)=delete;
    std::string helpstr(){ return helpstream.str();}

    /* scanner flags */
    static const int SCANNER_DISABLED       = 0x001; //  enabled by default
    static const int SCANNER_NO_USAGE       = 0x002; //  do not show scanner in usage
    static const int SCANNER_NO_ALL         = 0x004; //  do not enable with -eall
    static const int SCANNER_FIND_SCANNER   = 0x008; //  this scanner uses the find_list
    static const int SCANNER_RECURSE        = 0x010; //  this scanner will recurse
    static const int SCANNER_RECURSE_EXPAND = 0x020; //  recurses AND result is >= original size
    static const int SCANNER_WANTS_NGRAMS   = 0x040; //  Scanner gets buffers that are constant n-grams
    static const int SCANNER_FAST_FIND      = 0x080; //  This scanner is a very fast FIND scanner
    static const int SCANNER_DEPTH_0        = 0x100; //  scanner only runs at depth 0 by default
    static const int CURRENT_SI_VERSION     = 4;

    static const std::string flag_to_string(const int flag){
        std::string ret {};
        if(flag==0)                       ret += "NONE ";
        if(flag & SCANNER_DISABLED)       ret += "SCANNER_DISABLED ";
        if(flag & SCANNER_NO_USAGE)       ret += "SCANNER_NO_USAGE ";
        if(flag & SCANNER_NO_ALL)         ret += "SCANNER_NO_ALL ";
        if(flag & SCANNER_FIND_SCANNER)   ret += "SCANNER_FIND_SCANNER ";
        if(flag & SCANNER_RECURSE)        ret += "SCANNER_RECURSE ";
        if(flag & SCANNER_RECURSE_EXPAND) ret += "SCANNER_RECURSE_EXPAND ";
        if(flag & SCANNER_WANTS_NGRAMS)   ret += "SCANNER_WANTS_NGRAMS ";
        return ret;
    }

    // never change the order or delete old fields, or else you will
    // break backwards compatability
    scanner_info(){};
    /* PASSED FROM SCANNER to API: */
    int               si_version { CURRENT_SI_VERSION};             // version number for this structure
    std::string       name {};                //  (output) scanner name
    std::string       author {};              //  (output) who wrote me?
    std::string       description {};         //  (output) what do I do?
    std::string       url {};                 //  (output) where I come from
    std::string       scanner_version {};     //  (output) version for the scanner
    uint64_t          flags {};               //  (output) flags
    std::set<std::string> feature_names {};   //  (output) features I need
    histogram_defs_t  histogram_defs {};      //  (output) histogram definition info
    void              *packet_user {};        //  (output) data for network callback
    packet_callback_t *packet_cb {};          //  (output) callback for processing network packets, or NULL

    /* PASSED FROM API TO SCANNER; access with functions below */
    /* TODO: Why is this not a &? */
    const scanner_config config;          //  (intput to scanner) config

    virtual ~scanner_info(){};
};

/**
 * \class scanner_params
 * The scanner params class is the primary way that the bulk_extractor framework
 * communicates with the scanners. The scanner_params object is provided as the argument to each scanner.
 * @param phase_        - the PHASE
 * @param sbuf          - (only valid in the scanning phase) the buffer to be scanned
 * @param fs            - where the features should be saved.
 *                        This is an element of the scanner_set class.
 *                        We could pass the entier scanner_set, but there is no reason to do so.
 * @param config        - The current configuration. Typically only accessed as initialization
 *
 * The scanner_params struct is a way for sending parameters to the scanner
 * regarding the particular sbuf to be scanned.
 */
struct scanner_params {
    enum print_mode_t {MODE_NONE=0,MODE_HEX,MODE_RAW,MODE_HTTP};
    const int CURRENT_SP_VERSION {3};

    typedef std::map<std::string,std::string> PrintOptions;
    static print_mode_t getPrintMode(const PrintOptions &po){
        PrintOptions::const_iterator p = po.find("print_mode_t");
        if(p != po.end()){
            if(p->second=="MODE_NONE") return MODE_NONE;
            if(p->second=="MODE_HEX") return MODE_HEX;
            if(p->second=="MODE_RAW") return MODE_RAW;
            if(p->second=="MODE_HTTP") return MODE_HTTP;
        }
        return MODE_NONE;
    }
    static void setPrintMode(PrintOptions &po,int mode){
        switch(mode){
        default:
        case MODE_NONE:po["print_mode_t"]="MODE_NONE";return;
        case MODE_HEX:po["print_mode_t"]="MODE_HEX";return;
        case MODE_RAW:po["print_mode_t"]="MODE_RAW";return;
        case MODE_HTTP:po["print_mode_t"]="MODE_HTTP";return;
        }
    }

    // phase_t specifies when the scanner is being called
    typedef enum {
        PHASE_NONE     = -1,
        PHASE_STARTUP  = 0,            // called in main thread when scanner loads; called on EVERY scanner (called for help)
        PHASE_INIT     = 3,            // called in main thread for every ENABLED scanner after all scanners loaded
        PHASE_THREAD_BEFORE_SCAN = 4,  // called in worker thread for every ENABLED scanner before first scan
        PHASE_SCAN     = 1,            // called in worker thread for every ENABLED scanner to scan an sbuf
        PHASE_SHUTDOWN = 2,            // called in main thread for every ENABLED scanner when scanner is shutdown
    } phase_t ;

    /*
     * CONSTRUCTORS
     *
     * Scanner_params are made whenever a scanner needs to be called. These constructors cover all of those cases.
     */

    /* A scanner params with all of the instance variables, typically for scanning  */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_, PrintOptions print_options_):
        phase(phase_),sbuf(sbuf_),fs(fs_),print_options(print_options_){ }

    /* A scanner params with no print options */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_):
        phase(phase_),sbuf(sbuf_),fs(fs_){ }

    /* A scanner params with no print options but an xmlstream */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_, std::stringstream *xmladd):
        phase(phase_),sbuf(sbuf_),fs(fs_),sxml(xmladd){ }

    /** Construct a scanner_params for recursion from an existing sp and a new sbuf.
     * Defaults to phase1.
     * This is the complicated one!
     */
    scanner_params(const scanner_params &sp_existing, const sbuf_t &sbuf_new):
        depth(sp_existing.depth+1),
        phase(sp_existing.phase),sbuf(sbuf_new),
        fs(sp_existing.fs),
        print_options(sp_existing.print_options),
        info(sp_existing.info){
    };

    /**
     * A scanner params with an empty info
     */

    const uint32_t              depth{};         /*  how far down are we? / only valid in SCAN_PHASE */
    const int                   sp_version{CURRENT_SP_VERSION};     /* version number of this structure */
    void check_version(void) const { /* Meant to be called by plugin passed a &sp */
        if (sp_version != CURRENT_SP_VERSION){
            throw std::runtime_error("passed sp_version != CURRENT_SP_VERSION");
        }
        if (phase==scanner_params::PHASE_STARTUP && info!=nullptr) {
            if (info->si_version != scanner_info::CURRENT_SI_VERSION ){
                throw std::runtime_error("passed si_version != CURRENT_SI_VERSION");
            }
        }
    }

    /**
     * Constant instance variables that must always be provided. These cannot default.
     */

    class scanner_config        *config{}; // scanner config for scanners in this scanner_set.
    const phase_t               phase; /*  0=startup, 1=normal, 2=shutdown (changed to phase_t in v1.3) */
    const sbuf_t                &sbuf; /*  what to scan / only valid in SCAN_PHASE */
    class feature_recorder_set  &fs; /* where to put the results / only valid in SCAN_PHASE */

    /*
     * These are instance variables that can be changed.
     */
    PrintOptions                 print_options {}; /* how to print. Default is that there are now options*/
    scanner_info                *info{};           /* set/get parameters on startup, hasher */
    std::stringstream           *sxml{};           /* on scanning and shutdown: CDATA added to XML stream if provided (advanced feature) */
};

inline std::ostream & operator <<(std::ostream &os,const scanner_params &sp){
    os << "scanner_params(" << sp.sbuf << ")";
    return os;
};

/**
 * the recursion_control_block keeps track of what gets added to
 * the path when there is recursive re-analysis. It's now a structure within the scanner_set
 * because it's controlled by the scanning process.
 */

struct recursion_control_block {
    /* process_t is a function that processes a scanner_params block.
     */
    typedef void process_t(const scanner_params &sp);

    /**
     * @param callback_ - the function to call back
     * @param partName_ - the part of the forensic path processed by this scanner.
     */
    recursion_control_block(process_t *callback_,std::string partName_):
        callback(callback_),partName(partName_){}
    process_t *callback{};
    std::string partName{};            /* eg "ZIP", "GZIP" */
};


/** A scanner is a function that takes a reference to scanner params and a recrusion control block */
typedef void scanner_t(const scanner_params &sp, const recursion_control_block &rcb);



/**
 *  \class scanner_set
 *
 * scanner_set is a set of scanners that are loaded into memory. It consists of:
 *  - a set of commands for the scanners (we have the commands before the scanners are loaded)
 *  - a vector of the scanners
 *    - methods for adding scanners to the vector
 *  - the feature recorder set used by the scanners
 */

class scanner_set {
    // A boring class: can't copy or assign it.
    scanner_set(const scanner_set &s)=delete;
    scanner_set &operator=(const scanner_set &s)=delete;


    /* Scanner definitions and the variables in which they are kept
     *
     * The scanner_def class holds configuration information for each scanner.
     * loaded into this scanner_set.
     */
    struct scanner_def {
        uint32_t max_depth {7};       // maximum depth to scan for the scanners
        uint32_t max_ngram {10};      // maximum ngram size to change

        scanner_def(){};
        scanner_t   *scanner{};        // the scanner function.
        std::string  pathPrefix{};     // this scanner's path prefix for recursive scanners. e.g. "GZIP"
    };


    /* These vectors should be turned into sets.... */
    typedef std::vector<scanner_def* > scanner_vector;
    scanner_vector all_scanners;        // all of the scanners
    scanner_vector enabled_scanners;    // the scanners that are enabled

    // The commands for those scanners (enable, disable, options, etc.
    std::vector<struct scanner_command> scanner_commands {};

    // The scanner_set's configuration for all the scanners that are loaded.
    const scanner_config sc;

    // And the feature recorder set.
    // Each feature recorder is is created by a scanner.
    class feature_recorder_set fs;

    bool     dup_data_alerts       {false};  // notify when duplicate data is not processed
    uint64_t dup_data_encountered  {0}; // amount of dup data encountered

public:;
    // Create a scanner set with these builtin_scanners.
    scanner_set(const scanner_config &);

    /**
     *  Commands whether to enable or disable a scanner. Typically created from parsing command-line arguments
     */
    struct scanner_command {
        enum command_t {DISABLE_ALL=0,ENABLE_ALL,DISABLE,ENABLE};
        scanner_command(const scanner_command &sc):command(sc.command),name(sc.name){};
        scanner_command(scanner_command::command_t c,const std::string &n):command(c),name(n){};
        command_t command {};
        std::string name  {};
    };

    void set_debug(int debug);

    /* Scanners can be compiled in (which are passed to the constructor), loaded one-by-one from meory,
     * or loaded from a file, a directory, or a set of directories.
     * Loaded scanners are added to the 'scanners' vector.
     *
     * After the scanners are loaded, the scan starts.
     * Each scanner is called with a scanner control block
     */
    void add_scanner(scanner_t scanner);      // load a specific scanner in memory
    void add_scanner_file(std::string fn);    // load a scanner from a file
    void add_scanners(std::vector<scanner_t> &builtin_scanners);
    void add_scanners(scanner_t * const *scanners_builtin); // load the scan_ plugins
    void add_scanner_directory(const std::string &dirname); // load scanners in the directory
    void add_scanner_directories(const std::vector<std::string> &dirnames);

    void load_scanner_packet_handlers(); // after all scanners are loaded, this sets up the packet handlers.

    /* Managing scanners */
    scanner_t *find_scanner(const std::string &name);
    void get_scanner_feature_file_names(feature_file_names_t &feature_file_names);

    // enabling and disabling of scanners
    void get_enabled_scanners(std::vector<std::string> &svector); // put the enabled scanners into the vector
    void scanners_disable_all();                    // saves a command to disable all
    void scanners_enable_all();                    // enable all of them
    void set_scanner_enabled(const std::string &name,bool enable);
    void set_scanner_enabled_all(bool enable);
    void scanners_enable(const std::string &name); // saves a command to enable this scanner
    void scanners_disable(const std::string &name); // saves a command to disable this scanner
    void process_scanner_commands(const std::vector<scanner_command> &scanner_commands); // process the commands

    // returns the named scanner, or 0 if no scanner of that name
    void add_enabled_scanner_histograms_to_feature_recorder_set(feature_recorder_set &fs);
    bool find_scanner_enabled(); // return true if a find scanner is enabled

    void     scanners_init(feature_recorder_set &fs); // init the scanners
    void     info_scanners(bool detailed_info,
                           bool detailed_settings,
                           scanner_t * const *scanners_builtin,const char enable_opt,const char disable_opt);

    /* Run the phases on the scanners */
    void     message_enabled_scanners(scanner_params::phase_t phase);
    void     process_sbuf(const scanner_params &sp);                              /* process for feature extraction */
    void     process_packet(const be13::packet_info &pi);
    void     phase_shutdown(feature_recorder_set &fs,std::stringstream *sxml=0); // sxml is where to put XML from scanners that shutdown
    uint32_t get_max_depth_seen();
};

#endif
