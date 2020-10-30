/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"

//#include "bulk_extractor_i.h"
#include "histogram.h"
#include "feature_recorder_set.h"
#include "dfxml/src/dfxml_writer.h"
#include "dfxml/src/hash_t.h"


/**
 * feature_recorder_set:
 * Manage the set of feature recorders.
 * Handles both file-based feature recorders and the SQLite3 feature recorder.
 */

const std::string feature_recorder_set::ALERT_RECORDER_NAME = "alerts";
const std::string feature_recorder_set::DISABLED_RECORDER_NAME = "disabled";
const std::string feature_recorder_set::NO_INPUT = "<NO-INPUT>";
const std::string feature_recorder_set::NO_OUTDIR = "<NO-OUTDIR>";

/* be_hash. Currently this just returns the MD5 of the sbuf,
 * but eventually it will allow the use of different hashes.
 */
std::string feature_recorder_set::hash_def::md5_hasher(const uint8_t *buf,size_t bufsize)
{
    return dfxml::md5_generator::hash_buf(buf,bufsize).hexdigest();
}

std::string feature_recorder_set::hash_def::sha1_hasher(const uint8_t *buf,size_t bufsize)
{
    return dfxml::sha1_generator::hash_buf(buf,bufsize).hexdigest();
}

std::string feature_recorder_set::hash_def::sha256_hasher(const uint8_t *buf,size_t bufsize)
{
    return dfxml::sha256_generator::hash_buf(buf,bufsize).hexdigest();
}

feature_recorder_set::hash_func_t feature_recorder_set::hash_def::hash_func_for_name(const std::string &name)
{
    if(name=="md5" || name=="MD5"){
        return md5_hasher;
    }
    if(name=="sha1" || name=="SHA1" || name=="sha-1" || name=="SHA-1"){
        return sha1_hasher;
    }
    if(name=="sha256" || name=="SHA256" || name=="sha-256" || name=="SHA-256"){
        return sha256_hasher;
    }
    throw std::invalid_argument("invalid hasher name " + name);
}



/**
 * Constructor.
 * Create an empty recorder with no outdir.
 */
feature_recorder_set::feature_recorder_set(uint32_t flags_,const std::string hash_algorithm,
                                           const std::string &input_fname_,const std::string &outdir_):
    flags(flags_), input_fname(input_fname_), outdir(outdir_),
    hasher( hash_def(hash_algorithm, hash_def::hash_func_for_name(hash_algorithm)))
{
    if (outdir.size() == 0){
        throw std::invalid_argument("output directory not provided");
    }

    /* Make sure we can write to the outdir if one is provided */
    if ((outdir != NO_OUTDIR) && (access(outdir.c_str(),W_OK)!=0)) {
        throw std::invalid_argument("output directory not writable");
    }
    /* Now initialize the scanners */

    /* Create a disabled feature recorder if necessary */
    if (flag_set(SET_DISABLED)){
        create_named_feature_recorder(DISABLED_RECORDER_NAME,false);
        frm[DISABLED_RECORDER_NAME]->set_flag(feature_recorder::FLAG_DISABLED);
    }

    /* Create an alert recorder if necessary */
    if (flag_notset(NO_ALERT)) {
        create_named_feature_recorder(feature_recorder_set::ALERT_RECORDER_NAME,false); // make the alert recorder
    }

    //message_enabled_scanners(scanner_params::PHASE_INIT); // tell all enabled scanners to init

#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    if (flag_set(ENABLE_SQLITE3_RECORDERS)) {
        db_create();
    }
#endif

#if 0
    /* Create the requested feature files */
    for( auto it:feature_files){
        create_name(it, flags & CREATE_STOP_LIST_RECORDERS);
    }
#endif
}


/*
 * deallocator
 */
feature_recorder_set::~feature_recorder_set()
{
    for ( auto it:frm ){
        delete it.second;
    }
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    db_close();
#endif
}



/**
 *
 * Initialize a properly functioning feature recorder set.
 * If disabled, create a disabled feature_recorder that can respond to functions as requested.
 */


void    feature_recorder_set::set_flag(uint32_t f)
{
    if(f & MEM_HISTOGRAM){
        if(flags & MEM_HISTOGRAM){
            std::cerr << "MEM_HISTOGRAM flag cannot be set twice\n";
            assert(0);
        }
        /* Create the in-memory histograms for all of the feature recorders */
        for(feature_recorder_map::const_iterator it = frm.begin(); it!=frm.end(); it++){
            feature_recorder *fr = it->second;
            fr->enable_memory_histograms();
        }
    }
    flags |= f;
}

void    feature_recorder_set::unset_flag(uint32_t f)
{
    if(f & MEM_HISTOGRAM){
        throw std::runtime_error("MEM_HISTOGRAM flag cannot be cleared");
    }
    flags &= ~f;
}



/** Flush all of the feature recorder files.
 *  Typically done at the end of an sbuf.
 */
void feature_recorder_set::flush_all()
{
    for ( auto it:frm){
        it.second->flush();
    }
}

