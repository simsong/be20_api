#include "config.h"

#include "scanner_params.h"
#include "scanner_set.h"
#include "feature_recorder_set.h"
#include "feature_recorder.h"

/**
 * register_info() lets a scanner register its scanner info with the scanner_set.
 */
void scanner_params::register_info(const scanner_info *si)
{
    ss.register_info(si);
}


/* This interface creates if we are in init phase, doesn't if we are in scan phase */
feature_recorder &scanner_params::named_feature_recorder(const std::string feature_recorder_name) const
{
    return ss.named_feature_recorder(feature_recorder_name);
}


const std::string scanner_params::get_input_fname() const
{
    return ss.get_input_fname();
}
