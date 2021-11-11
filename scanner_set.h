/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#ifndef SCANNER_SET_H
#define SCANNER_SET_H

#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include "utils.h"
#include "thread-pool/thread_pool.hpp"
#include "atomic_map.h"
#include "sbuf.h"
#include "scanner_config.h"
#include "scanner_params.h"

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
 * \li \c phase_init     - scanners are loaded and set up the names of the feature files they want.
 * \li \c phase_enabled  - enabled - enable/disable commands called
 * \li \c phase_init2    - single-threaded call to each scanner in order to get a handle to the feature recorders.
 * \li \c phase_scan - each scanner is called to analyze 1 or more sbufs.
 * \li \c phase_shutdown - scanners are given a chance to shutdown
 *
 * The scanner_set references the feature_recorder_set, which is a set of feature_recorder objects.
 *
 * The scanner_set controls running of the scanners. It can run in a single-threaded mode, having a single
 * sbuf processed recursively within a single thread.
 * TODO: or it can be called with a threadpool.
 */

//#include "packet_info.h"
#include "feature_recorder_set.h"
#include "scanner_params.h" // needed for scanner_t

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
    scanner_set(const scanner_set& s) = delete;
    scanner_set& operator=(const scanner_set& s) = delete;

public:
    /* constructor and destructor */
    /* @param sc - the config variables
       @param f - the flags for the feature recorder set we make
       @param writer - the DFXML writer to use, or nullptr.
    */
    scanner_set(scanner_config& sc, const feature_recorder_set::flags_t& f, class dfxml_writer* writer);
    virtual ~scanner_set();
    scanner_config sc;                             // scanner_set configuration; passed to feature_recorder_set

    class dfxml_writer* writer {nullptr};          // if provided, a dfxml writer. Mutext locking done by dfxml_writer.h
    void set_dfxml_writer(class dfxml_writer *writer_);
    class dfxml_writer *get_dfxml_writer() const;

    /* They throw a ScannerNotFound exception if no scanner exists */
    class NoSuchScanner : public std::exception {
        std::string m_error{};
    public:
        NoSuchScanner(std::string_view error) : m_error(error) {}
        const char* what() const noexcept override { return m_error.c_str(); }
    };

    struct stats {
        explicit stats(){};
        explicit stats(uint64_t a, uint64_t b):ns(a),calls(b){};
        explicit stats(const stats &s):ns(s.ns),calls(s.calls){};
        stats operator+(const stats &s) {
            return stats(this->ns + s.ns, this->calls + s.calls);
        }
        stats &operator+=(const stats &s) {
            this->ns += s.ns;
            this->calls += s.calls;
            return *this;
        }
        uint64_t ns{0};    // nanoseconds
        uint64_t calls{0}; // calls
    };

    struct debug_flags_t {
        bool debug_print_steps{false};     // prints as each scanner is started
        bool debug_scanner{false};         // dump all feature writes to stderr
        bool debug_no_scanners{false};     // run with no scanners
        bool debug_dump_data{false};       // scanners should dump data as they see them
        bool debug_decoding{false};        // scanners should dump information on decoding as they see them
        bool debug_info{false};            // print extra info
        bool debug_exit_early{false};      // just print the size of the volume and exit
        bool debug_allocate_512MiB{false}; // allocate 512MiB but don't set any flags
        bool debug_register{false};        // print when scanners register
        std::string debug_scanners_ignore{}; // ignore these scanners, separated by :
    } debug_flags{};

    // Scanner database.
    std::map<scanner_t*, std::unique_ptr<struct scanner_params::scanner_info>> scanner_info_db{};
    std::set<scanner_t*> enabled_scanners{}; // the scanners that are enabled
    const std::string get_scanner_name(scanner_t scanner) const; // returns the name of the scanner
    virtual scanner_t* get_scanner_by_name(const std::string name) const;

    // Multi-threaded support
private:
    static const inline size_t SAME_THREAD_SBUF_SIZE = 8192; // sbufs smaller than this run in the same thread.
    class thread_pool *pool {nullptr}; // nullptr means we are not threading