void feature_recorder_set::close_all()
{
    for (auto it:frm){
        it.second->close();
    }
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    if ( flag_set(feature_recorder_set::ENABLE_SQLITE3_RECORDERS )) {
        db_transaction_commit();
    }
#endif
}


/****************************************************************
 *** adding and removing feature recorders
 ****************************************************************/

/*
 * Create a named feature recorder, any associated stoplist recorders, and open the files
 * stop recorders are basically a second feature paired with every feature recorder that records
 * things that were sent to the stop list.
 *
 * previously called create_name()
 */
void feature_recorder_set::create_named_feature_recorder(const std::string &name,bool create_stop_recorder)
{
    if(frm.find(name)!=frm.end()){
        std::cerr << "create_name: feature recorder '" << name << "' already exists\n";
        return;
    }

    frm[name] = new feature_recorder(*this, name);
    if (create_stop_recorder){
        std::string name_stopped = name+"_stopped";
        frm[name_stopped] = new feature_recorder(*this, name_stopped);
        frm[name]->set_stop_list_recorder( frm[name_stopped] );
    }
}


bool feature_recorder_set::has_name(std::string name) const
{
    return frm.find(name) != frm.end();
}

/*
 * Gets a feature_recorder fromn from the set.
 */
feature_recorder *feature_recorder_set::get_name(const std::string &name) const
{
    const std::string *thename = &name;
    if(flags & SET_DISABLED){           // if feature recorder set is disabled, return the disabled recorder.
        thename = &feature_recorder_set::DISABLED_RECORDER_NAME;
    }

    if(flags & ONLY_ALERT){
        thename = &feature_recorder_set::ALERT_RECORDER_NAME;
    }

    std::lock_guard<std::mutex> lock(Mscanner_stats);
    feature_recorder_map::const_iterator it = frm.find(*thename);
    if(it!=frm.end()) return it->second;
    return(0);                          // feature recorder does not exist
}

/*
 * The alert recorder is special.
 */
feature_recorder *feature_recorder_set::get_alert_recorder() const
{
    if (flag_set(NO_ALERT)) return 0;
    return get_name(feature_recorder_set::ALERT_RECORDER_NAME);
}

#if 0
// send every enabled scanner the phase message
void feature_recorder_set::message_enabled_scanners(scanner_params::phase_t phase)
{
    /* make an empty sbuf and feature recorder set */
    scanner_params sp(phase,sbuf_t(), *this); // dummy sp to get phase
    recursion_control_block rcb(0,"");    // dummy rcb
    for(auto const &it : frm){
        if (it.second->enabled){
            (it.second)(sp, rcb);
        }
    }
}
#endif


/****************************************************************
 *** Data handling
 ****************************************************************/


/*
 * uses md5 to determine if a block was prevously seen.
 * Hopefully sbuf.buf() is zero-copy.
 */
bool feature_recorder_set::check_previously_processed(const sbuf_t &sbuf)
{
    std::string md5 = dfxml::md5_generator::hash_buf(sbuf.buf,sbuf.bufsize).hexdigest();
    return seen_set.check_for_presence_and_insert(md5);
}

/****************************************************************
 *** Stats
 ****************************************************************/

void feature_recorder_set::add_stats(const std::string &bucket,double seconds)
{
    std::lock_guard<std::mutex> lock(Mscanner_stats);
    struct pstats &p = scanner_stats[bucket]; // get the location of the stats
    p.seconds += seconds;
    p.calls ++;
}

/*
 * Send the stats to a callback; if the callback returns less than 0, abort.
 */
void feature_recorder_set::get_stats(void *user,stat_callback_t stat_callback) const
{
    for(scanner_stats_map::const_iterator it = scanner_stats.begin();it!=scanner_stats.end();it++){
        if((*stat_callback)(user,(*it).first,(*it).second.calls,(*it).second.seconds)<0){
            break;
        }
    }
}

void feature_recorder_set::dump_name_count_stats(dfxml_writer &writer) const
{
    std::lock_guard<std::mutex> lock(Mscanner_stats);
    writer.push("feature_files");
    for (auto ij: frm) {
        writer.set_oneline(true);
        writer.push("feature_file");
        writer.xmlout("name",ij.second->name);
        writer.xmlout("count",ij.second->count());
        writer.pop();
        writer.set_oneline(false);
    }
}



/****************************************************************
 *** PHASE HISTOGRAM (formerly phase 3): Create the histograms
 ****************************************************************/

/**
 * We now have three kinds of histograms:
 * 1 - Traditional post-processing histograms specified by the histogram library
     1a - feature-file based traditional ones
     1b - SQL-based traditional ones.
 * 2 - In-memory histograms (used primarily by beapi)
 */


void feature_recorder_set::add_histogram(const histogram_def &def)
{
    feature_recorder *fr = get_name(def.feature);
    if(fr) fr->add_histogram(def);
}

void feature_recorder_set::dump_histograms(void *user,feature_recorder::dump_callback_t cb,
                                           feature_recorder_set::xml_notifier_t xml_error_notifier) const
{
    /* Ask each feature recorder to dump its histograms */
    for (feature_recorder_map::const_iterator it = frm.begin(); it!=frm.end(); it++){
        feature_recorder *fr = it->second;
        fr->dump_histograms(user,cb,xml_error_notifier);
    }
}

