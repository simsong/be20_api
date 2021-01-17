/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * common.cpp:
 * bulk_extractor backend stuff, used for both standalone executable and bulk_extractor.
 */

#include "config.h"
#include <cassert>

#ifdef HAVE_ERR_H
#include <err.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include "scanner_config.h"
#include "scanner_set.h"
#include "dfxml/src/hash_t.h"
#include "dfxml/src/dfxml_writer.h"
#include "aftimer.h"


/****************************************************************
 *** SCANNER SET IMPLEMENTATION (previously the PLUG-IN SYSTEM)
 ****************************************************************/

/* BE2 revision:
 * In BE1, the active scanners were maintained by the plugin system's global state.
 * In BE2, there is no global states. Instead, scanners are grouped into scanner set, which
 *         in turn has a feature_recorder_set, which in turn has multiple feature recorders.
 *         The scanner set can then be asked to process a sbuf.
 *         All of the global variables go away.
 */


/* object for keeping track of packet callbacks */
class packet_plugin_info {
public:
    packet_plugin_info(void *user_,be13::packet_callback_t *callback_):user(user_),callback(callback_){};
    void *user;
    be13::packet_callback_t *callback;
};

/* Vector of callbacks */
typedef std::vector<packet_plugin_info> packet_plugin_info_vector_t;
packet_plugin_info_vector_t  packet_handlers;   // pcap callback handlers

std::string scanner_set::ALL_SCANNERS {"all"};


/****************************************************************
 * create the scanner set
 */
scanner_set::scanner_set(const scanner_config &sc_,
                         const feature_recorder_set::flags_t &f,
                         class dfxml_writer *writer_):
    sc(sc_),fs(f,sc_.hash_alg, sc_.input_fname, sc_.outdir), writer(writer_)
{
}


/****************************************************************
 ** PHASE_INIT:
 ** Add scanners to the scanner set.
 ****************************************************************/


/* Callback for a scanner to register its info with the scanner set. */
void scanner_set::register_info(const scanner_params::scanner_info *si)
{
    if (scanner_info_db.find( si->scanner) != scanner_info_db.end()){
        throw std::runtime_error("A scanner tried to register itself that is already registered");
    }
    scanner_info_db[si->scanner] = si;

    /* Create all of the requested feature recorders.
     * Multiple scanners may request the same feature recorder without generating an error.
     */
    for (auto it: si->feature_names ){
        fs.named_feature_recorder( it, true );
    }

    /* Create all of the requested histograms
     * Multiple scanners may request the same histograms without generating an error.
     */
    for (auto it: si->histogram_defs ){
        fs.named_feature_recorder( it.feature, true ).histogram_add( it );
    }
}


/****************************************************************
 *
 * Add the scanner.
 */

void scanner_set::add_scanner(scanner_t scanner)
{
    /* If scanner is already loaded, that's an error */
    if (scanner_info_db.find(scanner) != scanner_info_db.end()) {
        throw std::runtime_error("scanner already added");
    }

    /* Initialize the scanner.
     * Use an empty sbuf and an empty feature recorder to create an empty scanner params that is in PHASE_STARTUP.
     * We then ask the scanner to initialize.
     */
    const sbuf_t sbuf;
    scanner_params::PrintOptions po;
    scanner_params sp(*this, scanner_params::PHASE_INIT, sbuf, po);

    // Send the scanner the PHASE_INIT message, which will cause it to call
    // register_info above, which will add the scanner's scanner_info to the database.
    (*scanner)(sp);

    if (scanner_info_db.find(scanner) == scanner_info_db.end()){
        throw std::runtime_error("a scanner did not register itself");
    }

    // Enable the scanner if it is not disabled by default.
    if ( scanner_info_db[scanner]->scanner_flags.default_enabled ) {
        enabled_scanners.insert(scanner);
    }
}


/* add_scanners allows a calling program to add a null-terminated array of scanners. */
void scanner_set::add_scanners(scanner_t * const *scanners)
{
    for( int i=0 ; scanners[i] ; i++){
        add_scanner(scanners[i]);
    }
}

/* Add a scanner from a file (it's in the file as a shared library) */
#if 0
void scanner_set::add_scanner_file(std::string fn, const scanner_config &c)
{    /* Figure out the function name */
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
}
#endif

#if 0
/* Add all of the scanners in a directory */
void scanner_set::add_scanner_directory(const std::string &dirname,const scanner_info::scanner_config &sc )
{
    DIR *dirp = opendir(dirname.c_str());
    if(dirp==0){
        fprintf(stderr,"Cannot open directory %s:",dirname.c_str());
        exit(1);
    }
    struct dirent *dp;
    while ((dp = readdir(dirp)) != NULL){
        std::string fname = dp->d_name;
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
}
#endif

#if 0
void scanner_set::load_scanner_packet_handlers()
{
    for (auto it: enabled_scanners){
        const scanner_def *sd = (*it);
            if(sd->info.packet_cb){
                packet_handlers.push_back(packet_plugin_info(sd->info.packet_user,sd->info.packet_cb));
            }
        }
    }
}
#endif


