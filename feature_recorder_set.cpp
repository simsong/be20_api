/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"
#include "bulk_extractor_i.h"

#include "histogram.h"


/****************************************************************
 *** feature_recorder_set
 *** No mutex is needed for the feature_recorder_set because it is never
 *** modified after it is created, only the contained feature_recorders are modified.
 ****************************************************************/

const string feature_recorder_set::ALERT_RECORDER_NAME = "alerts";
const string feature_recorder_set::DISABLED_RECORDER_NAME = "disabled";

/* Create an empty recorder */
feature_recorder_set::feature_recorder_set(uint32_t flags_):flags(flags_),seen_set(),input_fname(),
                                                            outdir(),
                                                            frm(),map_lock(),
                                                            histogram_defs(),
                                                            alert_list(),stop_list(),
                                                            scanner_stats()
{
    if(flags & SET_DISABLED){
        create_name(DISABLED_RECORDER_NAME,false);
        frm[DISABLED_RECORDER_NAME]->set_flag(feature_recorder::FLAG_DISABLED);
    }
}

/**
 * Create a properly functioning feature recorder set.
 * If disabled, create a disabled feature_recorder that can respond to functions as requested.
 */
void feature_recorder_set::init(const feature_file_names_t &feature_files,
                                const std::string &input_fname_,
                                const std::string &outdir_,
                                const histograms_t *histogram_defs_)
{
    input_fname    = input_fname_;
    outdir         = outdir_;
    histogram_defs = histogram_defs_;

    create_name(feature_recorder_set::ALERT_RECORDER_NAME,false); // make the alert recorder

    /* Create the requested feature files */
    for(set<string>::const_iterator it=feature_files.begin();it!=feature_files.end();it++){
        create_name(*it,flags & CREATE_STOP_LIST_RECORDERS);
    }
}

void feature_recorder_set::flush_all()
{
    for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
        i->second->flush();
    } 
}

void feature_recorder_set::close_all()
{
    for(feature_recorder_map::iterator i = frm.begin();i!=frm.end();i++){
        i->second->close();
    } 
}


bool feature_recorder_set::has_name(string name) const
{
    return frm.find(name) != frm.end();
}

/*
 * Gets a feature_recorder_set.
 */
feature_recorder *feature_recorder_set::get_name(const std::string &name) 
{
    const std::string *thename = &name;
    if(flags & SET_DISABLED){           // if feature recorder set is disabled, return the disabled recorder.
        thename = &feature_recorder_set::DISABLED_RECORDER_NAME;
    }

    if(flags & ONLY_ALERT){
        thename = &feature_recorder_set::ALERT_RECORDER_NAME;
    }

    cppmutex::lock lock(map_lock);
    feature_recorder_map::const_iterator it = frm.find(*thename);
    if(it!=frm.end()) return it->second;
    return(0);                          // feature recorder does not exist
}


feature_recorder *feature_recorder_set::create_name_factory(const std::string &outdir_,const std::string &input_fname_,const std::string &name_){
    return new feature_recorder(*this,outdir_,input_fname_,name_);
}


void feature_recorder_set::create_name(const std::string &name,bool create_stop_file) 
{
    if(frm.find(name)!=frm.end()){
        std::cerr << "create_name: feature recorder '" << name << "' already exists\n";
        return;
    }

    feature_recorder *fr = create_name_factory(outdir,input_fname,name);
    feature_recorder *fr_stopped = 0;

    frm[name] = fr;
    if(create_stop_file){
        string name_stopped = name+"_stopped";
        
        fr_stopped = create_name_factory(outdir,input_fname,name_stopped);
        fr->set_stop_list_recorder(fr_stopped);
        frm[name_stopped] = fr_stopped;
    }
    
    if(flags & SET_DISABLED) return;        // don't open if we are disabled
    
    /* Open the output!*/
    fr->open();
    if(fr_stopped) fr_stopped->open();
}

