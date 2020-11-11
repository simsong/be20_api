/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <cstdarg>
#include <regex>

#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "word_and_context_list.h"
#include "unicode_escape.h"
#include "histogram.h"
#include "utils.h"

int64_t feature_recorder::offset_add   = 0;
std::string  feature_recorder::banner_file;
uint32_t feature_recorder::opt_max_context_size=1024*1024;
uint32_t feature_recorder::opt_max_feature_size=1024*1024;
uint32_t feature_recorder::debug = DEBUG_PEDANTIC; // default during development
std::thread::id feature_recorder::main_thread_id = std::this_thread::get_id();
std::string feature_recorder::MAX_DEPTH_REACHED_ERROR_FEATURE {"process_extract: MAX DEPTH REACHED"};
std::string feature_recorder::MAX_DEPTH_REACHED_ERROR_CONTEXT {""};

std::string feature_recorder::CARVE_MODE_DESCRIPTION {"0=carve none; 1=carve encoded; 2=carve all"};

/* Feature recorder functions that don't have anything to do with files  or SQL databases */
