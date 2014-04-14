/*
 * Feature recorder mods for writing features into an SQLite3 database.
 */

/* http://blog.quibb.org/2010/08/fast-bulk-inserts-into-sqlite/ */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sbuf.h>

#include "bulk_extractor_i.h"
#include "histogram.h"

#ifdef HAVE_LIBSQLITE3

static const char *schema_db[] = {
    /* These optimizations are unsafe and don't seem to improve performance significantly */
    //"PRAGMA synchronous =  OFF",
    //"PRAGMA journal_mode=MEMORY",
    //"PRAGMA temp_store=MEMORY",
    "PRAGMA cache_size = 200000",        // 10x normal cache
    "CREATE TABLE db_info (schema_ver INTEGER, bulk_extractor_ver INTEGER)",
    "INSERT INTO  db_info (schema_ver, bulk_extractor_ver) VALUES (1,1)",
    "CREATE TABLE be_features (tablename VARCHAR,comment TEXT)",
    "CREATE TABLE be_config (name VARCHAR,value VARCHAR)",
    0};

static const char *schema_tbl[] = {
    "CREATE TABLE f_%s (offset INTEGER(12), path VARCHAR, feature_eutf8 TEXT, feature_utf8 TEXT, context_eutf8 TEXT)",
    "CREATE INDEX f_%s_idx1 ON f_%s(offset)",
    "CREATE INDEX f_%s_idx2 ON f_%s(feature_eutf8)",
    "CREATE INDEX f_%s_idx3 ON f_%s(feature_utf8)",
    "INSERT INTO be_features (tablename,comment) VALUES ('f_%s','')",
    0};

/* This creates the base histogram */
static const char *schema_hist[] = {
    "CREATE TABLE h_%s (count INTEGER(12), feature_utf8 TEXT)",
    "CREATE INDEX h_%s_idx1 ON h_%s(count)",
    "CREATE INDEX h_%s_idx2 ON h_%s(feature_utf8)",
    0};

/* This performs the histogram operation */
static const char *schema_hist1[] = {
    "INSERT INTO h_%s select count(*),feature_utf8 from f_%s group by feature_utf8",
    0};

static const char *insert_stmt = "INSERT INTO f_%s VALUES (?1, ?2, ?3, ?4, ?5)";

class beapi_sql_stmt {
    BEAPI_SQLITE3_STMT *stmt;                 // prepared statement
public:
    beapi_sql_stmt(sqlite3 *db3,const std::string &featurename){
#ifdef HAVE_SQLITE3_H
        /* prepare the statement */
        char buf[1024];
        snprintf(buf,sizeof(buf),insert_stmt,featurename.c_str());
        sqlite3_prepare_v2(db3,buf, strlen(buf), &stmt, NULL);
#endif
    }
    ~beapi_sql_stmt(){
#ifdef HAVE_SQLITE3_H
        if(stmt){
            sqlite3_finalize(stmt);
            stmt = 0;
        }
#endif
    }
    void insert_feature(const pos0_t &pos,const std::string &feature,const std::string &feature8, const std::string &context)
    {
#ifdef HAVE_SQLITE3_H
        const char *ps = pos.str().c_str();
        const char *fs = feature.c_str();
        const char *f8 = feature8.c_str();
        const char *cs = context.c_str();
        sqlite3_bind_int(stmt,  1, pos.offset);
        sqlite3_bind_text(stmt, 2, ps, strlen(ps), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, fs, strlen(fs), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, f8, strlen(f8), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, cs, strlen(cs), SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr,"sqlite3_step failed\n");
        }
        sqlite3_reset(stmt);
#endif
    }
};

void feature_recorder_set::db_send_sql(const char **stmts,const std::string &arg1,const std::string &arg2)
{
    for(int i=0;stmts[i];i++){
        char *errmsg = 0;
        char buf[65536];
        snprintf(buf,sizeof(buf),stmts[i],arg1.c_str(),arg2.c_str());
        if(sqlite3_exec(db3,buf,NULL,NULL,&errmsg) != SQLITE_OK ) {
            fprintf(stderr,"Error executing '%s' : %s\n",buf,errmsg);
            exit(1);
        }
    }
}

void feature_recorder_set::db_create_table(const std::string &name)
{
    db_send_sql(schema_tbl,name,name);
}

void feature_recorder_set::db_create()
{
    assert(db3==0);
    std::string dbfname  = outdir + "/report.sqlite3";
    std::cerr << "create_feature_database " << dbfname << "\n";
    if (sqlite3_open_v2(dbfname.c_str(), &db3,
                        SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,
                        0)!=SQLITE_OK) {
        std::cerr << "Cannot create database '" << dbfname << "': " << sqlite3_errmsg(db3) << "\n";
        sqlite3_close(db3);
        exit(1);
    }
    db_send_sql(schema_db,"","");
}

void feature_recorder_set::db_close()
{
    if(db3){
        sqlite3_close(db3);
        db3 = 0;
    }
}

void feature_recorder_set::db_transaction_begin()
{
    cppmutex::lock lock(Min_transaction);
    if(!in_transaction){
        char *errmsg = 0;
        if(sqlite3_exec(db3,"BEGIN TRANSACTION",NULL,NULL,&errmsg)==SQLITE_OK){
            in_transaction = true;
        } else {
            fprintf(stderr,"BEGIN TRANSACTION Error: %s\n",errmsg);
        }
    }
}