feature_recorder *feature_recorder_set::get_alert_recorder()
{
    return get_name(feature_recorder_set::ALERT_RECORDER_NAME);
}


/*
 * uses md5 to determine if a block was prevously seen.
 */
bool feature_recorder_set::check_previously_processed(const uint8_t *buf,size_t bufsize)
{
    std::string md5 = md5_generator::hash_buf(buf,bufsize).hexdigest();
    return seen_set.check_for_presence_and_insert(md5);
}

void feature_recorder_set::add_stats(string bucket,double seconds)
{
    cppmutex::lock lock(map_lock);
    struct pstats &p = scanner_stats[bucket]; // get the location of the stats
    p.seconds += seconds;
    p.calls ++;
}

void feature_recorder_set::get_stats(void *user,stat_callback_t stat_callback)
{
    for(scanner_stats_map::const_iterator it = scanner_stats.begin();it!=scanner_stats.end();it++){
        (*stat_callback)(user,(*it).first,(*it).second.calls,(*it).second.seconds);
    }
}

void feature_recorder_set::dump_name_count_stats(dfxml_writer &writer)
{
    cppmutex::lock lock(map_lock);
    writer.push("feature_files");
    for(feature_recorder_map::const_iterator ij = frm.begin(); ij != frm.end(); ij++){
        writer.set_oneline(true);
        writer.push("feature_file");
        writer.xmlout("name",ij->second->name);
        writer.xmlout("count",ij->second->count());
        writer.pop();
        writer.set_oneline(false);
    }
}


static const int LINE_LEN = 80;         // keep track of where we are on the line
void feature_recorder_set::dump_histograms(void *user,feature_recorder::dump_callback_t cb,
                                           feature_recorder_set::xml_notifier_t xml_error_notifier)
{
    if(histogram_defs==0){
        return;
    }

    // these are both for formatted printing
    int pos  = 0;                       // for generating \n when printing
    bool need_nl = false;               // formatted

    /* Loop through all of the feature recorders to see if they have in-memory histograms */
    for(feature_recorder_map::const_iterator it = frm.begin(); it!=frm.end(); it++){
        //std::cerr << "checking for memory histogram ... " << it->first << " flag=" << it->second->get_flags() << "\n";
        feature_recorder *fr = it->second;
        if(fr->flag_set(feature_recorder::FLAG_MEM_HISTOGRAM)){
            std::cerr << "***************** " << it->first << " has a memory histogram\n";
            histogram_def d("","","",0);            // empty
            fr->dump_histogram(d,user,cb);
        }
    }
       
    /* Loop through all the histograms */
    for(histograms_t::const_iterator it = histogram_defs->begin();it!=histogram_defs->end();it++){
        const std::string &name = (*it).feature; // feature recorder name
        std::string msg = string(" ") + name + " " + (*it).suffix + "...";
        if(msg.size() + pos > (unsigned) LINE_LEN){
            std::cout << "\n";
            pos = 0;
            need_nl = false;
        }
        std::cout << msg;
        std::cout.flush();
        pos += msg.size();
        need_nl = true;
        if(has_name(name)){
            feature_recorder *fr = get_name(name);
            try {
                //std::cerr << "make_histogram " << name << "\n";
                if(fr->flag_set(feature_recorder::FLAG_MEM_HISTOGRAM)){
                    std::cerr << name << " cannot have both a regular histogram and a memory histogram\n";
                } else {
                    fr->dump_histogram((*it),user,cb);
                }
            }
            catch (const std::exception &e) {
                std::cerr << "ERROR: " ;
                std::cerr.flush();
                std::cerr << e.what() << " computing histogram " << name << "\n";
                if(xml_error_notifier){
                    std::string error = std::string("<error function='phase3' histogram='")
                        + name + std::string("</error>");
                    (*xml_error_notifier)(error);
                }
            }
        }
    }
    if (need_nl) std::cout << "\n";
}