/**
 * return true a scanner is enabled
 */

/* enable or disable a specific scanner.
 * enable = 0  - disable that scanner.
 * enable = 1  - enable that scanner
 * 'all' is a special scanner that enables all scanners.
 */

void scanner_set::set_scanner_enabled(const std::string &name,bool enable)
{
    /* If name is 'all' and the NO_ALL flag is not set for that scanner,
     * then either enable it or disable it as appropriate */
    if (name == scanner_set::ALL_SCANNERS){
        for (auto it: scanner_info_db) {
            if (it.second->scanner_flags.no_all) {
                continue;
            }
            if (enable) {
                enabled_scanners.insert( it.first );
            } else {
                enabled_scanners.erase( it.first );
            }
        }
        return;
    }
    scanner_t *scanner = get_scanner_by_name(name);
    if (!scanner) {
        /* Scanner wasn't found */
        throw std::invalid_argument("Invalid scanner name: " + name);
    }
    if (enable) {
        enabled_scanners.insert(scanner );
    } else {
        enabled_scanners.erase( scanner );
    }
}


bool scanner_set::is_scanner_enabled(const std::string &name)
{
    scanner_t *scanner = get_scanner_by_name(name);
    return scanner && enabled_scanners.find(scanner) != enabled_scanners.end();
}

// put the enabled scanners into the vector
void scanner_set::get_enabled_scanners(std::vector<std::string> &svector)
{
    for (auto it: enabled_scanners) {
        svector.push_back( scanner_info_db[it]->name );
    }
}

// Return true if any of the enabled scanners are a FIND scanner
bool scanner_set::is_find_scanner_enabled()
{
    for (auto it: enabled_scanners) {
        if (scanner_info_db[it]->scanner_flags.find_scanner){
            return true;
        }
    }
    return false;
}


/****************************************************************
 *** scanner plugin loading
 ****************************************************************/
const std::string scanner_set::get_scanner_name(scanner_t scanner) const
{
    auto it = scanner_info_db.find(scanner);
    if (it != scanner_info_db.end()){
        return it->second->name;
    }
    return "";
}

scanner_t &scanner_set::get_scanner_by_name(const std::string &search_name)
{
    for (auto it: scanner_info_db) {
        if ( it.second->name == search_name) {
            return *it.first;
        }
    }
    throw NoSuchScanner(search_name);
}

/* This interface creates if we are in init phase, doesn't if we are in scan phase */
feature_recorder &scanner_set::named_feature_recorder(const std::string &name)
{
    return fs.named_feature_recorder(name, current_phase==scanner_params::PHASE_INIT);
}

/**
 * Print a list of scanners.
 * We need to load them to do this, so they are loaded with empty config
 * Note that scanners can only be loaded once, so this exits.
 */
void scanner_set::info_scanners(std::ostream &out,
                                bool detailed_info,
                                bool detailed_settings,
                                const char enable_opt,const char disable_opt)
{
    std::vector<std::string> enabled_scanner_names, disabled_scanner_names;
    for (auto it: scanner_info_db) {
        if (detailed_info){
            if (it.second->name.size()) out << "Scanner Name: " << it.second->name;
            if (is_scanner_enabled(it.second->name)) {
                out << " (ENABLED) ";
            }
            out << "\n";
            out << "flags:  " << it.second->scanner_flags.asString() << "\n";
            if (it.second->author.size()) out << "Author: " << it.second->author << "\n";
            if (it.second->description.size()) out << "Description: " << it.second->description << "\n";
            if (it.second->url.size()) out << "URL: " << it.second->url << "\n";
            if (it.second->scanner_version.size()) out << "Scanner Version: " << it.second->scanner_version << "\n";
            out << "Feature Names: ";
            for ( auto i2: it.second->feature_names ) {
                out << i2 << " ";
            }
            if (detailed_settings){
                out << "Settable Options (and their defaults): \n";
                out << it.second->helpstr();
            }
            out << "\n\n";
        }
        if (it.second->scanner_flags.no_usage) continue;
        if (is_scanner_enabled(it.second->name)){
            enabled_scanner_names.push_back(it.second->name);
        } else {
            disabled_scanner_names.push_back(it.second->name);
        }
    }
    out << "\n";
    out << "These scanners disabled by default; enable with -" << enable_opt << ":\n";

    sort( disabled_scanner_names.begin(), disabled_scanner_names.end());
    sort( enabled_scanner_names.begin(), enabled_scanner_names.end());

    for ( auto it:disabled_scanner_names ){
        out << "   -" << enable_opt << " " <<  it << " - enable scanner " << it << "\n";
    }
    out << "\n";
    out << "These scanners enabled by default; disable with -" << disable_opt << ":\n";
    for ( auto it:disabled_scanner_names ){
        out << "   -" << disable_opt << " " <<  it << " - disable scanner " << it << "\n";
    }
}



