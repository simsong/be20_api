/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * common.cpp:
 * bulk_extractor backend stuff, used for both standalone executable and bulk_extractor.
 */

#include <cassert>
#include <string>
#include <vector>
#include <thread>


/* needed solely for loading shared libraries */
#include "config.h"

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include "thread-pool/thread_pool.hpp"
#include "aftimer.h"
#include "dfxml_cpp/src/dfxml_writer.h"
#include "dfxml_cpp/src/hash_t.h"
#include "formatter.h"
#include "scanner_config.h"
#include "scanner_set.h"

/****************************************************************
 *** SCANNER SET IMPLEMENTATION (previously the PLUG-IN SYSTEM)
 ****************************************************************/

/* BE2 revision:
 * In BE1, the active scanners were maintained by the plugin system's global state.
 * In BE2, there is no global state. Instead, scanners are grouped into scanner set, which
 *        in turn has a feature_recorder_set, which in turn has multiple feature recorders.
 *        The scanner set can then be asked to process a sbuf.
 *        All of the global variables go away.
 */

/* constructor and destructors */
scanner_set::scanner_set(scanner_config& sc_, const feature_recorder_set::flags_t& f, class dfxml_writer* writer_)
    : sc(sc_), writer(writer_), fs(f, sc_)
{
    if (getenv("DEBUG_SCANNER_SET_PRINT_STEPS")) debug_flags.debug_print_steps = true;
    if (getenv("DEBUG_SCANNER_SET_NO_SCANNERS")) debug_flags.debug_no_scanners = true;
    if (getenv("DEBUG_SCANNER_SET_SCANNER")) debug_flags.debug_scanner = true;
    if (getenv("DEBUG_SCANNER_SET_DUMP_DATA")) debug_flags.debug_dump_data = true;
    if (getenv("DEBUG_SCANNER_SET_DECODING")) debug_flags.debug_decoding = true;
    if (getenv("DEBUG_SCANNER_SET_INFO")) debug_flags.debug_info = true;
    if (getenv("DEBUG_SCANNER_SET_EXIT_EARLY")) debug_flags.debug_exit_early = true;
    if (getenv("DEBUG_SCANNER_SET_REGISTER")) debug_flags.debug_register = true;
    const char *dsi = getenv("DEBUG_SCANNERS_IGNORE");
    if (dsi!=nullptr) debug_flags.debug_scanners_ignore=dsi;
}

scanner_set::~scanner_set()
{
}

void scanner_set::set_dfxml_writer(class dfxml_writer *writer_)
{
    if (writer) {
        throw std::runtime_error("dfxml_writer already set");
    }
    writer = writer_;
}

class dfxml_writer *scanner_set::get_dfxml_writer() const
{
    return writer;
}


/****************************************************************
 *** scanner database
 ****************************************************************/

const std::string scanner_set::get_scanner_name(scanner_t scanner) const {
    auto it = scanner_info_db.find(scanner);
    if (it != scanner_info_db.end()) { return it->second->name; }
    return "";
}

scanner_t* scanner_set::get_scanner_by_name(const std::string search_name) const {
    for (const auto &it : scanner_info_db) {
        if (it.second->name == search_name) { return it.first; }
    }
    throw NoSuchScanner(search_name);
}

/****************************************************************
 *** thread interface
 ****************************************************************/