public:;
    static uint64_t get_available_memory();
    static double get_cpu_percentage();

    static const inline std::string THREAD_COUNT_STR {"thread_count"};
    static const inline std::string TASKS_QUEUED_STR {"tasks_queued"};
    static const inline std::string DEPTH0_SBUFS_QUEUED_STR {"depth0_sbufs_queued"};
    static const inline std::string DEPTH0_BYTES_QUEUED_STR {"depth0_bytes_queued"};
    static const inline std::string SBUFS_QUEUED_STR {"sbufs_queued"};
    static const inline std::string BYTES_QUEUED_STR {"bytes_queued"};
    static const inline std::string AVAILABLE_MEMORY_STR {"available_memory"};
    static const inline std::string SBUFS_CREATED_STR {"sbufs_created"};
    static const inline std::string SBUFS_REMAINING_STR {"sbufs_remaining"};
    static const inline std::string MAX_OFFSET {"max_offset"};

    int get_thread_count() { return (pool!=nullptr) ? pool->get_thread_count() : 1; };
    std::atomic<int>      depth0_sbufs_in_queue {0};
    std::atomic<uint64_t> depth0_bytes_in_queue {0};
    std::atomic<int>      sbufs_in_queue {0};
    std::atomic<uint64_t> bytes_in_queue {0};
    std::atomic<int>      disk_write_errors {0};
    mutable std::atomic<uint64_t> max_offset {0}; // largest offset read by any of the threads.

    // thread status and notification
    atomic_map<std::thread::id, std::string> thread_status {}; // the status of each thread::id

    // to get a copy of thread_status, use get_stats, which also returns information about the queue
    std::map<std::string,std::string> get_realtime_stats() const;   // thread-safe return of a copy of threadpool stats; for notification APIs.

    // thread interface
    void launch_workers(int count);
    void update_queue_stats(sbuf_t *sbufp, int dir);   // either +1 increment or -1 decrement
    void thread_set_status(const std::string &status); // designed to be overridden
    void join();                                       // join the threads

    // scanner stats
    bool record_call_stats {true};     // by default, record the call stats
    std::atomic<uint64_t> sbuf_seen{0}; // number of seen sbufs.
    std::atomic<uint64_t> dup_bytes_encountered{0}; // amount of dup data encountered
    atomic_map<scanner_t* , struct stats> scanner_stats{}; // maps scanner name to performance stats
    void add_scanner_stat(scanner_t *, const struct stats &st);
    uint64_t get_dup_bytes_encountered()  const  { return dup_bytes_encountered; }
    uint32_t get_max_depth_seen() const          { return max_depth_seen;} ; // max seen during scan

    // per-path stats
    atomic_map<std::string, struct stats> path_stats{}; // maps scanner name to performance stats
    void add_path_stat(std::string path, const struct stats &st);

    unsigned int max_depth{7};   // maximum depth for recursive scans
    unsigned int log_depth{1};   // log to dfxml all sbufs at this depth or less
    std::atomic<uint32_t> max_depth_seen{0};

    // Feature recorders. Functions below are virtual so they can be called by loaded scanners.
    class feature_recorder_set fs;
    virtual feature_recorder& named_feature_recorder(const std::string name) const; // returns the feature recorder
    virtual std::vector<std::string> feature_file_list() const;                     // returns the list of feature files
    size_t histogram_count() const { return fs.histogram_count(); }; // passthrough, mostly for debugging
    size_t feature_recorder_count() const { return fs.feature_recorder_count(); };
    void dump_name_count_stats() const { if(writer) fs.dump_name_count_stats(*writer); }; // passthrough

    // Management of previously seen data
    // hex hash values of sbuf pages that have been seen
    atomic_map<std::string, std::atomic<uint64_t>> previously_processed_counter {};
    uint64_t previously_processed_count(const sbuf_t& sbuf);

    /* Run-time configuration for all of the scanners (per-scanner configuration is stored in sc)
     * Default values are hard-coded below.
     */

    template <typename T> void get_global_config(const std::string& name, T* val, const std::string& help) {
        return sc.get_global_config(name, val, help);
    }



    // Scanning
    scanner_params::phase_t current_phase{scanner_params::PHASE_INIT};
    scanner_params::phase_t get_current_phase() const { return current_phase; };

    /* PHASE_INIT SUPPORT */
    void add_scanner(scanner_t scanner);                    // load a specific scanner in memory
    void add_scanners(scanner_t* const* scanners_builtin);  // load a nullptr array of scanners.
    void add_scanner_file(std::string fn);                  // load a scanner from a shared library file
    void add_scanner_directory(const std::string& dirname); // load all scanners in the directory
    void info_scanners(std::ostream& out, bool detailed_info, bool detailed_settings,
                       const char enable_opt, const char disable_opt);
    void apply_scanner_commands(); // applies all of the enable/disable commands and create the feature recorders
    bool is_scanner_enabled(const std::string& name);      // report if it is enabled or not
    std::vector<std::string> get_enabled_scanners() const; // put names of the enabled scanners into the vector
    bool is_find_scanner_enabled();                        // return true if a find scanner is enabled
    void dump_enabled_scanner_config() const;  // dump the enabled scanners to dfxml writer
    void dump_scanner_stats() const;           // dump scanner stats to dfxml writer

    // hash support
    virtual std::string hash(const sbuf_t& sbuf) const;

    // report on the loaded scanners

    // dfxml support
    virtual void log(const std::string message); // writes message to dfxml and log
    virtual void log(const sbuf_t &sbuf, const std::string message); // writes sbuf if not too deep.


    /*
     * PHASE_ENABLE
     * Various scanners are enabled and their histograms are created
     */
    // void    load_scanner_packet_handlers(); // after all scanners are loaded, this sets up the packet handlers.

    const std::filesystem::path get_input_fname() const;


    /* PHASE SCAN */
    void phase_scan();               // start the scan phase
private:;
    void process_sbuf(sbuf_t* sbuf); // process the sbuf, then delete it.
public:;
    void record_work_start(const std::string &pos0, size_t pagesize, size_t bufsize); // note std::string
    void schedule_sbuf(sbuf_t* sbuf);  // schedule the sbuf to be processed, after which it is deleted
    void delete_sbuf(sbuf_t *sbuf);    // delete after processing

    // void     process_packet(const be13::packet_info &pi);

    /* PHASE_SHUTDOWN */
    // explicit shutdown, called automatically on delete if it hasn't be called
    // flushes all remaining histograms
    void shutdown();
};

#endif