#if 0
/****************************************************************
 *** Scanner Commands (which one is enabled or disabled)
 ****************************************************************/

void scanner_set::set_scanner_enabled_all( bool shouldEnable )
{

    assert(scanner_commands_processed==false);
    scanner_commands.push_back(scanner_command(scanner_command::DISABLE_ALL,std::string("")));
}

void scanner_set::scanners_enable_all()
{
    assert(scanner_commands_processed==false);
    scanner_commands.push_back(scanner_command(scanner_command::ENABLE_ALL,std::string("")));
}

void scanner_set::scanners_enable(const std::string &name)
{
    assert(scanner_commands_processed==false);
    scanner_commands.push_back(scanner_command(scanner_command::ENABLE,name));
}

void scanner_set::scanners_disable(const std::string &name)
{
    assert(scanner_commands_processed==false);
    scanner_commands.push_back(scanner_command(scanner_command::DISABLE,name));
}

void scanner_set::scanners_process_enable_disable_commands()
{
    for(std::vector<scanner_command>::const_iterator it=scanner_commands.begin();
        it!=scanner_commands.end();it++){
        switch((*it).command){
        case scanner_command::ENABLE_ALL:  set_scanner_enabled_all(true);break;
        case scanner_command::DISABLE_ALL: set_scanner_enabled_all(false); break;
        case scanner_command::ENABLE:      set_scanner_enabled((*it).name,true);break;
        case scanner_command::DISABLE:     set_scanner_enabled((*it).name,false);break;
        }
    }
    load_scanner_packet_handlers();     // can't do until enable/disable commands are run
    scanner_commands_processed = true;
}
#endif


const std::string & scanner_set::get_input_fname() const
{
    return sc.input_fname;
}


/****************************************************************
 *** PHASE_SCAN methods.
 ****************************************************************/


/* Transition to the scanning phase */
void scanner_set::phase_scan()
{
    if (current_phase != scanner_params::PHASE_INIT){
        throw std::runtime_error("start_scan can only be run in scanner_params::PHASE_INIT");
    }
    current_phase = scanner_params::PHASE_SCAN;
}



/****************************************************************
 *** PHASE_SHUTDOWN methods.
 ****************************************************************/

void scanner_set::shutdown()
{
    if (current_phase != scanner_params::PHASE_SCAN){
        throw std::runtime_error("shutdown can only be called in scanner_params::PHASE_SCAN");
    }
    current_phase = scanner_params::PHASE_SHUTDOWN;

    /* Tell the scanners we are shutting down */
    const sbuf_t sbuf;               // empty sbuf
    scanner_params::PrintOptions po; // empty po
    scanner_params sp(*this, scanner_params::PHASE_SHUTDOWN, sbuf, po);
    for ( auto it: enabled_scanners ){
        (*it)(sp);
    }

    /* Tell every feature recorder to flush all of its histograms */
    fs.histograms_generate();

    /* Output the scanner stats */
    if (writer) {
        writer->push("scanner_stats");
        for( auto it: scanner_stats ){
            writer->set_oneline("true");
            writer->push("scanner");
            writer->xmlout("name", get_scanner_name(it.first));
            writer->xmlout("ns", it.second->ns);
            writer->xmlout("calls", it.second->calls);
            writer->pop();
        }
        writer->pop();
    }
}

// https://stackoverflow.com/questions/16190078/how-to-atomically-update-a-maximum-value
template<typename T>
void update_maximum(std::atomic<T>& maximum_value, T const& value) noexcept
{
    T prev_value = maximum_value;
    while(prev_value < value &&
          !maximum_value.compare_exchange_weak(prev_value, value)){
    }
}


