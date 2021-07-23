#include "scanner_params.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "mt_scanner_set.h"

/* This interface creates if we are in init phase, doesn't if we are in scan phase */
feature_recorder& scanner_params::named_feature_recorder(const std::string feature_recorder_name) const
{
    assert(ss!=nullptr);
    return ss->named_feature_recorder(feature_recorder_name);
}

const std::filesystem::path scanner_params::get_input_fname() const
{
    assert(ss!=nullptr);
    return ss->get_input_fname();

}

void scanner_params::recurse(sbuf_t* new_sbuf) const {
    std::cerr << "scanner_params::recurse ss=" << (void *)&ss << "\n";
    assert(ss!=nullptr);
    ss->schedule_sbuf(new_sbuf);
    /* sbuf will be deleted after it is processed */
}
