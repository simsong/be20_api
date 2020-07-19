#ifndef PLUGIN_H
#define PLUGIN_H

/**
 * \file
 * bulk_extractor scanner plug_in architecture.
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
 */

#include "packet_info.h"

namespace be13 {

    /** scanner_info gets filled in by the scanner to tell the caller about the scanner.
     *
     */
    class scanner_info {
    private:
        static std::stringstream helpstream; // where scanner info help messages are saved.

        // default copy construction and assignment are meaningless
        // and not implemented
        scanner_info(const scanner_info &i);
        scanner_info &operator=(const scanner_info &i);
    public:
        static std::string helpstr(){return helpstream.str();}
        typedef std::map<std::string,std::string>  config_t; // configuration for scanner passed in

        /* scanner flags */
        static const int SCANNER_DISABLED       = 0x001; // v1: enabled by default 
        static const int SCANNER_NO_USAGE       = 0x002; // v1: do not show scanner in usage 
        static const int SCANNER_NO_ALL         = 0x004; // v2: do not enable with -eall 
        static const int SCANNER_FIND_SCANNER   = 0x008; // v2: this scanner uses the find_list 
        static const int SCANNER_RECURSE        = 0x010; // v3: this scanner will recurse
        static const int SCANNER_RECURSE_EXPAND = 0x020; // v3: recurses AND result is >= original size
        static const int SCANNER_WANTS_NGRAMS   = 0x040; // v3: Scanner gets buffers that are constant n-grams
        static const int SCANNER_FAST_FIND      = 0x080; // v3: This scanner is a very fast FIND scanner
        static const int SCANNER_DEPTH_0        = 0x100; // v3: scanner only runs at depth 0 by default
        static const int CURRENT_SI_VERSION     = 4;     

        static const std::string flag_to_string(const int flag){
            std::string ret;
            if(flag==0) ret += "NONE ";
            if(flag & SCANNER_DISABLED) ret += "SCANNER_DISABLED ";
            if(flag & SCANNER_NO_USAGE) ret += "SCANNER_NO_USAGE ";
            if(flag & SCANNER_NO_ALL) ret += "SCANNER_NO_ALL ";
            if(flag & SCANNER_FIND_SCANNER) ret += "SCANNER_FIND_SCANNER ";
            if(flag & SCANNER_RECURSE) ret += "SCANNER_RECURSE ";
            if(flag & SCANNER_RECURSE_EXPAND) ret += "SCANNER_RECURSE_EXPAND ";
            if(flag & SCANNER_WANTS_NGRAMS) ret += "SCANNER_WANTS_NGRAMS ";
            return ret;
        }

        /* Global config is passed to each scanner as a pointer when it is loaded.
         * Scanner histograms are added to 'histograms' by machinery.
         */
        struct scanner_config {
            scanner_config():namevals(),debug(){};
            virtual ~scanner_config(){}
            config_t  namevals;             // v3: (input) name=val map
            int       debug;                // v3: (input) current debug level
        };

        // never change the order or delete old fields, or else you will
        // break backwards compatability 
        scanner_info():si_version(CURRENT_SI_VERSION),
                       name(),author(),description(),url(),scanner_version(),flags(0),feature_names(),
                       histogram_defs(),packet_user(),packet_cb(),config(){}
        /* PASSED FROM SCANNER to API: */
        int         si_version;             // version number for this structure
        std::string      name;                   // v1: (output) scanner name
        std::string      author;                 // v1: (output) who wrote me?
        std::string      description;            // v1: (output) what do I do?
        std::string      url;                    // v1: (output) where I come from
        std::string      scanner_version;        // v1: (output) version for the scanner
        uint64_t    flags;                  // v1: (output) flags
        std::set<std::string> feature_names;          // v1: (output) features I need
        histogram_defs_t histogram_defs;        // v1: (output) histogram definition info
        void        *packet_user;           // v2: (output) data for network callback
        packet_callback_t *packet_cb;       // v2: (output) callback for processing network packets, or NULL

        /* PASSED FROM API TO SCANNER; access with functions below */
        const scanner_config *config;       // v3: (intput to scanner) config

