/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef SCANNER_PARAMS_H
#define SCANNER_PARAMS_H

/*
 * Interface for individual scanners.
 */

#include <string>
#include <set>
#include <sstream>
#include <map>
#include <ostream>


#include "histogram_def.h"
#include "packet_info.h"
#include "sbuf.h"
#include "scanner_config.h"
#include "feature_recorder.h"

/** A scanner is a function that takes a reference to scanner params and a recrusion control block */
typedef void scanner_t(struct scanner_params &sp);

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
        //std::stringstream helpstream {}; // where scanner info help messages are saved.

        // default copy construction and assignment are meaningless
        // and not implemented
        scanner_info(const scanner_info &i)=delete;
        scanner_info &operator=(const scanner_info &i)=delete;

        struct scanner_flags_t {
            bool  default_enabled {true}; //  enabled by default
            bool  no_usage {false}; //  do not show scanner in usage
            bool  no_all {false}; //  do not enable with -eall
            bool  find_scanner {false}; //  this scanner uses the find_list
            bool  recurse {false}; //  this scanner will recurse
            bool  recurse_expand {false}; //  recurses AND result is >= original size
            bool  scan_ngram_buffer {false}; //  Scanner can run even if the entire gets buffer is filled with constant n-grams
            bool  scan_seen_before {false}; //  Scanner can run even if buffer has seen before
            bool  fast_find {false}; //  This scanner is a very fast FIND scanner
            bool  depth_0 {false}; //  scanner only runs at depth 0 by default

            const std::string asString() const {
                std::string ret;
                ret += default_enabled ? "ENABLED" : "DISABLED";
                if ( no_usage )       ret += " NO_USAGE";
                if ( no_all )         ret += " NO_ALL";
                if ( find_scanner )   ret += " FIND_SCANNER";
                if ( recurse )        ret += " RECURSE";
                if ( recurse_expand ) ret += " RECURSE_EXPAND";
                if ( scan_ngram_buffer )   ret += " SCAN_NGRAM_BUFFER";
                if ( scan_seen_before )   ret += " SCAN_SEEN_BEFORE";
                if ( fast_find )      ret += " FAST_FIND";
                if ( depth_0 )        ret += " DEPTH_0";
                return ret;
            }
        } scanner_flags {};

        scanner_info(){};
        /* PASSED FROM SCANNER to API: */
        //int               si_version { CURRENT_SI_VERSION };             // version number for this structure
        scanner_t         *scanner {};            // the scanner
        std::string       helpstr {};             // the help string
        std::string       name {};                //   scanner name
        std::string       author {};              //   who wrote me?
        std::string       description {};         //   what do I do?
        std::string       url {};                 //   where I come from
        std::string       scanner_version {};     //   version for the scanner
        std::string       pathPrefix{};           //   this scanner's path prefix for recursive scanners. e.g. "GZIP"
        uint64_t          flags {};               //   flags
        std::set<std::string>    feature_names {};   //   feature files that this scanner needs.
        std::set<histogram_def>  histogram_defs {};      //   histogram definition info
        //void              *packet_user {};        //   data for network callback
        //be13::packet_callback_t *packet_cb {};    //   callback for processing network packets, or NULL

        // Move constructor
        scanner_info(scanner_info &&source ) :
            scanner(source.scanner),
            helpstr(source.helpstr),
            name(source.name),
            description(source.description),
            url(source.url),
            scanner_version(source.scanner_version),
            pathPrefix(source.pathPrefix),
            flags(source.flags),
            feature_names(source.feature_names),
            histogram_defs(source.histogram_defs) {
        }
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
        PHASE_INIT,    // called in main thread when scanner loads
        PHASE_SCAN,    // called in worker thread for every ENABLED scanner to scan an sbuf
        PHASE_SHUTDOWN // called in main thread for every ENABLED scanner when scanner is shutting down. Allows XML closing.
    };

    /*
     * CONSTRUCTORS
     *
     * Scanner_params are made whenever a scanner needs to be called. These constructors cover all of those cases.
     */
    scanner_params(const scanner_params &)=delete;
    scanner_params &operator=(const scanner_params &)=delete;


    typedef void (*register_info_t)(class scanner_set *ss,
                                    const scanner_params::scanner_info *info);


    /* A scanner params with all of the instance variables, typically for scanning  */
    scanner_params(scanner_set &ss_, phase_t phase_, const sbuf_t &sbuf_,
                   PrintOptions print_options_, std::stringstream *xmladd=nullptr ):
        ss(ss_),phase(phase_),sbuf(sbuf_),print_options(print_options_),sxml(xmladd){ }

    /* User-servicable parts; these are here for scanners */
    //register_info should use unique_ptr(), but I couldn't get it to work.
    virtual void register_info(const scanner_info *); // called by a scanner to register its info
    virtual ~scanner_params(){};
    virtual feature_recorder &named_feature_recorder(const std::string &feature_recorder_name);


#if 0
    /* A scanner params with no print options */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_):
        phase(phase_),sbuf(sbuf_),fs(fs_){ }

    /* A scanner params with no print options but an xmlstream */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_,
                   std::stringstream *xmladd):
        phase(phase_),sbuf(sbuf_),fs(fs_),sxml(xmladd){ }


    /** Construct a scanner_params for recursion from an existing sp and a new sbuf.
     * Defaults to phase1.
     * This is the complicated one!
     */
    scanner_params(const scanner_params &sp_existing, const sbuf_t &sbuf_new):
        //register_info(register_info_),
        //owner(owner_),
        config(sp_existing.config),
        phase(sp_existing.phase),
        sbuf(sbuf_new),
        fs(sp_existing.fs),
        print_options(sp_existing.print_options),
        depth(sp_existing.depth+1){ };
#endif

    class scanner_set           &ss;           // the scanner set calling this scanner. Includes the scanner_config and feature_
    //register_info_t             register_info; // callback function to register the scanner
    //const class scanner_config  &config;       // configuration for all scanners.
    const phase_t               phase;         // what scanner should do
    const sbuf_t                &sbuf;         // what to scan / only valid in SCAN_PHASE
    //class feature_recorder_set  &fs;           // where to put the results / only valid in SCAN_PHASE
    PrintOptions                print_options {}; // how to print. Default is that there are no options
    const uint32_t              depth {0};     //  how far down are we? / only valid in SCAN_PHASE
    std::stringstream           *sxml{};       //  on scanning and shutdown: CDATA added to XML stream if provided

    std::string const &get_input_fname() const;
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
