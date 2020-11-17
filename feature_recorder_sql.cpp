/*
 * Feature recorder mods for writing features into an SQLite3 database.
 */

/* http://blog.quibb.org/2010/08/fast-bulk-inserts-into-sqlite/ */

#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "bulk_extractor_i.h"
#include "histogram.h"
#include "sbuf.h"

feature_recorder_sql::feature_recorder_sql(class feature_recorder_set &fs_, const std::string &name_):
    fs(fs_),name(name_)
{
    /*
     * If the feature recorder set is disabled, just return.
     */
    if (fs.flag_set(feature_recorder_set::SET_DISABLED)) return;
    /* write to a database? Create tables if necessary and create a prepared statement */

    char buf[1024];
    fs.db_create_table(name);
    snprintf( buf, sizeof(buf), db_insert_stmt,name.c_str() );
    bs = new besql_stmt( fs.db3, buf );
}