        // These methods are implemented in the plugin system for the scanner to get config information.
        // The get_config methods should be called on the si object during PHASE_STARTUP
        virtual void get_config(const scanner_info::config_t &c,
                                const std::string &name,std::string *val,const std::string &help);
        virtual void get_config(const std::string &name,std::string *val,const std::string &help);
        virtual void get_config(const std::string &name,uint64_t *val,const std::string &help);
        virtual void get_config(const std::string &name,int32_t *val,const std::string &help);
        virtual void get_config(const std::string &name,uint32_t *val,const std::string &help);
        virtual void get_config(const std::string &name,uint16_t *val,const std::string &help);
        virtual void get_config(const std::string &name,uint8_t *val,const std::string &help);
#ifdef __APPLE__
        virtual void get_config(const std::string &name,size_t *val,const std::string &help);
#define HAVE_GET_CONFIG_SIZE_T
#endif
        virtual void get_config(const std::string &name,bool *val,const std::string &help);
        virtual ~scanner_info(){};
    };

    /**
     * The scanner_params class is a way for sending the scanner parameters
     * for a particular sbuf to be scanned.
     */
    class scanner_params {
    public:
        enum print_mode_t {MODE_NONE=0,MODE_HEX,MODE_RAW,MODE_HTTP};
        static const int CURRENT_SP_VERSION=3;

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
        static PrintOptions no_options;    // in common.cpp

        /********************
         *** CONSTRUCTORS ***
         ********************/

        /* A scanner params with all of the instance variables, typically for scanning  */
        scanner_params(phase_t phase_,const sbuf_t &sbuf_,class feature_recorder_set &fs_,
                       PrintOptions &print_options_):
            sp_version(CURRENT_SP_VERSION),
            phase(phase_),sbuf(sbuf_),fs(fs_),depth(0),print_options(print_options_),info(0),sxml(0){
        }

        /* A scanner params with no print options */
        scanner_params(phase_t phase_,const sbuf_t &sbuf_, class feature_recorder_set &fs_):
            sp_version(CURRENT_SP_VERSION),
            phase(phase_),sbuf(sbuf_),fs(fs_),depth(0),print_options(no_options),info(0),sxml(0){
        }

        /* A scanner params with no print options but an xmlstream */
        scanner_params(phase_t phase_,const sbuf_t &sbuf_,class feature_recorder_set &fs_,std::stringstream *xmladd):
            sp_version(CURRENT_SP_VERSION),
            phase(phase_),sbuf(sbuf_),fs(fs_),depth(0),print_options(no_options),info(0),sxml(xmladd){
        }

        /** Construct a scanner_params for recursion from an existing sp and a new sbuf.
         * Defaults to phase1
         */
        scanner_params(const scanner_params &sp_existing,const sbuf_t &sbuf_new):
            sp_version(CURRENT_SP_VERSION),phase(sp_existing.phase),
            sbuf(sbuf_new),fs(sp_existing.fs),depth(sp_existing.depth+1),
            print_options(sp_existing.print_options),info(sp_existing.info),sxml(0){
            assert(sp_existing.sp_version==CURRENT_SP_VERSION);
        };

        /**
         * A scanner params with an empty info
         */

        /**************************
         *** INSTANCE VARIABLES ***
         **************************/

        const int                   sp_version;                /* version number of this structure */
        const phase_t               phase;                 /* v1: 0=startup, 1=normal, 2=shutdown (changed to phase_t in v1.3) */
        const sbuf_t                &sbuf;                 /* v1: what to scan / only valid in SCAN_PHASE */
        class feature_recorder_set  &fs;     /* v1: where to put the results / only valid in SCAN_PHASE */
        const uint32_t              depth;            /* v1: how far down are we? / only valid in SCAN_PHASE */

        PrintOptions                &print_options;    /* v1: how to print / NOT USED IN SCANNERS */
        scanner_info                *info;             /* v2: set/get parameters on startup, hasher */
        std::stringstream           *sxml;         /* v3: on scanning and shutdown: CDATA added to XML stream (advanced feature) */
    };
    inline std::ostream & operator <<(std::ostream &os,const class scanner_params &sp){
        os << "scanner_params(" << sp.sbuf << ")";
        return os;
    };