void feature_recorder_set::get_feature_file_list(std::vector<std::string> &ret)
{
    for(feature_recorder_map::const_iterator it = frm.begin(); it!=frm.end(); it++){
        ret.push_back(it->first);
    }
}


#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)

/*** SQL Support ***/

/*
 * Time results with ubnist1 on R4:
 * no SQL - 79 seconds
 * no pragmas - 651 seconds
 * "PRAGMA synchronous =  OFF", - 146 second
 * "PRAGMA synchronous =  OFF", "PRAGMA journal_mode=MEMORY", - 79 seconds
 *
 * Time with domexusers:
 * no SQL -
 */


#define SQLITE_EXTENSION ".sqlite"
#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

static int debug  = 0;

static const char *schema_db[] = {
    "PRAGMA synchronous =  OFF",
    "PRAGMA journal_mode=MEMORY",
    //"PRAGMA temp_store=MEMORY",  // did not improve performance
    "PRAGMA cache_size = 200000",
    "CREATE TABLE IF NOT EXISTS db_info (schema_ver INTEGER, bulk_extractor_ver INTEGER)",
    "INSERT INTO  db_info (schema_ver, bulk_extractor_ver) VALUES (1,1)",
    "CREATE TABLE IF NOT EXISTS be_features (tablename VARCHAR,comment TEXT)",
    "CREATE TABLE IF NOT EXISTS be_config (name VARCHAR,value VARCHAR)",
    0};

/* Create a feature table and note that it has been created in be_features */
static const char *schema_tbl[] = {
    "CREATE TABLE IF NOT EXISTS f_%s (offset INTEGER(12), path VARCHAR, feature_eutf8 TEXT, feature_utf8 TEXT, context_eutf8 TEXT)",
    "CREATE INDEX IF NOT EXISTS f_%s_idx1 ON f_%s(offset)",
    "CREATE INDEX IF NOT EXISTS f_%s_idx2 ON f_%s(feature_eutf8)",
    "CREATE INDEX IF NOT EXISTS f_%s_idx3 ON f_%s(feature_utf8)",
    "INSERT INTO be_features (tablename,comment) VALUES ('f_%s','')",
    0};

static const char *begin_transaction[] = {"BEGIN TRANSACTION",0};
static const char *commit_transaction[] = {"COMMIT TRANSACTION",0};
void feature_recorder_set::db_send_sql(sqlite3 *db,const char **stmts, ...)
{
    assert(db!=0);
    for(int i=0;stmts[i];i++){
        char *errmsg = 0;
        char buf[65536];

        va_list ap;
        va_start(ap,stmts);
        vsnprintf(buf,sizeof(buf),stmts[i],ap);
        va_end(ap);
        if(debug) std::cerr << "SQL: " << buf << "\n";
        // Don't error on a PRAGMA
        if((sqlite3_exec(db,buf,NULL,NULL,&errmsg) != SQLITE_OK)  && (strncmp(buf,"PRAGMA",6)!=0)) {
            fprintf(stderr,"Error executing '%s' : %s\n",buf,errmsg);
            exit(1);
        }
    }
}

void feature_recorder_set::db_create_table(const std::string &name)
{
    std::cerr << "db_create_table\n";
    assert(name.size()>0);
    assert(db3!=NULL);
    db_send_sql(db3,schema_tbl,name.c_str(),name.c_str());
}

sqlite3 *feature_recorder_set::db_create_empty(const std::string &name)
{
    assert(name.size()>0);
    std::string dbfname  = outdir + "/" + name +  SQLITE_EXTENSION;
    if(debug) std::cerr << "create_feature_database " << dbfname << "\n";
    sqlite3 *db=0;
    if (sqlite3_open_v2(dbfname.c_str(), &db,
                        SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,
                        0)!=SQLITE_OK) {
        std::cerr << "Cannot create database '" << dbfname << "': " << sqlite3_errmsg(db) << "\n";
        sqlite3_close(db);
        exit(1);
    }
    return db;
}

void feature_recorder_set::db_create()
{
    assert(db3==0);
    db3 = db_create_empty("report");
    db_send_sql(db3,schema_db);
    std::cout << "in db_create called\n";
}

void feature_recorder_set::db_close()
{
    if(db3){
        if(debug) std::cerr << "db_close()\n";
        sqlite3_close(db3);
        db3 = 0;
    }
}

void feature_recorder_set::db_transaction_begin()
{
    std::lock_guard<std::mutex> lock(Min_transaction);
    if(!in_transaction){
        db_send_sql(db3,begin_transaction);
        in_transaction = true;
    }
}

void feature_recorder_set::db_transaction_commit()
{
    std::lock_guard<std::mutex> lock(Min_transaction);
    if(in_transaction){
        db_send_sql(db3,commit_transaction);
        in_transaction = false;
    } else {
        std::cerr << "No transaction to commit\n";
    }
}

#endif