void scanner_set::notify_thread()
{
    while(true){
        time_t rawtime = time (0);
        struct tm *timeinfo = localtime (&rawtime);
        std::cerr << asctime(timeinfo) << "\n";
        print_tp_stats();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void scanner_set::launch_workers(int count)
{
    assert(pool == nullptr);
    pool = new thread_pool(count);
}

void scanner_set::update_queue_stats(sbuf_t *sbufp, int dir)
{
    assert (sbufp != nullptr );
    assert ( dir==1 || dir==-1);
    if (sbufp->depth()==0){
        depth0_sbufs_in_queue += 1 * dir;
        depth0_bytes_in_queue += sbufp->bufsize * dir;
        assert(depth0_sbufs_in_queue >= 0);
        assert(depth0_bytes_in_queue >= 0);
    }
    sbufs_in_queue += 1 * dir;
    bytes_in_queue += sbufp->bufsize * dir;

    assert(sbufs_in_queue >= 0);
    assert(sbufs_in_queue >= depth0_sbufs_in_queue);
    assert(bytes_in_queue >= depth0_bytes_in_queue);
}


void scanner_set::thread_set_status(const std::string &status)
{
    thread_status[std::this_thread::get_id()] = status;
}

/*
 * Print the status of each thread in the threadpool.
 */
void scanner_set::print_tp_stats()
{
    if (pool==nullptr) return;

    std::cout << "---enter print_tp_status----------------\n";
    std::cerr << "tasks queued: " << pool->get_tasks_queued() << "\n";
    std::cerr << "depth 0 sbufs in queue " << depth0_sbufs_in_queue << "\n";
    std::cerr << "depth 0 bytes in queue " << depth0_bytes_in_queue << "\n";
    std::cerr << "sbufs in queue " << sbufs_in_queue << "\n";
    std::cerr << "bytes in queue " << bytes_in_queue << "\n";
#ifdef BSD_STYLE
    show_free_bsd(std::cerr);
#endif
    std::cout << "----------------------------------------\n";
}

void scanner_set::join()
{
    if (pool != nullptr) {
        pool->wait_for_tasks();
    }
}

void scanner_set::launch_notify_thread()
{
    notifier = new std::thread(&scanner_set::notify_thread, this);
}

/****************************************************************
 *** scanner stats
 ****************************************************************/

void scanner_set::add_scanner_stat(scanner_t *scanner, const struct scanner_set::stats &n)
{
    scanner_stats[scanner] += n;
}


/****************************************************************
 *** per-path stats
 ****************************************************************/

void scanner_set::add_path_stat(std::string path, const struct scanner_set::stats &n)
{
    path_stats[path] += n;
}



/****************************************************************
 *** Feature recorders
 ****************************************************************/

feature_recorder& scanner_set::named_feature_recorder(const std::string name) const
{
    return fs.named_feature_recorder(name);
}

std::vector<std::string> scanner_set::feature_file_list() const
{
    return fs.feature_file_list();
}

/****************************************************************
 *** Scanning
 ****************************************************************/


void scanner_set::add_scanner(scanner_t scanner) {
    /* If scanner is already loaded, that's an error */
    if (scanner_info_db.find(scanner) != scanner_info_db.end()) { throw std::runtime_error("scanner already added"); }

    /* Initialize the scanner.
     * Use an empty sbuf and an empty feature recorder to create an empty scanner params that is in PHASE_STARTUP.
     * We then ask the scanner to initialize.
     */
    scanner_params::PrintOptions po;
    scanner_params sp(sc, this, scanner_params::PHASE_INIT, nullptr, po, nullptr);

    // Send the scanner the PHASE_INIT message, which will cause it to fill in the sp.info field.
    (*scanner)(sp);

    // The scanner should have set the info field.
    if (sp.info == nullptr) {
        throw std::runtime_error("scanner_set::add_scanner: a scanner did not set the sp.info field.  "
                                 "Re-run with DEBUG_SCANNER_SET_REGISTER=1 to find those that did.");
    }
    if (debug_flags.debug_scanners_ignore.find(sp.info->name) != std::string::npos){
        std::cerr << "DEBUG: ignore add_scanner " << sp.info->name << "\n";
        return;
    }
    if (debug_flags.debug_register) {
        std::cerr << "add_scanner( " << sp.info->name << " )\n";
    }
    scanner_info_db[scanner] = std::move(sp.info);

    // Make sure it was registered
    if (scanner_info_db.find(scanner) == scanner_info_db.end()) {}

    // Enable the scanner if it is not disabled by default.
    if (scanner_info_db[scanner]->scanner_flags.default_enabled) { enabled_scanners.insert(scanner); }
}

/* add_scanners allows a calling program to add a null-terminated array of scanners. */
void scanner_set::add_scanners(scanner_t* const* scanners)
{
    for (int i = 0; scanners[i]; i++) { add_scanner(scanners[i]); }
}

/* Add a scanner from a file (it's in the file as a shared library) */
void scanner_set::add_scanner_file(std::string fn)
{
    if(time(0)>10) {
        throw std::runtime_error("not implemented yet.");
    }
#if 0
    /* Figure out the function name */
    size_t extloc = fn.rfind('.');
    if(extloc==std::string::npos){
        fprintf(stderr,"Cannot find '.' in %s",fn.c_str());
        exit(1);
    }
    std::string func_name = fn.substr(0,extloc);
    size_t slashloc = func_name.rfind('/');
    if(slashloc!=std::string::npos) func_name = func_name.substr(slashloc+1);
    slashloc = func_name.rfind('\\');
    if(slashloc!=std::string::npos) func_name = func_name.substr(slashloc+1);

    if(debug) std::cout << "Loading: " << fn << " (" << func_name << ")\n";
    scanner_t *scanner = 0;
#if defined(HAVE_DLOPEN)
    void *lib=dlopen(fn.c_str(), RTLD_LAZY);

    if(lib==0){
        fprintf(stderr,"dlopen: %s\n",dlerror());
        exit(1);
    }

    /* Resolve the symbol */
    scanner = (scanner_t *)dlsym(lib, func_name.c_str());

    if(scanner==0){
        fprintf(stderr,"dlsym: %s\n",dlerror());
        exit(1);
    }
#elif defined(HAVE_LOADLIBRARY)
    /* Use Win32 LoadLibrary function */
    /* See http://msdn.microsoft.com/en-us/library/ms686944(v=vs.85).aspx */
    HINSTANCE hinstLib = LoadLibrary(TEXT(fn.c_str()));
    if(hinstLib==0){
        fprintf(stderr,"LoadLibrary(%s) failed",fn.c_str());
        exit(1);
    }
    scanner = (scanner_t *)GetProcAddress(hinstLib,func_name.c_str());
    if(scanner==0){
        fprintf(stderr,"GetProcAddress(%s) failed",func_name.c_str());
        exit(1);
    }
#else
    std::cout << "  ERROR: Support for loadable libraries not enabled\n";
    return;
#endif
    load_scanner(*scanner,sc);
#endif
}

/* Add all of the scanners in a directory */
void scanner_set::add_scanner_directory(const std::string &dirname)
{
#if 0
TODO: Re-implement using C++17 directory reading.


        if(fname.substr(0,5)=="scan_" || fname.substr(0,5)=="SCAN_"){
            size_t extloc = fname.rfind('.');
            if(extloc==std::string::npos) continue; // no '.'
            std::string ext = fname.substr(extloc+1);
#ifdef WIN32
            if(ext!="DLL") continue;    // not a DLL
#else
            if(ext!="so") continue;     // not a shared library
#endif
            load_scanner_file(dirname+"/"+fname,sc );
        }
    }
#endif
}

/* This interface creates if we are in init phase, doesn't if we are in scan phase */
/**
 * Print a list of scanners.
 * We need to load them to do this, so they are loaded with empty config
 * Note that scanners can only be loaded once, so this exits.
 */
#if 0
bool cmp(const struct scanner_params::scanner_info* a,
         const struct scanner_params::scanner_info* b)
{
    return a->name < b->name;
}
#endif
void scanner_set::info_scanners(std::ostream& out, bool detailed_info, bool detailed_settings, const char enable_opt,
                                const char disable_opt) {
    /* Get a list of scanner names */
    std::vector<std::string> all_scanner_names,enabled_scanner_names, disabled_scanner_names;
    for (auto &it : scanner_info_db) {
        all_scanner_names.push_back( it.second->name );
    }

    /* Sort them */
    sort (all_scanner_names.begin(), all_scanner_names.end());

    /* Now print info on each scanner in alphabetical order */
    for (const auto &name : all_scanner_names) {
        scanner_t *scanner = get_scanner_by_name(name);
        const struct scanner_params::scanner_info &scanner_info  = *(scanner_info_db[scanner]);
        if (detailed_info) {
            if (scanner_info.name.size()) out << "Scanner Name: " << scanner_info.name;
            if (is_scanner_enabled(scanner_info.name)) { out << " (ENABLED) "; }
            out << "\n";
            out << "flags:  " << scanner_info.scanner_flags.asString() << "\n";
            if (scanner_info.author.size()) out << "Author: " << scanner_info.author << "\n";
            if (scanner_info.description.size()) out << "Description: " << scanner_info.description << "\n";
            if (scanner_info.url.size()) out << "URL: " << scanner_info.url << "\n";
            if (scanner_info.scanner_version.size()) out << "Scanner Version: " << scanner_info.scanner_version << "\n";
            out << "Min sbuf size: " << scanner_info.min_sbuf_size << "\n";
            out << "Feature Names: ";
            int count = 0;
            for (auto i2 : scanner_info.feature_defs) {
                if (count++ > 0) out << ", ";
                out << i2.name;
            }
            if (count == 0) { out << "(none)"; }
            out << "\n";
            if (detailed_settings) {
                out << "Settable Options (and their defaults): \n";
                out << scanner_info.helpstr;
            }
            out << "------------------------------------------------\n\n";
        }
        if (scanner_info.scanner_flags.no_usage) continue;
        if (is_scanner_enabled(scanner_info.name)) {
            enabled_scanner_names.push_back(scanner_info.name);
        } else {
            disabled_scanner_names.push_back(scanner_info.name);
        }
    }
    if (enabled_scanner_names.size()) {
        out << "These scanners enabled; disable with -" << disable_opt << ":\n";
        for (const auto &it : enabled_scanner_names) {
            out << "   -" << disable_opt << " " << it << " - disable scanner " << it << "\n";
        }
    }
    if (disabled_scanner_names.size()) {
        out << "These scanners disabled; enable with -" << enable_opt << ":\n";
        sort(disabled_scanner_names.begin(), disabled_scanner_names.end());
        for (const auto &it : disabled_scanner_names) {
            out << "   -" << enable_opt << " " << it << " - enable scanner " << it << "\n";
        }
    }
}


/**
 * apply_sacanner_commands:
 * applies all of the enable/disable commands and create the feature recorders
 */

void scanner_set::apply_scanner_commands() {
    if (current_phase != scanner_params::PHASE_INIT) {
        throw std::runtime_error("apply_scanner_commands can only be run in scanner_params::PHASE_INIT");
    }
    for (const auto &cmd : sc.scanner_commands) {
        if (cmd.scannerName == scanner_config::scanner_command::ALL_SCANNERS) {
            /* If name is 'all' and the NO_ALL flag is not set for that scanner,
             * then either enable it or disable it as appropriate
             */
            for (const auto &it : scanner_info_db) {
                if (it.second->scanner_flags.no_all) {
                    continue;
                }
                if (cmd.command == scanner_config::scanner_command::ENABLE) {
                    enabled_scanners.insert(it.first);
                } else {
                    enabled_scanners.erase(it.first);
                }
            }
        } else {
            /* enable or disable this scanner */
            scanner_t* scanner = get_scanner_by_name(cmd.scannerName);
            if (cmd.command == scanner_config::scanner_command::ENABLE) {
                enabled_scanners.insert(scanner);
            } else {
                enabled_scanners.erase(scanner);
            }
        }
    }

    /* Create all of the requested feature recorders.
     * Multiple scanners may request the same feature recorder without generating an error.
     */
    fs.create_alert_recorder();
    for (const auto &sit : scanner_info_db) {
        for (const auto &it : sit.second->feature_defs) {
            fs.create_feature_recorder(it);
        }

        /* Create all of the requested histograms
         * Multiple scanners may request the same histograms without generating an error.
         */
        for (const auto &it : sit.second->histogram_defs) {
            // Make sure that a feature recorder exists for the feature specified in the histogram
            fs.named_feature_recorder(it.feature).histogram_add(it);
        }
    }

    /* Tell each of the enabled scanners to init */
    current_phase = scanner_params::PHASE_INIT2;
    scanner_params::PrintOptions po; // empty po
    scanner_params sp(sc, this, scanner_params::PHASE_INIT2, nullptr, po, nullptr);
    for (const auto &it : enabled_scanners) { (*it)(sp); }

    /* set the carve defaults */
    fs.set_carve_defaults();
    current_phase = scanner_params::PHASE_ENABLED;
}

bool scanner_set::is_scanner_enabled(const std::string& name)
{
    scanner_t* scanner = get_scanner_by_name(name);
    return enabled_scanners.find(scanner) != enabled_scanners.end();
}

// put the enabled scanners into the vector
std::vector<std::string> scanner_set::get_enabled_scanners() const
{
    std::vector<std::string> ret;
    for (const auto &it : enabled_scanners) {
        auto f = scanner_info_db.find(it);
        ret.push_back(f->second->name);
    }
    return ret;
}

// Return true if any of the enabled scanners are a FIND scanner
bool scanner_set::is_find_scanner_enabled()
{
    for (const auto &it : enabled_scanners) {
        if (scanner_info_db[it]->scanner_flags.find_scanner) { return true; }
    }
    return false;
}



/****************************************************************
 *** PHASE_ENABLE methods.
 ****************************************************************/

const std::filesystem::path scanner_set::get_input_fname() const
{
return sc.input_fname;
}



/****************************************************************
 *** PHASE_SCAN methods.
 ****************************************************************/

/* Transition to the scanning phase */
void scanner_set::phase_scan() {
    if (current_phase != scanner_params::PHASE_ENABLED) {
        throw std::runtime_error("start_scan can only be run in scanner_params::PHASE_ENABLED");
    }
    fs.frm_freeze();
    current_phase = scanner_params::PHASE_SCAN;
}

// https://stackoverflow.com/questions/16190078/how-to-atomically-update-a-maximum-value
template <typename T> void update_maximum(std::atomic<T>& maximum_value, T const& value) noexcept {
    T prev_value = maximum_value;
    while (prev_value < value && !maximum_value.compare_exchange_weak(prev_value, value)) {}
}

/* Process an sbuf!
 * Deletes the buf after processing.
 */
void scanner_set::process_sbuf(class sbuf_t* sbufp) {
    assert(sbufp != nullptr);
    assert(sbufp->children == 0); // we are going to free it, so it better not have any children.
    thread_set_status( sbufp->pos0.str() + " process_sbuf (" + std::to_string(sbufp->bufsize) + ")" );
    //std::cerr << "process_sbuf: " << *sbufp << "\n";

    /* If we  have not transitioned to PHASE::SCAN, error */
    if (current_phase != scanner_params::PHASE_SCAN) {
        throw std::runtime_error("process_sbuf can only be run in scanner_params::PHASE_SCAN");
    }

    if (sbufp->bufsize==0){
        delete_sbuf(sbufp);
        return;        // nothing to scan
    }

    const class sbuf_t& sbuf = *sbufp; // don't allow modification
    aftimer timer;
    timer.start();

    const pos0_t& pos0 = sbuf.pos0;
    /* If we are too deep, error out */
    if (sbuf.depth() >= max_depth) {
        fs.get_alert_recorder().write(pos0,
                                      feature_recorder::MAX_DEPTH_REACHED_ERROR_FEATURE,
                                      feature_recorder::MAX_DEPTH_REACHED_ERROR_CONTEXT);
        delete_sbuf(sbufp);
        return;        // nothing to scan
    }

    update_maximum<unsigned int>(max_depth_seen, sbuf.depth());
    auto pool_hold = (void*)pool;

    /* Determine if we have seen this buffer before */
    bool seen_before = previously_processed_count(sbuf) > 0;
    if (seen_before) {
        dup_bytes_encountered += sbuf.bufsize;
    }
    auto pool_now = (void *)pool;
    if(pool_now != pool_hold){
        std::cerr << "scanner_set::process_sbuf: error. pool_hold=" << pool_hold << "but now pool_now=" << pool_now << "\n";
        assert(pool_now == pool_hold);
    }

    /* Determine if the sbuf consists of a repeating ngram. If so,
     * it's only passed to the parsers that want ngrams. (By default,
     * such sbufs are booring.)
     */

    thread_set_status( sbuf.pos0.str() + " finding ngram size (" + std::to_string(sbuf.bufsize) + ")" );
    size_t ngram_size = sbuf.find_ngram_size(sc.max_ngram);

    /****************************************************************
     *** CALL EACH OF THE SCANNERS ON THE SBUF
     ****************************************************************/

    if (debug_flags.debug_dump_data) {
        sbuf.hex_dump(std::cerr);
    }

    /* Make the scanner params once, rather than every time through */
    scanner_params sp(sc, this, scanner_params::PHASE_SCAN, sbufp, scanner_params::PrintOptions(), nullptr);
    for (const auto &it : scanner_info_db) {
        // Look for reasons not to run a scanner
        // this is a lot of find operations - could we make a vector of the enabled scanner_info_dbs?
        const auto &name = it.second->name; // scanner name
        const auto &flags = it.second->scanner_flags; // scanner flags
        if (enabled_scanners.find(it.first) == enabled_scanners.end()) {
            continue; //  not enabled
        }

        if (ngram_size > 0 && flags.scan_ngram_buffer == false) {
            continue;
        }

        if (sbuf.depth() > 0 && flags.depth0_only) {
            // depth >0 and this scanner only run at depth 0
            continue;
        }

        // Don't rescan data that has been seen twice (if scanner doesn't want it.)
        if (seen_before && flags.scan_seen_before == false) {
            continue;
        }

        // If the scanner is a recurse_all, it always calls recurse. We can't it twice in the stack, or else
        // we get infinite regression.
        if (flags.recurse_always && sbuf.pos0.contains( it.second->pathPrefix)) {
            continue;
        }

        // is sbuf large enough?
        if (sbuf.bufsize < it.second->min_sbuf_size) {
            continue;
        }

        bool record_call_stats = false;

        try {

            /* Compute the effective path for stats */
            std::string epath;
            if (record_call_stats) {
                bool inname = false;
                for (auto cc : sbuf.pos0.path) {
                    if (isupper(cc)) inname = true;
                    if (inname) epath.push_back(toupper(cc));
                    if (cc == '-') inname = false;
                }
                if (epath.size() > 0) epath.push_back('-');
                for (auto cc : name) { epath.push_back(toupper(cc)); }
            }

            /* Call the scanner.*/
            aftimer t;
            thread_set_status( sbuf.pos0.str() + ": " + name + " (" + std::to_string(sbuf.bufsize) + ")" );

            if (debug_flags.debug_print_steps) {
                std::cerr << "sbuf.pos0=" << sbuf.pos0 << " calling scanner " << name << "\n";
            }
            if (record_call_stats || debug_flags.debug_print_steps) {
                t.start();
            }
            (*it.first)(sp);

            if (record_call_stats || debug_flags.debug_print_steps) {
                t.stop();
                struct stats st(t.elapsed_nanoseconds(), 1);
                add_scanner_stat(*it.first, st);
                add_path_stat(epath, st);
                if (debug_flags.debug_print_steps) {
                    std::cerr << "sbuf.pos0=" << sbuf.pos0 << " scanner " << name << " t=" << t.elapsed_seconds() << "\n";
                }
            }
        } catch (const std::exception& e) {
            std::stringstream ss;
            ss << "std::exception Scanner: " << name << " Exception: " << e.what() << " sbuf.pos0: " << sbuf.pos0
               << " bufsize=" << sbuf.bufsize << "\n";
            std::cerr << ss.str();
            try {
                fs.get_alert_recorder().write(sbuf.pos0, "scanner=" + name,
                                              std::string("<exception>") + e.what() + "</exception>");
            } catch (feature_recorder_set::NoSuchFeatureRecorder& e2) {}
        } catch (...) {
            std::stringstream ss;
            ss << "std::exception Scanner: " << name << " Unknown Exception "
               << " sbuf.pos0: " << sbuf.pos0 << " bufsize=" << sbuf.bufsize << "\n";
            std::cerr << ss.str();
            try {
                fs.get_alert_recorder().write(sbuf.pos0, "scanner=" + name,
                                              std::string("<unknown_exception></unknown_exception>"));
            } catch (feature_recorder_set::NoSuchFeatureRecorder& e) {}
        }
    }
    timer.stop();
    delete_sbuf(sbufp);
    return;
}

void scanner_set::schedule_sbuf(sbuf_t *sbufp)
{
    assert (sbufp != nullptr );
    //std::cerr << "schedule_sbuf: " << *sbufp << "\n";
    /* Run in same thread? */
    if (pool==nullptr || (sbufp->depth() > 0 && sbufp->bufsize < SAME_THREAD_SBUF_SIZE)) {
        process_sbuf(sbufp);
        return;
    }

    sbufp->scheduled = true;
    update_queue_stats( sbufp, +1 );
    print_tp_stats();

    /* Run in a different thread */
    pool->push_task( [this, sbufp]{ this->process_sbuf(sbufp); } );
}


void scanner_set::delete_sbuf(sbuf_t *sbufp)
{
    //std::cerr << "delete_sbuf: " << *sbufp << "\n";
    if (sbufp->scheduled) {
        update_queue_stats( sbufp, -1 );
    }
    thread_set_status(sbufp->pos0.str() + " delete_sbuf");
    print_tp_stats();
    if (sbufp->depth()==0 && writer) {
        std::stringstream attribute;
        attribute << "t='" << time(0) << "'";
        writer->xmlout("sbuf_delete", sbufp->pos0.str(), attribute.str(), false);
    }
    delete sbufp;
}

/****************************************************************
 *** PHASE_SHUTDOWN methods.
 ****************************************************************/


void scanner_set::log(const std::string message)
{
    std::stringstream attribute;
    attribute << "t='" << time(0) << "'";
    std::cerr  << "log: " << message << " " << attribute.str() << "\n";
    if (writer){
        writer->xmlout("log", message, attribute.str(), false);
        writer->flush();
    }
    else {
        std::cerr << "writer is null\n";
    }
}

void scanner_set::log(const sbuf_t &sbuf, const std::string message) // writes sbuf if not too deep.
{
    if (sbuf.depth() <= log_depth) {
        std::stringstream m2;
        m2 << sbuf.pos0 << " buflen=" << sbuf.bufsize;
        if (sbuf.has_hash()) {
            m2 << " " << sbuf.hash();
        }
        m2 << ": " << message;
        log(m2.str());
    }
}




/**
 * Shutdown:
 * 1 - change phase from PHASE_SCAN to PHASE_SHUTDOWN.
 * 2 - Shut down all of the scanners.
 * 3 - write out the in-memory histograms.
 * 4 - terminate the XML file.
 */
void scanner_set::shutdown() {
    if (current_phase != scanner_params::PHASE_SCAN) {
        throw std::runtime_error("shutdown can only be called in scanner_params::PHASE_SCAN");
    }
    current_phase = scanner_params::PHASE_SHUTDOWN;

    /* Tell the scanners we are shutting down */
    scanner_params::PrintOptions po; // empty po
    scanner_params sp(sc, this, scanner_params::PHASE_SHUTDOWN, nullptr, po, nullptr);
    for (const auto &it : enabled_scanners) { (*it)(sp); }

    fs.feature_recorders_shutdown();

    /* Tell every feature recorder to flush all of its histograms */
    fs.histograms_generate();

    /* Output the scanner stats */
    if (writer) {
        writer->push("scanner_stats");
#if 0
        for (scanner_t *scanner : scanner_stats.keys()) {
            struct scanner_set::stats &st = scanner_stats[scanner];
            writer->set_oneline("true");
            writer->push("scanner");
            writer->xmlout("name", get_scanner_name(scanner));
            writer->xmlout("ns", st.ns);
            writer->xmlout("calls", st.calls);
            writer->pop();
        }
#endif
        writer->pop();
    }
}

/*
 * uses hash to determine if a block was prevously seen.
 * Hopefully sbuf.buf() is zero-copy.
 */
std::string scanner_set::hash(const sbuf_t& sbuf) const
{
    return sbuf.hash(fs.hasher.func);
}

uint64_t scanner_set::previously_processed_count(const sbuf_t& sbuf) {
    std::string hash = sbuf.hash();
    return previously_processed_counter[ hash ]++;
}




/****************************************************************
 *** Feature Recorder
 ****************************************************************/


/**
 * Process a pcap packet.
 * Designed to be very efficient because we have so many packets.
 */
#if 0
void scanner_set::process_packet(const be13::packet_info &pi)
{
    for (packet_plugin_info_vector_t::iterator it = packet_handlers.begin(); it != packet_handlers.end(); it++){
        (*(*it).callback)((*it).user,pi);
    }
}
#endif

/****************************************************************
 *** packet handling
 ****************************************************************/

#if 0
/* Vector of callbacks */
typedef std::vector<packet_plugin_info> packet_plugin_info_vector_t;
//packet_plugin_info_vector_t  packet_handlers;   // pcap callback handlers
/* object for keeping track of packet callbacks */
class packet_plugin_info {
public:
    packet_plugin_info(void *user_,be13::packet_callback_t *callback_):user(user_),callback(callback_){};
    void *user;
    be13::packet_callback_t *callback;
};
void scanner_set::load_scanner_packet_handlers()
{
    for (const auto &it: enabled_scanners){
        const scanner_def *sd = (*it);
            if(sd->info.packet_cb){
                packet_handlers.push_back(packet_plugin_info(sd->info.packet_user,sd->info.packet_cb));
            }
        }
    }
}
#endif
