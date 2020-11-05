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
#include "aftimer.h"


/****************************************************************
 *** SCANNER SET IMPLEMENTATION (previously the PLUG-IN SYSTEM)
 ****************************************************************/

/* BE2 revision:
 * In BE1, the active scanners were maintained by the plugin system.
 * In BE2, there is no plugin system. Instead, scanners are grouped into scanner set, which
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
scanner_set::scanner_set(const scanner_config &sc_, std::ostream *sxml_):
    sc(sc_),fs(0,"md5",sc.input_fname,sc.outdir), sxml(sxml_)
{
}

/****************************************************************
 *
 * Add the scanner.
 */

void scanner_set::register_info(const scanner_params::scanner_info *si)
{
    std::cerr << "in register_info\n";
    scanner_info_db[si->scanner] = si;
}

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
    //recursion_control_block rcb(0,"");

    //std::cerr << "register_info: " << register_info << "\n";
    // Initialize the scanner, which adds it to the database
    (*scanner)(sp);

    if (scanner_info_db.find(scanner) == scanner_info_db.end()){
        throw std::runtime_error("a scanner did not register itself");
    }

    // Enable the scanner if it is not disabled by default.
    if (!(scanner_info_db[scanner]->flags & scanner_params::scanner_info::SCANNER_DEFAULT_DISABLED)){
        enabled_scanners.insert(scanner);
    }
}

#if 0
void scanner_set::load_scanner_file(std::string fn, const scanner_config &c)
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


void scanner_set::add_scanners(scanner_t * const *scanners)
{
    for( int i=0 ; scanners[i] ; i++){
        add_scanner(scanners[i]);
    }
}

#if 0
void scanner_set::load_scanner_directory(const std::string &dirname,const scanner_info::scanner_config &sc )
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

void scanner_set::load_scanner_directories(const std::vector<std::string> &dirnames,
                                            const scanner_info::scanner_config &sc)
{
    for(std::vector<std::string>::const_iterator it = dirnames.begin();it!=dirnames.end();it++){
        load_scanner_directory(*it,sc);
    }
}

