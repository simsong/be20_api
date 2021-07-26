#include "scanner_params.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
#include "scanner_set.h"

scanner_params::scanner_params(struct scanner_config &sc_, class scanner_set  *ss_, phase_t phase_,
                               const sbuf_t* sbuf_, PrintOptions print_options_, std::stringstream* xmladd)
    : sc(sc_), ss(ss_), phase(phase_), sbuf(sbuf_), print_options(print_options_), sxml(xmladd)
{
    //std::cerr << "scanner_params: make1 this=" << this << "\n";
    //ss->info();
}

scanner_params::scanner_params(const scanner_params& sp_existing, const sbuf_t* sbuf_)
    : sc(sp_existing.sc), ss(sp_existing.ss),
      phase(sp_existing.phase), sbuf(sbuf_),
      print_options(sp_existing.print_options),
      depth(sp_existing.depth + 1), sxml(sp_existing.sxml)
{
    //std::cerr << "scanner_params: make2 this=" << this << "\n";
}


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

bool scanner_params::check_previously_processed(const sbuf_t &s) const
{
    return ss->previously_processed_count(s)==0;
}

void scanner_params::recurse(sbuf_t* new_sbuf) const {
    std::cerr << "scanner_params::recurse ss=" << (void *)&ss << "\n";
    assert(ss!=nullptr);
    ss->schedule_sbuf(new_sbuf);
    /* sbuf will be deleted after it is processed */
}
