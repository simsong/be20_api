/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef SCANNER_PARAMS_H
#define SCANNER_PARAMS_H

/*
 * Interface for individual scanners.
 */

#include <cassert>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iterator>
#include <memory>

// Note: Do not include scanner_set.h, because it needs scanner_params.h!

#include "histogram_def.h"
//#include "packet_info.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "sbuf.h"
#include "scanner_config.h"

/** A scanner is a function that takes a reference to scanner params and a recrusion control block */
typedef void scanner_t(struct scanner_params& sp);

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
        // std::stringstream helpstream {}; // where scanner info help messages are saved.

        // default copy construction and assignment are meaningless
        // and not implemented
        // scanner_info(const scanner_info &i)=delete;
        // scanner_info &operator=(const scanner_info &i)=delete;

        const static inline size_t DEFAULT_MIN_SBUF_SIZE=16; // by default, do not scan sbufs smaller than this.

        struct scanner_flags_t {
            bool default_enabled{true}; //  enabled by default
            bool no_usage{false};       //  do not show scanner in usage
            bool no_all{false};         //  do not enable with -eall
            bool find_scanner{false};   //  this scanner uses the find_list
            bool recurse{false};        //  this scanner will recurse
            bool recurse_expand{false}; //  recurses AND result is >= original size
            bool recurse_always{false};    //  this scanner performs no data validation and ALWAYS recurses.
            bool scan_ngram_buffer{
                false}; //  Scanner can run even if the entire gets buffer is filled with constant n-grams
            bool scan_seen_before{false}; //  Scanner can run even if buffer has seen before
            bool fast_find{false};        //  This scanner is a very fast FIND scanner
            bool depth0_only{false};      //  scanner only runs at depth 0 by default

            const std::string asString() const {
                std::string ret;
                ret += default_enabled ? "ENABLED" : "DISABLED";
                if (no_usage) ret += " NO_USAGE";
                if (no_all) ret += " NO_ALL";
                if (find_scanner) ret += " FIND_SCANNER";
                if (recurse) ret += " RECURSE";
                if (recurse_expand) ret += " RECURSE_EXPAND";
                if (scan_ngram_buffer) ret += " SCAN_NGRAM_BUFFER";
                if (scan_seen_before) ret += " SCAN_SEEN_BEFORE";
                if (fast_find) ret += " FAST_FIND";
                if (depth0_only) ret += " DEPTH0_ONLY";
                return ret;
            }
        } scanner_flags{};

        std::string ToUpper(const std::string input) {
            std::string ret;
            for (auto ch:input){
                ret.push_back(std::toupper(ch));
            }
            return ret;
        }

        // constructor. We must have the name and the pointer. Everything else is optional
        scanner_info(scanner_t* scanner_, std::string name_) :
            scanner(scanner_), name(name_), pathPrefix(ToUpper(name_)){
        };
        /* PASSED FROM SCANNER to API: */
        scanner_t* scanner;            // the scanner
        std::string name;              // scanner name
        std::string pathPrefix{};      //   this scanner's path prefix for recursive scanners. e.g. "GZIP". Typically name uppercase
        std::string helpstr{};         // the help string
        std::string author{};          //   who wrote me?
        std::string description{};     //   what do I do?
        std::string url{};             //   where I come from
        std::string scanner_version{}; //   version for the scanner
        std::vector<feature_recorder_def> feature_defs{}; //   feature files that this scanner needs.
        size_t   min_sbuf_size {DEFAULT_MIN_SBUF_SIZE}; // minimum sbuf to scan
        uint64_t flags{};              //   flags
        std::vector<histogram_def> histogram_defs{};      //   histogram definitions that the scanner needs

        // Derrived:

        // void              *packet_user {};        //   data for network callback
        // be13::packet_callback_t *packet_cb {};    //   callback for processing network packets, or NULL

        // Move constructor
        scanner_info(scanner_info&& source)
            : scanner(source.scanner), name(source.name), pathPrefix(source.pathPrefix),
              helpstr(source.helpstr), description(source.description),
              url(source.url), scanner_version(source.scanner_version),
              feature_defs(source.feature_defs), flags(source.flags), histogram_defs(source.histogram_defs) {}
    };

    /* Scanners can also be asked to assist in printing. */
    enum print_mode_t { MODE_NONE = 0, MODE_HEX, MODE_RAW, MODE_HTTP };
    const int SCANNER_PARAMS_VERSION{20210531};
    int scanner_params_version{SCANNER_PARAMS_VERSION};
    void check_version() { assert(this->scanner_params_version == SCANNER_PARAMS_VERSION); }

    typedef std::map<std::string, std::string> PrintOptions;
    static print_mode_t getPrintMode(const PrintOptions& po) {
        PrintOptions::const_iterator p = po.find("print_mode_t");
        if (p != po.end()) {
            if (p->second == "MODE_NONE") return MODE_NONE;
            if (p->second == "MODE_HEX") return MODE_HEX;
            if (p->second == "MODE_RAW") return MODE_RAW;
            if (p->second == "MODE_HTTP") return MODE_HTTP;
        }
        return MODE_NONE;
    }
    static void setPrintMode(PrintOptions& po, int mode) {
        switch (mode) {
        default:
        case MODE_NONE: po["print_mode_t"] = "MODE_NONE"; return;
        case MODE_HEX: po["print_mode_t"] = "MODE_HEX"; return;
        case MODE_RAW: po["print_mode_t"] = "MODE_RAW"; return;
        case MODE_HTTP: po["print_mode_t"] = "MODE_HTTP"; return;
        }
    }

    // phase_t specifies when the scanner is being called.
    // the scans are implemented in the scanner set
    enum phase_t {
        PHASE_INIT,    // called in main thread when scanner loads
        PHASE_ENABLED, // enable/disable commands called
        PHASE_INIT2,    // called in main thread to get the feature recorders and so forth.
        PHASE_SCAN,    // called in worker thread for every ENABLED scanner to scan an sbuf
        PHASE_SHUTDOWN // called in main thread for every ENABLED scanner when scanner is shutting down. Allows XML
                       // closing.
    };

    /*
     * CONSTRUCTORS
     *
     * Scanner_params are made whenever a scanner needs to be called. These constructors cover all of those cases.
     */
    scanner_params(const scanner_params&) = delete;
    scanner_params& operator=(const scanner_params&) = delete;

    /* A scanner params with all of the instance variables, typically for scanning  */
    scanner_params(struct scanner_config &sc_, class scanner_set  *ss_, phase_t phase_,
                   const sbuf_t* sbuf_, PrintOptions print_options_, std::stringstream* xmladd);


    /* User-servicable parts; these are here for scanners */
    virtual ~scanner_params(){};
    virtual feature_recorder& named_feature_recorder(const std::string feature_recorder_name) const;
    template <typename T> void get_config(const std::string& name, T* val, const std::string& help) const {
        sc.get_config(name, val, help);
    };

    /** Construct a scanner_params for recursion from an existing sp and a new sbuf.
     * Defaults to phase1.
     * This is the complicated one!
     */
    scanner_params(const scanner_params& sp_existing, const sbuf_t* sbuf_);
