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

