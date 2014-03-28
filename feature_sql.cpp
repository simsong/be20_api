/*
 * Feature recorder that can write to an SQLite3 database
 */

/* http://blog.quibb.org/2010/08/fast-bulk-inserts-into-sqlite/ */

#include "sqlite3.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

static const char *stmts[] = {
    "PRAGMA synchronous =  OFF;",
    "PRAGMA cache_size = 20000;",        // 10x normal cache
    "PRAGMA journal_mode=MEMORY",
    "PRAGMA temp_store=MEMORY",
    "CREATE TABLE db_info (schema_ver INTEGER, bulk_extractor_ver INTEGER);",
    "INSERT INTO db_info (schema_ver, bulk_extractor_ver) VALUES (1,1);",
    "CREATE TABLE features (offset INTEGER(12), path VARCHAR, feature TEXT, context TEXT)",
    "CREATE INDEX features_idx ON features(offset);",
    "CREATE INDEX features_vdx ON features(feature);",
    0};

int main(int argc,char **argv)
{
    const char *dbfile = "test.sql3";
    char *errmsg = 0;
    sqlite3 *db=0;

    if (sqlite3_open(dbfile, &db)) {
        fprintf(stderr,"Cannot open database: %s\n",sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    for(int i=0;stmts[i];i++){
        if(sqlite3_exec(db,stmts[i],NULL,NULL,&errmsg) != SQLITE_OK ) {
            fprintf(stderr,"Error executing '%s' : %s\n",stmts[i],errmsg);
            exit(1);
        }
    }
    /* Lets throw a million features into the table as a test */
    sqlite3_exec(db,"BEGIN TRANSACTION",NULL,NULL,&errmsg);
    const char *cmd = "INSERT INTO features VALUES (?1, ?2, ?3, ?4)";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,cmd, strlen(cmd), &stmt, NULL);
    for(int i=0;i<1000000;i++){
        char buf[64];
        snprintf(buf,sizeof(buf),"%d",i);
        char feature[64];
        snprintf(feature,sizeof(feature),"user%d@company.com",i);
        char context[64];
        snprintf(context,sizeof(context),"this is the context user%d@company.com yes it is!",i);
        sqlite3_bind_int(stmt, 1, i);
        sqlite3_bind_text(stmt, 2, buf, strlen(buf), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, feature, strlen(feature), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, context, strlen(context), SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr,"Commit Failed\n");
        }
        sqlite3_reset(stmt);
        if(i%1000==0) printf("i=%d\n",i);
    }
    sqlite3_exec(db,"COMMIT TRANSACTION",NULL,NULL,&errmsg);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}
