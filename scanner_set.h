/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef SCANNER_SET_H
#define SCANNER_SET_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include <sstream>

#include "scanner_params.h"
#include "scanner_config.h"
#include "sbuf.h"
#include "atomic_set_map.h"

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

#include "packet_info.h"
#include "feature_recorder_set.h"

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

    // The commands for those scanners (enable, disable, options, etc.
    typedef std::vector<struct scanner_command> scanner_commands_t;
    scanner_commands_t scanner_commands {};
    void    process_scanner_commands(const std::vector<scanner_command> &scanner_commands); // process the commands

    /* The scanners that are registered and enabled */

    // Map the scanner name to the scanner pointer
    std::map<scanner_t *, const struct scanner_params::scanner_info *>scanner_info_db {};
    std::set<scanner_t *> enabled_scanners {};    // the scanners that are enabled

    // scanner_stats
    struct stats {
        std::atomic<uint64_t> ns    {0};    // nanoseconds
        std::atomic<uint64_t> calls {0}; // calls
    };
    atomic_map<scanner_t *,struct stats *> scanner_stats {}; // maps scanner name to performance stats


    // a pointer to every scanner info in all of the scanners.
    // This provides all_scanners

    // The scanner_set's configuration for all the scanners that are loaded.
    const  scanner_config sc;

    /* The feature recorder set where the scanners outputs are stored */
    class  feature_recorder_set fs;

    /* Run-time configuration for all of the scanners (per-scanner configuration is stored in sc)
     * Default values are hard-coded below.
     */

    unsigned int max_depth                   {7};       // maximum depth for recursive scans
    std::atomic<unsigned int> max_depth_seen {0};
    std::atomic<uint64_t> sbuf_seen {0}; // number of seen sbufs.

    uint32_t max_ngram             {10};      // maximum ngram size to scan for
    bool     dup_data_alerts       {false};   // notify when duplicate data is not processed
    std::atomic<uint64_t> dup_bytes_encountered  {0}; // amount of dup data encountered
    std::ostream *sxml             {nullptr}; // if provided, a place to put XML tags when scanning

    scanner_params::phase_t     current_phase {scanner_params::PHASE_INIT};




public:;
    /* constructor and destructor */
    scanner_set(const scanner_config &sc, const feature_recorder_set::flags_t &f, std::ostream *sxml=0);
    virtual ~scanner_set(){};

    /* PHASE_INIT */
    // Add scanners to the scanner set.

    /* Debug for feature recorders. This used to be a flag, but Stroustrup (2013) recommends just having
     * a bunch of bools.
     */
    struct debug_flags_t {
        bool debug_print_steps {false}; // prints as each scanner is started
        bool debug_scanner {false};     // dump all feature writes to stderr
        bool debug_no_scanners {false}; // run with no scanners
        bool debug_dump_data {false};   // scanners should dump data as they see them
        bool debug_decoding {false};    // scanners should dump information on decoding as they see them
        bool debug_info {false};        // print extra info
        bool debug_exit_early {false};  // just print the size of the volume and exit
        bool debug_allocate_512MiB;     // allocate 512MiB but don't set any flags
    } debug_flags {};

    /* Scanners can be compiled in (which are passed to the constructor), loaded one-by-one from meory,
     * or loaded from a file, a directory, or a set of directories.
     * Loaded scanners are added to the 'scanners' vector.
     *
     * After the scanners are loaded, the scan starts.
     * Each scanner is called with scanner_params and a scanner control block as arguments.
     * See "scanner_params.h".
     */
    void    register_info(const scanner_params::scanner_info *si);
    void    add_scanner(scanner_t scanner);      // load a specific scanner in memory
    void    add_scanners(scanner_t * const *scanners_builtin); // load a nullptr array of scanners.
    void    add_scanner_file(std::string fn);    // load a scanner from a shared library file
    void    add_scanner_directory(const std::string &dirname); // load all scanners in the directory

    void    load_scanner_packet_handlers(); // after all scanners are loaded, this sets up the packet handlers.

    /* Control which scanners are enabled */
    void    set_scanner_enabled(const std::string &name, bool shouldEnable); // enable/disable a specific scanner
    void    set_scanner_enabled_all(bool shouldEnable); // enable/disable all scanners

    bool    is_scanner_enabled(const std::string &name); // report if it is enabled or not
    void    get_enabled_scanners(std::vector<std::string> &svector); // put names of the enabled scanners into the vector
    bool    is_find_scanner_enabled(); // return true if a find scanner is enabled

    /* These functions must be virtual so they can be called by dynamically loaded plugins */
    /* They throw a ScannerNotFound exception if no scanner exists */

    class NoSuchScanner : public std::exception {
        std::string m_error{};
    public:
        NoSuchScanner(std::string_view error):m_error(error){}
        const char *what() const noexcept override {return m_error.c_str();}
    };
    const std::string get_scanner_name(scanner_t scanner) const; // returns the name of the scanner
    virtual scanner_t &get_scanner_by_name(const std::string &name) ;
    virtual feature_recorder &named_feature_recorder(const std::string &name);

    // report on the loaded scanners
    void     info_scanners(std::ostream &out,
                           bool detailed_info,bool detailed_settings,
                           const char enable_opt,const char disable_opt);

    /* Control the histograms set up during initialization phase */
    const std::string & get_input_fname() const;

    /* PHASE SCAN */

    /* Managing scanners */
    // TK - should this be an sbuf function?
    size_t find_ngram_size(const sbuf_t &sbuf) const;

    //void  get_scanner_feature_file_names(feature_file_names_t &feature_file_names);

    // enabling and disabling of scanners
    static std::string ALL_SCANNERS;

    // returns the named scanner, or 0 if no scanner of that name

    // Scanners automatically get initted when they are loaded, so there is no scanners init or info phase
    // They are immediately ready to process sbufs and packets!
    // These trigger a move the PHASE_SCAN
    void     phase_scan();              // start the scan phase

    void     process_sbuf(const sbuf_t &sbuf);                              /* process for feature extraction */
    void     process_packet(const be13::packet_info &pi);
    scanner_params::phase_t get_current_phase() const { return current_phase;};
    // make the histograms
    // sxml is where to put XML from scanners that shutdown
    // the sxml should go to the constructor

    uint32_t get_max_depth_seen() const;

    /* PROCESS HISTOGRAMS */
    void     process_histograms();

    /* PHASE_SHUTDOWN */
    void     shutdown(class dfxml_writer *);
};

#endif