#if 0
    /* A scanner params with no print options */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_):
        phase(phase_),sbuf(sbuf_),fs(fs_){ }

    /* A scanner params with no print options but an xmlstream */
    scanner_params(phase_t phase_, const sbuf_t &sbuf_, class feature_recorder_set &fs_,
                   std::stringstream *xmladd):
        phase(phase_),sbuf(sbuf_),fs(fs_),sxml(xmladd){ }

#endif

    struct scanner_config &sc;                 // configuration.
    class scanner_set *ss {nullptr}; // null in PHASE_INIT. the scanner set calling this scanner. Failed when we made it a &ss. Not that this is frequently a mt_scanner_set.
    std::unique_ptr<struct scanner_info> info {nullptr};  // filled in by callback in PHASE_INIT
    const phase_t phase{};        // what scanner should do
    const sbuf_t* sbuf{nullptr};  // what to scan / only valid in SCAN_PHASE
    PrintOptions print_options{}; // how to print. Default is that there are no options
    const uint32_t depth{0};      //  how far down are we? / only valid in SCAN_PHASE
    std::stringstream* sxml{};    //  on scanning and shutdown: CDATA added to XML stream if provided
    std::filesystem::path const get_input_fname() const; // not sure why this is needed?

    virtual void recurse(sbuf_t* sbuf) const; // recursive call by scanner
    virtual bool check_previously_processed(const sbuf_t &sbuf) const;
};

//template <> void scanner_params::get_config(const std::string& name, std::string* val, const std::string& help);
//template <> void scanner_params::get_config(const std::string& name, signed char* val, const std::string& help);
//template <> void scanner_params::get_config(const std::string& name, unsigned char* val, const std::string& help);
//template <> void scanner_params::get_config(const std::string& name, bool* val, const std::string& help);

inline std::ostream& operator<<(std::ostream& os, const scanner_params& sp) {
    os << "scanner_params(" << sp.sbuf << ")";
    return os;
};

#endif
