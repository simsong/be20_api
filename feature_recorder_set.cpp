/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"
#include "bulk_extractor_i.h"
#include "histogram.h"

/****************************************************************
 *** feature_recorder_set:
 *** Manage the set of feature recorders.
 *** Handles both file-based feature recorders and the SQLite3 feature recorder.
 ****************************************************************/

const std::string feature_recorder_set::ALERT_RECORDER_NAME = "alerts";
const std::string feature_recorder_set::DISABLED_RECORDER_NAME = "disabled";
const std::string feature_recorder_set::NO_INPUT = "<NO-INPUT>";
const std::string feature_recorder_set::NO_OUTDIR = "<NO-OUTDIR>";

std::string feature_recorder_set::null_hasher_name("null");
std::string feature_recorder_set::null_hasher_func(const uint8_t *buf,size_t bufsize)
{
    return std::string("0000000000000000");
}

feature_recorder_set::hash_def feature_recorder_set::null_hasher(null_hasher_name,null_hasher_func);

/* be_hash. Currently this just returns the MD5 of the sbuf,
 * but eventually it will allow the use of different hashes.
 */
static std::string be_hash_name("md5");
static std::string be_hash_func(const uint8_t *buf,size_t bufsize)
{
    if(be_hash_name=="md5" || be_hash_name=="MD5"){
        return dfxml::md5_generator::hash_buf(buf,bufsize).hexdigest();
    }
    if(be_hash_name=="sha1" || be_hash_name=="SHA1" || be_hash_name=="sha-1" || be_hash_name=="SHA-1"){
        return dfxml::sha1_generator::hash_buf(buf,bufsize).hexdigest();
    }
    if(be_hash_name=="sha256" || be_hash_name=="SHA256" || be_hash_name=="sha-256" || be_hash_name=="SHA-256"){
        return dfxml::sha256_generator::hash_buf(buf,bufsize).hexdigest();
    }
    std::cerr << "Invalid hash name: " << be_hash_name << "\n";
    std::cerr << "This version of bulk_extractor only supports MD5, SHA1, and SHA256\n";
    exit(1);
}
static feature_recorder_set::hash_def the_be_hasher(be_hash_name, be_hash_func);

/* Create an empty recorder with no outdir. */
feature_recorder_set::feature_recorder_set(uint32_t flags_,const std::string hash_algorithm,
                                           const std::string &input_fname_,const std::string &outdir_):
    flags(flags_),seen_set(),input_fname(input_fname_),
    outdir(outdir_),
    frm(),Mscanner_stats(),
    histogram_defs(),
    Min_transaction(),in_transaction(),db3(),
    alert_list(),stop_list(),
    scanner_stats(),
    hasher( the_be_hasher )
{
    assert(outdir.size() > 0);
    if(flags & SET_DISABLED){
        create_name(DISABLED_RECORDER_NAME,false);
        frm[DISABLED_RECORDER_NAME]->set_flag(feature_recorder::FLAG_DISABLED);
    }
}

/**
 * 
 * Initialize a properly functioning feature recorder set.
 * If disabled, create a disabled feature_recorder that can respond to functions as requested.
 */

#if 0
WTF? Why do I even *have* an init? Is it to create the feature files? THey could get initted as they are added, and this shoudl be done in the creator.

                - CXheck in the test program.
                - chekc in the API program
                - Check in Phase1
                - Delete init()
#endif
void feature_recorder_set::init(const feature_file_names_t &feature_files)
{
    std::cerr << "feature_recorder_set::init\n";

    /* Make sure we can write to the outdir if one is provided */
    if ((outdir != NO_OUTDIR) && (access(outdir.c_str(),W_OK)!=0)) {
        throw new std::invalid_argument("output directory not writable");
    }
        
    if (flag_set(ENABLE_SQLITE3_RECORDERS)) {
        std::cerr << "calling db_create\n";
        db_create();
        std::cerr << "called db_create\n";
    }

    if (flag_notset(NO_ALERT)) {
        create_name(feature_recorder_set::ALERT_RECORDER_NAME,false); // make the alert recorder
    }

    /* Create the requested feature files */
    for(std::set<std::string>::const_iterator it=feature_files.begin();it!=feature_files.end();it++){
        create_name(*it,flags & CREATE_STOP_LIST_RECORDERS);
    }
}

/** Flush all of the feature recorder files.
 * Typically done at the end of an sbuf.
 */
void feature_recorder_set::flush_all()
{
    for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
        i->second->flush();
    } 
}

void feature_recorder_set::close_all()
{
    for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
        i->second->close();
    } 
    if ( flag_set(feature_recorder_set::ENABLE_SQLITE3_RECORDERS )) {
        db_transaction_commit();
    }
}


bool feature_recorder_set::has_name(std::string name) const
{
    return frm.find(name) != frm.end();
}

/*
 * Gets a feature_recorder_set.
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


feature_recorder *feature_recorder_set::create_name_factory(const std::string &name_)
{
    return new feature_recorder(*this,name_);
}


/*
 * Create a named feature recorder, any associated stoplist recorders, and open the files
 */
void feature_recorder_set::create_name(const std::string &name,bool create_stop_recorder) 
{
    if(frm.find(name)!=frm.end()){
        std::cerr << "create_name: feature recorder '" << name << "' already exists\n";
        return;
    }

    feature_recorder *fr = create_name_factory(name);

    frm[name] = fr;
    if (create_stop_recorder){
        std::string name_stopped = name+"_stopped";
        
        feature_recorder *fr_stopped = create_name_factory(name_stopped);
        fr->set_stop_list_recorder(fr_stopped);
        frm[name_stopped] = fr_stopped;
    }
}

feature_recorder *feature_recorder_set::get_alert_recorder() const
{
    if (flag_set(NO_ALERT)) return 0;

    return get_name(feature_recorder_set::ALERT_RECORDER_NAME);
}


/*
 * uses md5 to determine if a block was prevously seen.
 */
bool feature_recorder_set::check_previously_processed(const uint8_t *buf,size_t bufsize)
{
    std::string md5 = dfxml::md5_generator::hash_buf(buf,bufsize).hexdigest();
    return seen_set.check_for_presence_and_insert(md5);
}

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
        std::cerr << "MEM_HISTOGRAM flag cannot be cleared\n";
        assert(0);
    }
    flags &= ~f;
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
    for(feature_recorder_map::const_iterator it = frm.begin(); it!=frm.end(); it++){
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


/*** SQL Support ***/
#ifdef BEAPI_SQLITE3

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
void feature_recorder_set::db_send_sql(BEAPI_SQLITE3 *db,const char **stmts, ...)
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
    std::cerr << "db_create_called\n";
    assert(name.size()>0);
    assert(db3!=NULL);
    db_send_sql(db3,schema_tbl,name.c_str(),name.c_str());
}

BEAPI_SQLITE3 *feature_recorder_set::db_create_empty(const std::string &name)
{
    assert(name.size()>0);
    std::string dbfname  = outdir + "/" + name +  SQLITE_EXTENSION;
    if(debug) std::cerr << "create_feature_database " << dbfname << "\n";
    BEAPI_SQLITE3 *db=0;
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