void feature_recorder_set::db_commit()
{
    cppmutex::lock lock(Min_transaction);
    if(in_transaction){
        char *errmsg = 0;
        if(sqlite3_exec(db3,"COMMIT TRANSACTION",NULL,NULL,&errmsg)==SQLITE_OK){
            in_transaction = false;
        } else {
            fprintf(stderr,"COMMIT TRANSACTION Error: %s\n",errmsg);
        }
    }
}

/* Hook for writing feature to SQLite3 database */
void feature_recorder::write0_db(const pos0_t &pos0,const std::string &feature,const std::string &context)
{
    /**
     * Note: this is not very efficient, passing through a quoted feature and then unquoting it.
     * We could make this more efficient.
     */
    std::string *feature8 = HistogramMaker::convert_utf16_to_utf8(feature_recorder::unquote_string(feature));
    cppmutex::lock lock(Mstmt);
    if(stmt==0){
        stmt = new beapi_sql_stmt(fs.db3,name);
    }
    stmt->insert_feature(pos0,feature,feature8 ? *feature8 : feature,context);
    if (feature8) delete feature8;
}

/* Hook for writing histogram
 */
int callback_counter(void *param, int argc, char **argv, char **azColName)
{
    printf("CALLBACK\n");
    int *counter = reinterpret_cast<int *>(param);
    (*counter)++;
    return 0;
}

void feature_recorder::dump_histogram_db(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const
{
    /* First check to see if there exists a feature histogram summary. If not, make it */
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='h_" + def.feature +"'";
    char *errmsg=0;
    int rowcount=0;
    std::cerr << "QUERY: " << query << "\n";
    if (sqlite3_exec(fs.db3,query.c_str(),callback_counter,&rowcount,&errmsg)){
        std::cerr << "sqlite3: " << errmsg << "\n";
        return;
    }
    if (rowcount==0){
        fs.db_send_sql(schema_hist, def.feature, def.feature); // creates the histogram
        fs.db_send_sql(schema_hist1, def.feature, def.feature); // creates the histogram
    }
    /* Now create the summarized histogram for the regex, if it is not existing. */
    if (def.pattern.size()>0){
        /* go through all of the features and build a new table */
    }
}



#else
/* sqlite3 is typedef'ed to void if the .h is not available */
void feature_recorder_set::db_create_table(const std::string &name) {}
void feature_recorder_set::db_create() {}
void feature_recorder_set::db_close() {}
void feature_recorder_set::db_begin_transaction(){}
void feature_recorder_set::db_commit(){}
void feature_recorder::write0_db(){}
#endif

#ifdef STAND
static std::string hash_name("md5");
static std::string hash_func(const uint8_t *buf,size_t bufsize)
{
    if(hash_name=="md5" || hash_name=="MD5"){
        return md5_generator::hash_buf(buf,bufsize).hexdigest();
    }
    if(hash_name=="sha1" || hash_name=="SHA1" || hash_name=="sha-1" || hash_name=="SHA-1"){
        return sha1_generator::hash_buf(buf,bufsize).hexdigest();
    }
    if(hash_name=="sha256" || hash_name=="SHA256" || hash_name=="sha-256" || hash_name=="SHA-256"){
        return sha256_generator::hash_buf(buf,bufsize).hexdigest();
    }
    std::cerr << "Invalid hash name: " << hash_name << "\n";
    std::cerr << "This version of bulk_extractor only supports MD5, SHA1, and SHA256\n";
    exit(1);
}
static feature_recorder_set::hash_def my_hasher(hash_name,hash_func);

feature_recorder_set::feature_recorder_set(uint32_t flags_,const feature_recorder_set::hash_def &hasher_):
    flags(flags_),seen_set(),input_fname(),
    outdir(),
    frm(),
    histogram_defs(),
    db3(),
    alert_list(),stop_list(),
    scanner_stats(),hasher(hasher_)
{
}

feature_recorder *feature_recorder_set::create_name_factory(const std::string &name_){return 0;}
void feature_recorder_set::create_name(const std::string &name,bool create_stop_also){}
bool feature_recorder_set::check_previously_processed(const uint8_t *buf,size_t bufsize){return 0;}
feature_recorder *feature_recorder_set::get_name(const std::string &name) const{return 0;}
feature_recorder *feature_recorder_set::get_alert_recorder() const{return 0;}
void feature_recorder_set::get_feature_file_list(std::vector<std::string> &ret){}

int main(int argc,char **argv)
{
    const char *dbfile = "test.sql3";
    char *errmsg = 0;
    sqlite3 *db=0;

    feature_recorder_set fs(0,my_hasher);

    unlink(dbfile);
    fs.db_create();
    if(1){
        /* Create an email table */
        fs.db_create_table("email");
        
        /* Lets throw a million features into the table as a test */
        //sqlite3_exec(db,"BEGIN TRANSACTION",NULL,NULL,&errmsg);
        beapi_sql_stmt s(db,"email");
        for(int i=0;i<1000000;i++){
            pos0_t p;
            pos0_t p1 = p+i;
            
            if(i%10000==0) printf("i=%d\n",i);
            
            char feature[64];
            snprintf(feature,sizeof(feature),"user%d@company.com",i);
            char context[64];
            snprintf(context,sizeof(context),"this is the context user%d@company.com yes it is!",i);
            //insert_statement(stmt,p1,feature,context);
        }
        //sqlite3_exec(db,"COMMIT TRANSACTION",NULL,NULL,&errmsg);
    }
    fs.db_close();
}
#endif

