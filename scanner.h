/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef SCANNER_H
#define SCANNER_H

/*
 * Interface for individual scanners.
 */

#include <string>
#include <set>
#include <sstream>
#include <map>
#include <ostream>


#include "histogram.h"
#include "packet_info.h"
#include "sbuf.h"
#include "scanner_config.h"

/** A scanner is a function that takes a reference to scanner params and a recrusion control block */
typedef void scanner_t(const struct scanner_params &sp, const struct recursion_control_block &rcb);

/**
 * \class scanner_params
 * The scanner params class is the primary way that the bulk_extractor framework
 * communicates with the scanners.
 *
 * A pointer to the scanner_params struct is provided as the first argument to each scanner when it is called.
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
    /**
     * scanner_info is an object created by the scanner when it is initialized.
     * A pointer to the object is passed back to the scanner_set in the scanner_params.
     * Once the object is created, it is never modified.
     *
     */
    struct scanner_info {
        std::stringstream helpstream {}; // where scanner info help messages are saved.

        // default copy construction and assignment are meaningless
        // and not implemented
        scanner_info(const scanner_info &i)=delete;
        scanner_info &operator=(const scanner_info &i)=delete;
        std::string helpstr(){ return helpstream.str();}

        /* scanner flags */
        static const int SCANNER_DEFAULT_DISABLED       = 0x001; //  enabled by default
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
            if(flag & SCANNER_DEFAULT_DISABLED)  ret += "SCANNER_DEFAULT_DISABLED ";
            if(flag & SCANNER_NO_USAGE)       ret += "SCANNER_NO_USAGE ";
            if(flag & SCANNER_NO_ALL)         ret += "SCANNER_NO_ALL ";
            if(flag & SCANNER_FIND_SCANNER)   ret += "SCANNER_FIND_SCANNER ";
            if(flag & SCANNER_RECURSE)        ret += "SCANNER_RECURSE ";
            if(flag & SCANNER_RECURSE_EXPAND) ret += "SCANNER_RECURSE_EXPAND ";
            if(flag & SCANNER_WANTS_NGRAMS)   ret += "SCANNER_WANTS_NGRAMS ";
            return ret;
        }

        scanner_info(){};
        /* PASSED FROM SCANNER to API: */
        int               si_version { CURRENT_SI_VERSION };             // version number for this structure
        scanner_t         *scanner;            // the scanner
        std::string       name {};                //   scanner name
        std::string       author {};              //   who wrote me?
        std::string       description {};         //   what do I do?
        std::string       url {};                 //   where I come from
        std::string       scanner_version {};     //   version for the scanner
        std::string       pathPrefix{};           //   this scanner's path prefix for recursive scanners. e.g. "GZIP"
        uint64_t          flags {};               //   flags
        std::set<std::string> feature_names {};   //   features I need
        histogram_defs_t  histogram_defs {};      //   histogram definition info
        //void              *packet_user {};        //   data for network callback
        //be13::packet_callback_t *packet_cb {};    //   callback for processing network packets, or NULL
    };

    /* Scanners can also be asked to assist in printing. */
    enum print_mode_t {MODE_NONE=0,MODE_HEX,MODE_RAW,MODE_HTTP};
    const int CURRENT_SP_VERSION {4};

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

    // phase_t specifies when the scanner is being called.
    // the scans are implemented in the scanner set
    enum phase_t {
        PHASE_NONE     ,
        PHASE_STARTUP  ,            // called in main thread when scanner loads; called on EVERY scanner (called for help)
        PHASE_INIT     ,            // called in main thread for every ENABLED scanner after all scanners loaded
        PHASE_THREAD_BEFORE_SCAN4,  // called in worker thread for every ENABLED scanner before first scan
        PHASE_SCAN     ,            // called in worker thread for every ENABLED scanner to scan an sbuf
        PHASE_SHUTDOWN             // called in main thread for every ENABLED scanner when scanner is shutdown
    };

    /*
     * CONSTRUCTORS
     *
     * Scanner_params are made whenever a scanner needs to be called. These constructors cover all of those cases.
     */
    scanner_params(const scanner_params &)=delete;
    scanner_params &operator=(const scanner_params &)=delete;


    /* A scanner params with all of the instance variables, typically for scanning  */
    scanner_params(class scanner_set *owner_,
                   const scanner_config &config_,phase_t phase_, const sbuf_t &sbuf_,
                   class feature_recorder_set &fs_, PrintOptions print_options_):
        owner(owner_),config(config_),phase(phase_),sbuf(sbuf_),fs(fs_),print_options(print_options_){ }

    /* A scanner params with no print options */
    scanner_params(class scanner_set *owner_,
                   const scanner_config &config_,phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_):
        owner(owner_),config(config_),phase(phase_),sbuf(sbuf_),fs(fs_){ }

    /* A scanner params with no print options but an xmlstream */
    scanner_params(class scanner_set *owner_,
                   const scanner_config &config_,phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_,
                   std::stringstream *xmladd):
        owner(owner_),config(config_),phase(phase_),sbuf(sbuf_),fs(fs_),sxml(xmladd){ }

    /** Construct a scanner_params for recursion from an existing sp and a new sbuf.
     * Defaults to phase1.
     * This is the complicated one!
     */
    scanner_params(class scanner_set *owner_,const scanner_params &sp_existing, const sbuf_t &sbuf_new):
        owner(owner_),
        config(sp_existing.config),
        phase(sp_existing.phase),
        sbuf(sbuf_new),
        fs(sp_existing.fs),
        print_options(sp_existing.print_options),
        depth(sp_existing.depth+1){ };

    class scanner_set *owner;
    const class scanner_config  &config; // allows scanners to review the configuration for this scanner set
    const phase_t               phase; /*  0=startup, 1=normal, 2=shutdown (changed to phase_t in v1.3) */
    void  (*register_info)(class scanner_set *ss,
                           const scanner_params::scanner_info *info); // callback to register info
    const sbuf_t                &sbuf; /*  what to scan / only valid in SCAN_PHASE */
    class feature_recorder_set  &fs; /* where to put the results / only valid in SCAN_PHASE */
    PrintOptions                print_options {}; /* how to print. Default is that there are now options*/
    const uint32_t              depth {0};        /*  how far down are we? / only valid in SCAN_PHASE */
    std::stringstream           *sxml{};    /* on scanning and shutdown: CDATA added to XML stream if provided (advanced feature) */
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


#endif
