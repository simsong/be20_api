#include "scanner_params.h"
#include "scanner_set.h"
#include "feature_recorder_set.h"
#include "feature_recorder.h"

/* This interface creates if we are in init phase, doesn't if we are in scan phase */
feature_recorder &scanner_params::named_feature_recorder(const std::string feature_recorder_name) const
{
    return ss.named_feature_recorder(feature_recorder_name);
}


const std::filesystem::path scanner_params::get_input_fname() const
{
    return ss.get_input_fname();
}

void scanner_params::recurse(sbuf_t *new_sbuf) const
{
    /* TODO - call other recursive functions depending on mode */
    ss.process_sbuf(new_sbuf);
    /* At this point, sbuf is deleted */
}