    /* process_t is a function that processes a scanner_params block.
     */
    typedef void process_t(const class scanner_params &sp); 

    /**
     * the recursion_control_block keeps track of what gets added to
     * the path when there is recursive re-analysis.
     */

    class recursion_control_block {
    public:
        /**
         * @param callback_ - the function to call back
         * @param partName_ - the part of the forensic path processed by this scanner.
         */
        recursion_control_block(process_t *callback_,std::string partName_):
            callback(callback_),partName(partName_){}
        process_t *callback;
        std::string partName;            /* eg "ZIP", "GZIP" */
    };
    


    typedef void scanner_t(const class scanner_params &sp,const class recursion_control_block &rcb);
    
    /** 
     * the scanner_def class holds configuraiton information for each scanner that is loaded into memory.
     * It should be renamed scanner_config.
     */
    class scanner_def {
    public:;
        static uint32_t max_depth;          // maximum depth to scan for the scanners
        static uint32_t max_ngram;          // maximum ngram size to change
        scanner_def():scanner(0),enabled(false),info(),pathPrefix(){};
        scanner_t  *scanner;                // pointer to the primary entry point
        bool        enabled;                // is enabled?
        scanner_info info;                  // info block sent to and returned by scanner
        std::string      pathPrefix;             /* path prefix for recursive scanners */
    };

    /** The plugin class implements loadable scanners from files and
     * keeps track of which are enabled and which are not.
     *
     */
    class plugin {
    public:;
        typedef std::vector<class scanner_def *> scanner_vector;
        static scanner_vector current_scanners;                         // current scanners
        static bool dup_data_alerts;  // notify when duplicate data is not processed
        static uint64_t dup_data_encountered; // amount of dup data encountered

        static void set_scanner_debug(int debug);

        static void load_scanner(scanner_t scanner,const scanner_info::scanner_config &sc); // load a specific scanner
        static void load_scanner_file(std::string fn,const scanner_info::scanner_config &sc);    // load a scanner from a file
        static void load_scanners(scanner_t * const *scanners_builtin,const scanner_info::scanner_config &sc); // load the scan_ plugins
        static void load_scanner_directory(const std::string &dirname,const scanner_info::scanner_config &sc); // load scanners in the directory
        static void load_scanner_directories(const std::vector<std::string> &dirnames,const scanner_info::scanner_config &sc);
        static void load_scanner_packet_handlers();
        
        // send every enabled scanner the phase message
        static void message_enabled_scanners(scanner_params::phase_t phase,feature_recorder_set &fs);

        // returns the named scanner, or 0 if no scanner of that name
        static scanner_t *find_scanner(const std::string &name); 
        static void get_enabled_scanners(std::vector<std::string> &svector); // put the enabled scanners into the vector
        static void add_enabled_scanner_histograms_to_feature_recorder_set(feature_recorder_set &fs); 
        static bool find_scanner_enabled(); // return true if a find scanner is enabled
        
        // print info about the scanners:
        static void scanners_disable_all();                    // saves a command to disable all
        static void scanners_enable_all();                    // enable all of them
        static void set_scanner_enabled(const std::string &name,bool enable);
        static void set_scanner_enabled_all(bool enable);
        static void scanners_enable(const std::string &name); // saves a command to enable this scanner
        static void scanners_disable(const std::string &name); // saves a command to disable this scanner
        static void scanners_process_enable_disable_commands();               // process the enable/disable and config commands
        static void scanners_init(feature_recorder_set &fs); // init the scanners

        static void info_scanners(bool detailed_info,
                                  bool detailed_settings,
                                  scanner_t * const *scanners_builtin,const char enable_opt,const char disable_opt);
        

        /* Run the phases on the scanners */
        static void phase_shutdown(feature_recorder_set &fs,std::stringstream *sxml=0); // sxml is where to put XML from scanners that shutdown
        static uint32_t get_max_depth_seen();
        static void process_sbuf(const class scanner_params &sp);                              /* process for feature extraction */
        static void process_packet(const be13::packet_info &pi);

        /* recorders */
        static void get_scanner_feature_file_names(feature_file_names_t &feature_file_names);

    };
};


#endif
