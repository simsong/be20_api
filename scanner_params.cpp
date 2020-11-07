#include "config.h"

#include "scanner_params.h"
#include "scanner_set.h"
#include "feature_recorder_set.h"
#include "feature_recorder.h"

void scanner_params::register_info(const scanner_info &si)
{
    ss.register_info(&si);
}


feature_recorder & scanner_params::get_name(const std::string &feature_recorder_name)
{
    feature_recorder *fr = ss.get_feature_recorder_by_name(feature_recorder_name);
    if (fr) {
        return *fr;
    }
    throw std::invalid_argument("Invalid feature recorder name: "+feature_recorder_name);
}

const std::string &scanner_params::get_input_fname() const
{
    return ss.get_input_fname();
}
