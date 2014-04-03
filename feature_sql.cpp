/*
 * Feature recorder mods for writing features into an SQLite3 database.
 */

/* http://blog.quibb.org/2010/08/fast-bulk-inserts-into-sqlite/ */

#include "../../config.h"

#ifdef HAVE_LIBSQLITE3
#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "unistd.h"
#include "sbuf.h"

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

void send_sql(sqlite3 *db,const char **stmts,const char *arg1,const char *arg2)
{
    for(int i=0;stmts[i];i++){
        char *errmsg = 0;
        char buf[65536];
        snprintf(buf,sizeof(buf),stmts[i],arg1,arg2);
        if(sqlite3_exec(db,buf,NULL,NULL,&errmsg) != SQLITE_OK ) {
            fprintf(stderr,"Error executing '%s' : %s\n",buf,errmsg);
            exit(1);
        }
    }
}

sqlite3 *create_feature_database(const char *dbfile)
{
    sqlite3 *db = 0;
    if (sqlite3_open(dbfile, &db)) {
        fprintf(stderr,"Cannot open database: %s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    send_sql(db,schema_db,"","");
    return db;
}

void create_feature_table(sqlite3 *db,const char *featurename)
{
    send_sql(db,schema_tbl,featurename,featurename);
}

sqlite3_stmt *prepare_insert_statement(sqlite3 *db,const char *featurename)
{
    sqlite3_stmt *stmt = 0;
    const char *cmd = "INSERT INTO f_%s VALUES (?1, ?2, ?3, ?4)";
    char buf[1024];
    snprintf(buf,sizeof(buf),cmd,featurename);
    sqlite3_prepare_v2(db,buf, strlen(buf), &stmt, NULL);
    return stmt;
}

void insert_statement(sqlite3_stmt *stmt,const pos0_t &pos,const std::string &feature,const std::string &context)
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
