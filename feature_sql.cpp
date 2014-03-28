/*
 * Feature recorder that can write to an SQLite3 database
 */

/* PRAGMAs Taken from auto_db.cpp in SleuthKit */

#include "sqlite3.h"
#include "stdio.h"
#include "stdlib.h"

static const char *stmts[] = {
    "PRAGMA synchronous =  OFF;",
    "PRAGMA cache_size = 20000;",        // 10x normal cache
    "CREATE TABLE db_info (schema_ver INTEGER, bulk_extractor_ver INTEGER);",
    "INSERT INTO db_info (schema_ver, bulk_extractor_ver) VALUES (1,1);",
    "CREATE TABLE features (offset INTEGER(12), path VARCHAR, feature TEXT, context TEXT)",
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
}
