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

#ifdef HAVE_LIBSQLITE3

static const char *schema_db[] = {
    /* These optimizations are unsafe and don't seem to improve performance significantly */
    //"PRAGMA synchronous =  OFF",
    //"PRAGMA journal_mode=MEMORY",
    //"PRAGMA temp_store=MEMORY",
    "PRAGMA cache_size = 200000",        // 10x normal cache
    "CREATE TABLE db_info (schema_ver INTEGER, bulk_extractor_ver INTEGER)",
    "INSERT INTO  db_info (schema_ver, bulk_extractor_ver) VALUES (1,1)",
    "CREATE TABLE features (tablename VARCHAR,comment TEXT)",
    0};

static const char *schema_tbl[] = {
    "CREATE TABLE f_%s (offset INTEGER(12), path VARCHAR, feature TEXT, context TEXT)",
    "CREATE INDEX f_%s_idx ON f_%s(offset);",
    "CREATE INDEX f_%s_vdx ON f_%s(feature);",
    "INSERT INTO features (tablename,comment) VALUES ('f_%s','')",
    0};

void feature_recorder_set::send_sql(const char **stmts,const char *arg1,const char *arg2)
{
    for(int i=0;stmts[i];i++){
        char *errmsg = 0;
        char buf[65536];
        snprintf(buf,sizeof(buf),stmts[i],arg1,arg2);
        if(sqlite3_exec(db3,buf,NULL,NULL,&errmsg) != SQLITE_OK ) {
            fprintf(stderr,"Error executing '%s' : %s\n",buf,errmsg);
            exit(1);
        }
    }
}

void feature_recorder_set::create_feature_table(const std::string &name)
{
    send_sql(schema_tbl,name.c_str(),name.c_str());
}

void feature_recorder_set::create_feature_database()
{
    assert(db3==0);
    std::cerr << "create_feature_database\n";
    std::string dbfname  = outdir + "/report.sqlite3";
    if (sqlite3_open(dbfname.c_str(), &db3)) {
        fprintf(stderr,"Cannot create database: %s\n",sqlite3_errmsg(db3));
        sqlite3_close(db3);
        exit(1);
    }
    send_sql(schema_db,"","");
}

BEAPI_SQLITE3_STMT *feature_recorder_set::prepare_insert_statement(const char *featurename)
{
    sqlite3_stmt *stmt = 0;
    const char *cmd = "INSERT INTO f_%s VALUES (?1, ?2, ?3, ?4)";
    char buf[1024];
    snprintf(buf,sizeof(buf),cmd,featurename);
    sqlite3_prepare_v2(db3,buf, strlen(buf), &stmt, NULL);
    return stmt;
}

void feature_recorder_set::insert_feature(sqlite3_stmt *stmt,const pos0_t &pos,const std::string &feature,const std::string &context)
{
    const char *ps = pos.str().c_str();
    const char *fs = feature.c_str();
    const char *cs = context.c_str();
    sqlite3_bind_int(stmt, 1, pos.offset);
    sqlite3_bind_text(stmt, 2, ps, strlen(ps), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, fs, strlen(fs), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, cs, strlen(cs), SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr,"Commit Failed\n");
    }
    sqlite3_reset(stmt);
}
#else
/* sqlite3 is typedef'ed to void if the .h is not available */

void feature_recorder_set::create_feature_table(const std::string &name) { }
void feature_recorder_set::create_feature_database() {  }
#endif

#ifdef STAND
int main(int argc,char **argv)
{
    const char *dbfile = "test.sql3";
    char *errmsg = 0;
    sqlite3 *db=0;

    unlink(dbfile);
    db = create_feature_database(dbfile);

    /* Create an email table */
    create_feature_table(db,"email");

    /* Lets throw a million features into the table as a test */
    sqlite3_exec(db,"BEGIN TRANSACTION",NULL,NULL,&errmsg);
    sqlite3_stmt *stmt = prepare_insert_statement(db,"email");
    for(int i=0;i<1000000;i++){
        pos0_t p;
        pos0_t p1 = p+i;

        if(i%10000==0) printf("i=%d\n",i);

        char feature[64];
        snprintf(feature,sizeof(feature),"user%d@company.com",i);
        char context[64];
        snprintf(context,sizeof(context),"this is the context user%d@company.com yes it is!",i);
        insert_statement(stmt,p1,feature,context);
    }
    sqlite3_exec(db,"COMMIT TRANSACTION",NULL,NULL,&errmsg);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}
#endif