void scanner_set::load_scanner_packet_handlers()
{
    for(scanner_vector::const_iterator it = all_scanners.begin(); it!=all_scanners.end(); it++){
        if((*it)->enabled){
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
            if (it.second->flags & scanner_params::scanner_info::SCANNER_NO_ALL) {
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
    scanner_t *scanner = find_scanner_by_name(name);
    if (!scanner) {
        /* Scanner wasn't found */
        throw std::invalid_argument("Invalid scanner name" + name);
    }
    if (enable) {
        enabled_scanners.insert(scanner );
    } else {
        enabled_scanners.erase( scanner );
    }
}


/** Name of feature files that should be histogramed.
 * The histogram should be done in the plug-in
 */

/****************************************************************
 *** scanner plugin loading
 ****************************************************************/
scanner_t *scanner_set::find_scanner_by_name(const std::string &search_name)
{
    for (auto it: scanner_info_db) {
        if ( it.second->name == search_name) {
            return it.first;
        }
    }
    return nullptr;
}

bool scanner_set::is_scanner_enabled(const std::string &name)
{
    scanner_t *scanner = find_scanner_by_name(name);
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
        if (scanner_info_db[it]->flags & scanner_params::scanner_info::SCANNER_FIND_SCANNER){
            return true;
        }
    }
    return false;
}


void scanner_set::add_enabled_scanner_histograms()
{
    for (auto it: scanner_info_db ){
        for (auto hi: it.second->histogram_defs ){
            fs.add_histogram( hi );
        }
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


/****************************************************************
 *** PHASE_SCAN methods.
 ****************************************************************/



/****************************************************************
 *** PHASE_SHUTDOWN methods.
 ****************************************************************/

void scanner_set::shutdown()
{
    assert(current_phase != scanner_params::PHASE_SHUTDOWN);
    current_phase = scanner_params::PHASE_SHUTDOWN;
    const sbuf_t sbuf;              // empty sbuf
    scanner_params::PrintOptions po; // empty po
    scanner_params sp(*this, scanner_params::PHASE_SHUTDOWN, sbuf, po);
    //recursion_control_block rcb(0,"");        // empty rcb
    for ( auto it: enabled_scanners ){
        (*it)(sp);
    }
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
            out << "flags:  " << scanner_params::scanner_info::flag_to_string(it.second->flags) << "\n";
            out << "Scanner Interface version: " << it.second->si_version << "\n";
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
        if (it.second->flags & scanner_params::scanner_info::SCANNER_NO_USAGE) continue;
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


/* Determine if the sbuf consists of a repeating ngram */
size_t scanner_set::find_ngram_size(const sbuf_t &sbuf) const
{
    for (size_t ngram_size = 1; ngram_size < max_ngram; ngram_size++){
	bool ngram_match = true;
	for (size_t i=ngram_size;i<sbuf.pagesize && ngram_match;i++){
	    if (sbuf[i%ngram_size]!=sbuf[i]) ngram_match = false;
	}
	if (ngram_match) return ngram_size;
    }
    return 0;                           // no ngram size
}

/** process_sbuf is the main workhorse. It is calls each scanner on the sbuf,
 * And the scanners can recursively call the scanner set.
 * @param sp    - the scanner params, including the sbuf to process
 * We now track depth through pos0_t of sbuf_t.
 */

#if 0
static std::string upperstr(const std::string &str)
{
    std::string ret;
    for(std::string::const_iterator i=str.begin();i!=str.end();i++){
        ret.push_back(toupper(*i));
    }
    return ret;
}
#endif


void scanner_set::set_max_depth_seen(uint32_t max_depth_seen_)
{
    std::lock_guard<std::mutex> lock(max_depth_seenM);
    max_depth_seen = max_depth_seen_;
}

uint32_t scanner_set::get_max_depth_seen() const
{
    std::lock_guard<std::mutex> lock(max_depth_seenM);
    return max_depth_seen;
}


void scanner_set::process_sbuf(const class sbuf_t &sbuf)
{
    /**
     * upperstr - Turns an ASCII string into upper case (should be UTF-8)
     */

    const pos0_t &pos0 = sbuf.pos0;

    if (sbuf.depth() > get_max_depth_seen()) {
        set_max_depth_seen( sbuf.depth() );
    }

    /* If we are too deep, error out */
    if (sbuf.depth() >= max_depth ) {
        feature_recorder *fr = fs.get_alert_recorder();
        if(fr) fr->write(pos0,
                         feature_recorder::MAX_DEPTH_REACHED_ERROR_FEATURE,
                         feature_recorder::MAX_DEPTH_REACHED_ERROR_CONTEXT );
        return;
    }

    /* Determine if we have seen this buffer before */
    bool seen_before = fs.check_previously_processed(sbuf);
    if (seen_before) {
        dfxml::md5_t md5 = dfxml::md5_generator::hash_buf(sbuf.buf, sbuf.bufsize);
        feature_recorder *alert_recorder = fs.get_alert_recorder();
        std::stringstream ss;
        ss << "<buflen>" << sbuf.bufsize  << "</buflen>";
        if(alert_recorder && dup_data_alerts) {
            alert_recorder->write(sbuf.pos0,"DUP SBUF "+md5.hexdigest(),ss.str());
        }
#ifdef HAVE__SYNC_ADD_AND_FETCH
        // TODO - replace with std::atomic<
        __sync_add_and_fetch(&dup_data_encountered,sp.sbuf.bufsize);
#endif
    }

    /* Determine if the sbuf consists of a repeating ngram. If so,
     * it's only passed to the parsers that want ngrams. (By default,
     * such sbufs are booring.)
     */

    size_t ngram_size = find_ngram_size( sbuf );

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

        if ( (it.second->flags & scanner_params::scanner_info::SCANNER_WANTS_NGRAMS)==0){
            /* If the scanner does not want ngrams, don't run it if we have ngrams or duplicate data */
            if (ngram_size > 0) continue;
            if (seen_before)    continue;
        }

        if ( sbuf.depth() > 0 && (it.second->flags & scanner_params::scanner_info::SCANNER_DEPTH_0)){
            // depth >0 and this scanner only run at depth 0
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
                fs.add_stats(epath,t.elapsed_seconds());
            }

        }
        catch (const std::exception &e ) {
            std::stringstream ss;
            ss << "std::exception Scanner: " << name
               << " Exception: " << e.what()
               << " sbuf.pos0: " << sbuf.pos0 << " bufsize=" << sbuf.bufsize << "\n";
            std::cerr << ss.str();
            feature_recorder *alert_recorder = fs.get_alert_recorder();
            if(alert_recorder) alert_recorder->write(sbuf.pos0,"scanner="+name,
                                                     std::string("<exception>")+e.what()+"</exception>");
        }
        catch (...) {
            std::stringstream ss;
            ss << "std::exception Scanner: " << name
               << " Unknown Exception "
               << " sbuf.pos0: " << sbuf.pos0 << " bufsize=" << sbuf.bufsize << "\n";
            std::cerr << ss.str();
            feature_recorder *alert_recorder = fs.get_alert_recorder();
            if(alert_recorder) alert_recorder->write(sbuf.pos0,"scanner="+name,"<unknown_exception/>");
        }
    }
    fs.flush_all();
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