/* Process an sbuf! */
void scanner_set::process_sbuf(const class sbuf_t &sbuf)
{
    /* If we  have not transitioned to PHASE::SCAN, error */
    if (current_phase != scanner_params::PHASE_SCAN){
        throw std::runtime_error("process_sbuf can only be run in scanner_params::PHASE_SCAN");
    }

    /**
     * upperstr - Turns an ASCII string into upper case (should be UTF-8)
     */

    const pos0_t &pos0 = sbuf.pos0;

    /* If we are too deep, error out */
    if (sbuf.depth() >= max_depth ) {
        fs.get_alert_recorder().write(pos0,
                                      feature_recorder::MAX_DEPTH_REACHED_ERROR_FEATURE,
                                      feature_recorder::MAX_DEPTH_REACHED_ERROR_CONTEXT );
        return;
    }

    update_maximum<unsigned int>(max_depth_seen, sbuf.depth() );

    /* Determine if we have seen this buffer before */
    bool seen_before = fs.check_previously_processed(sbuf);
    if (seen_before) {
        dfxml::sha1_t sha1 = dfxml::sha1_generator::hash_buf(sbuf.buf, sbuf.bufsize);
        std::stringstream ss;
        ss << "<buflen>" << sbuf.bufsize  << "</buflen>";
        if(dup_data_alerts) {
            fs.get_alert_recorder().write(sbuf.pos0,"DUP SBUF "+sha1.hexdigest(),ss.str());
        }
        dup_bytes_encountered += sbuf.bufsize;
    }

    /* Determine if the sbuf consists of a repeating ngram. If so,
     * it's only passed to the parsers that want ngrams. (By default,
     * such sbufs are booring.)
     */

    size_t ngram_size = sbuf.find_ngram_size( max_ngram );

    /****************************************************************
     *** CALL EACH OF THE SCANNERS ON THE SBUF
     ****************************************************************/

#if 0
    if (debug & DEBUG_DUMP_DATA) {
        sp.sbuf.hex_dump(std::cerr);
    }
#endif

    for (auto it: scanner_info_db) {
        // Look for reasons not to run a scanner
        if (enabled_scanners.find(it.first) == enabled_scanners.end()){
            continue;                       //  not enabled
        }

        if ( ngram_size>0  && it.second->scanner_flags.scan_ngram_buffer==false ){
            continue;
        }

        if ( sbuf.depth() > 0 && it.second->scanner_flags.depth_0) {
            // depth >0 and this scanner only run at depth 0
            continue;
        }

        if ( seen_before && it.second->scanner_flags.scan_seen_before==false ){
            continue;
        }

        const std::string &name = it.second->name;

        try {

            /* Compute the effective path for stats */
            bool inname=false;
            std::string epath;
            for( auto cc: sbuf.pos0.path){
                if (isupper(cc)) inname=true;
                if (inname) epath.push_back(toupper(cc));
                if (cc=='-') inname=false;
            }
            if (epath.size()>0) epath.push_back('-');
            for (auto cc:name){
                epath.push_back(toupper(cc));
            }

            /* Create a RCB that will recursively call process_sbuf() */
            //recursion_control_block rcb(process_sbuf, upperstr(name));

            /* Call the scanner.*/
            {
                aftimer t;
#if 0
                if (debug & DEBUG_PRINT_STEPS){
                    std::cerr << "sbuf.pos0=" << sbuf.pos0 << " calling scanner " << name << "\n";
                }
#endif
                t.start();
                scanner_params sp(*this, scanner_params::PHASE_SCAN, sbuf, scanner_params::PrintOptions());
                (*it.first)( sp );
                t.stop();
#if 0
                if (debug & DEBUG_PRINT_STEPS){
                    std::cerr << "sbuf.pos0=" << sbuf.pos0 << " scanner "
                              << name << " t=" << t.elapsed_seconds() << "\n";
                }
#endif
                // fs.add_stats(epath,t.elapsed_seconds());
            }

        }
        catch (const std::exception &e ) {
            std::stringstream ss;
            ss << "std::exception Scanner: " << name
               << " Exception: " << e.what()
               << " sbuf.pos0: " << sbuf.pos0 << " bufsize=" << sbuf.bufsize << "\n";
            std::cerr << ss.str();
            try {
                fs.get_alert_recorder().write(sbuf.pos0,"scanner="+name,
                                              std::string("<exception>")+e.what()+"</exception>");
            }
            catch (feature_recorder_set::NoSuchFeatureRecorder &e2){
            }
        }
        catch (...) {
            std::stringstream ss;
            ss << "std::exception Scanner: " << name
               << " Unknown Exception "
               << " sbuf.pos0: " << sbuf.pos0 << " bufsize=" << sbuf.bufsize << "\n";
            std::cerr << ss.str();
            try {
                fs.get_alert_recorder().write(sbuf.pos0,"scanner="+name,
                                              std::string("<unknown_exception></unknown_exception>"));
            }
            catch (feature_recorder_set::NoSuchFeatureRecorder &e){
            }
        }
    }
}


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


#if 0
void scanner_set::get_scanner_feature_file_names(feature_file_names_t &feature_file_names)
{
    for (auto it: scanner_info_db) {
        if (enabled_scanners.find(it.first) != enabled_scanners.end()){
            for( auto it2:second->feature_names) {
                feature_file_names.insert(it2);
            }
        }
    }
}
#endif
