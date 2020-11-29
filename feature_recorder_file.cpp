/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <cstdarg>
#include <regex>

#include "feature_recorder_file.h"
#include "feature_recorder_set.h"
#include "word_and_context_list.h"
#include "unicode_escape.h"
#include "utils.h"

#ifndef MAXPATHLEN
#define MAXPATHLEN 65536
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef DEBUG_PEDANTIC
#define DEBUG_PEDANTIC    0x0001// check values more rigorously
#endif


/**
 * Create a feature recorder object. Each recorder records a certain
 * kind of feature.  Features are stored in a file. The filename is
 * permutated based on the total number of threads and the current
 * thread that's recording. Each thread records to a different file,
 * and thus a different feature recorder, to avoid locking
 * problems.
 *
 * @param feature_recorder_set &fs - common information for all of the feature recorders
 * @param name         - the name of the feature being recorded.
 */



/*
 * constructor. Not it is called with the feature_recorder_set to which it belongs.
 *
 */
//TODO - make it register itself with the feature recorder set. and do the stuff that's in init.
feature_recorder_file::feature_recorder_file(class feature_recorder_set &fs_, const std::string &name_):
    feature_recorder(fs_,name_)
{
    /* If the feature recorder set is disabled, just return. */
    if ( fs.flags.disabled ) return;

    /* If there is no output directory, just return */
    //if ( fs.get_outdir() == scanner_config::NO_OUTDIR ) return;

    //if ( fs.flag_set(feature_recorder_set::DISABLE_FILE_RECORDERS)) return;

    /* Open the file recorder for output.
     * If the file exists, seek to the end and find the last complete line, and start there.
     */
    const std::lock_guard<std::mutex> lock(Mios);
    fname = fname_in_outdir("",NO_COUNT);
    ios.open( fname.c_str(), std::ios_base::in|std::ios_base::out|std::ios_base::ate);
    if ( ios.is_open()){                  // opened existing file
        ios.seekg(0L,std::ios_base::end);
        while ( ios.is_open() ) {
            /* Get current position.
             * If we are at the beginning, just return.
             */
            if (int(ios.tellg())==0 ) {
                ios.seekp(0L,std::ios_base::beg);
                return;
            }
            /* backup one character and see if the next character is a newline */
            ios.seekg(-1,std::ios_base::cur); // backup to once less than the end of the file
            if (ios.peek()=='\n'){           // we are finally on the \n
                ios.seekg(1L,std::ios_base::cur); // move the getting one forward
                ios.seekp(ios.tellg(),std::ios_base::beg); // put the putter at the getter location
                //count_ = 1;                            // greater than zero
                return;
            }
        }
    }
    /* Just open the stream for output */
    ios.open(fname.c_str(),std::ios_base::out);
    if (!ios.is_open()){
        std::cerr << "*** feature_recorder_file::open CANNOT OPEN FEATURE FILE FOR WRITING "
                  << fname << ":" << strerror(errno) << "\n";
        throw std::invalid_argument("cannot open feature file for writing");
    }
}

/* Exiting: make sure that the stream is closed.
 */
feature_recorder_file::~feature_recorder_file()
{
    if (ios.is_open()){
        ios.close();
    }
}

void feature_recorder_file::banner_stamp(std::ostream &os,const std::string &header) const
{
    int banner_lines = 0;
    if (fs.banner_filename.size()>0){
        std::ifstream i(fs.banner_filename.c_str());
        if (i.is_open()){
            std::string line;
            while (getline(i,line)){
                if (line.size()>0 && ((*line.end()=='\r') || (*line.end()=='\n'))){
                    line.erase(line.end()); /* remove the last character while it is a \n or \r */
                }
                os << "# " << line << "\n";
                banner_lines++;
            }
            i.close();
        }
    }
    if (banner_lines==0){
        os << "# BANNER FILE NOT PROVIDED (-b option)\n";
    }

    os << bulk_extractor_version_header;
    os << "# Feature-Recorder: " << name << "\n";

    if (fs.get_input_fname().size()) os << "# Filename: " << fs.get_input_fname() << "\n";
#if 0
    if (feature_recorder::debug!=0){
        os << "# DEBUG: " << debug << " (";
        if (feature_recorder::debug & DEBUG_PEDANTIC) os << " DEBUG_PEDANTIC ";
        os << ")\n";
    }
#endif
    os << header;
}



/* statics */
const std::string feature_recorder_file::feature_file_header("# Feature-File-Version: 1.1\n");
const std::string feature_recorder_file::histogram_file_header("# Histogram-File-Version: 1.1\n");
const std::string feature_recorder_file::bulk_extractor_version_header("# " PACKAGE_NAME "-Version: " PACKAGE_VERSION " ($Rev: 10844 $)\n");


#if 0
// add a memory histogram; assume the position in the mhistograms is stable
void feature_recorder::enable_memory_histograms()
{
    for( auto it: histogram_defs){
        mhistograms[it] = new mhistogram_t();
    }
}
#endif

#if 0
/**
 *  Create a histogram for this feature recorder and an extraction pattern.
 */

/* dump_callback_test is a simple callback that just prints to stderr. It's for testing */
int feature_recorder::dump_callback_test(void *user,const feature_recorder &fr,
                                          const std::string &str,const uint64_t &count)
{
    (void)user;
    std::cerr << "dump_cb: user=" << user << " " << str << ": " << count << "\n";
    return 0;
}

/* Make a histogram. If a callback is provided, send the output there. */
class mhistogram_callback {
    mhistogram_callback(const mhistogram_callback&);
    mhistogram_callback &operator=(const mhistogram_callback &);
public:
    mhistogram_callback(void *user_,
                        feature_recorder::dump_callback_t *cb_,
                        const histogram_def &def_,
                        const feature_recorder &fr_,
                        uint64_t limit_):user(user_),cb(cb_),def(def_),fr(fr_),callback_count(0),limit(limit_){}
    void *user;
    feature_recorder::dump_callback_t *cb;
    const histogram_def &def;
    const feature_recorder &fr;
    uint64_t callback_count;
    uint64_t limit;
    int do_callback(const std::string &str,const uint64_t &tally){
        (*cb)(user,fr,def,str,tally);
        if(limit && ++callback_count >= limit) return -1;
        return 0;
    }
    static int callback(void *ptr,const std::string &str,const uint64_t &tally) {
        return ((mhistogram_callback *)(ptr))->do_callback(str,tally);
    }
};
#endif

/**
 * We now have three kinds of histograms:
 * 1 - Traditional post-processing histograms specified by the histogram library
 *   1a - feature-file based traditional ones
 *   1b - SQL-based traditional ones.
 * 2 - In-memory histograms (used primarily by beapi)
 */


#if 0
size_t feature_recorder::count_histograms() const
{
    return histogram_defs.size();
}
#endif


/****************************************************************
 *** WRITING SUPPORT
 ****************************************************************/

/* write to the file.
 * this is the only place where writing happens.
 * so it's an easy place to do utf-8 validation in debug mode.
 */
void feature_recorder_file::write0(const std::string &str)
{
    if (fs.flags.pedantic && (utf8::find_invalid(str.begin(),str.end()) != str.end())) {
        std::cerr << "******************************************\n";
        std::cerr << "feature recorder: " << name << "\n";
        std::cerr << "invalid utf-8 in write: " << str << "\n";
        throw std::runtime_error("Invalid utf-8 in write");
    }

    /* this is where the writing happens. lock the output and write */
    if (fs.flags.disabled) {
        return;
    }

    const std::lock_guard<std::mutex> lock(Mios);
    if(ios.is_open()){
        /* If there is no banner, add it */
        if (ios.tellg()==0){
            banner_stamp(ios, feature_file_header);
        }

        /* Output the feature */
        ios << str << '\n';
        if (ios.fail()){
            throw std::runtime_error("Disk full. Free up space and re-restart.");
        }
        feature_recorder::write0(str);  // call super class
    }
}


/**
 * Combine the pos0, feature and context into a single line and write it to the feature file.
 * This must be called for every feature
 *
 * @param feature - The feature, which is valid UTF8 (but may not be exactly the bytes on the disk)
 * @param context - The context, which is valid UTF8 (but may not be exactly the bytes on the disk)
 *
 * Interlocking is done in write().
 */

void feature_recorder_file::write0(const pos0_t &pos0, const std::string &feature, const std::string &context)
{
    if ( fs.flags.disabled ) {
        return;
    }
    std::stringstream ss;
    ss << pos0.shift( fs.offset_add).str() << '\t' << feature;
    if ((flags.no_context == false) && ( context.size()>0 )) {
        ss << '\t' << context;
    }
    feature_recorder::write0(pos0, feature, context); // call super
    write0( ss.str() );                               // and do the actual write
}



#if 0
void feature_recorder::printf(const char *fmt, ...)
{
    const int maxsize = 65536;
    managed_malloc<char>p(maxsize);

    if(p.buf==0) return;

    va_list ap;
    va_start(ap,fmt);
    vsnprintf(p.buf,maxsize,fmt,ap);
    va_end(ap);
    this->write(p.buf);
}
#endif


#if 0
/* Hook for writing histogram
 */
static int callback_counter(void *param, int argc, char **argv, char **azColName)
{
    int *counter = reinterpret_cast<int *>(param);
    (*counter)++;
    return 0;
}
#endif

#if 0
static void dump_hist(sqlite3_context *ctx,int argc,sqlite3_value**argv)
{
    const histogram_def *def = reinterpret_cast<const histogram_def *>(sqlite3_user_data(ctx));

#ifdef DEBUG
    std::cerr << "behist feature=" << def->feature << "  suffix="
              << def->suffix << "  argc=" << argc << "value = " << sqlite3_value_text(argv[0]) << "\n";
#endif
    std::string new_feature(reinterpret_cast<const char *>(sqlite3_value_text(argv[0])));
    std::smatch sm;
    std::regex_search( new_feature, sm, def->reg);
    if (sm.size() > 0){
        new_feature = sm.str();
        sqlite3_result_text(ctx,new_feature.c_str(),new_feature.size(),SQLITE_TRANSIENT);
    }
}
#endif

/****************************************************************
 *** FILE-BASED HISOTGRAMS
 ****************************************************************/



void feature_recorder_file::generate_histogram(std::ostream &os, const struct histogram_def &def)
{
#if 0
    const std::lock_guard<std::mutex> lock(Mios);
    ios.flush();
    os << histogram_file_header;

    /* This is a file based histogram. We will be reading from one file and writing to another */
    std::ifstream f(fname.c_str());
    if(!f.is_open()){
        throw std::invalid_argument("Cannot open histogram input file: " + ifname);
    }

    /* Read each line of the feature file and add it to the histogram.
     * If we run out of memory, dump that histogram to a file and start
     * on the next histogram.
     */
    for(int histogram_counter = 0;histogram_counter<max_histogram_files;histogram_counter++){

        AtomicUnicodeHistogram h(def.flags);            /* of seen features, created in pass two */
        try {
            std::string line;
            while (getline(f,line)){
                if (line.size()==0) continue; // empty line
                if (line[0]=='#') continue;   // comment line
                truncate_at(line,'\r');      // truncate at a \r if there is one.

                /** If there is a string required in the line and it isn't present, don't use this line */
                if (def.require.size()){
                    if(line.find_first_of(def.require)==std::string::npos){
                        continue;
                    }
                }

                std::string feature = extract_feature(line);
                // Unquote string if it has anything quoted
                if (feature.find('\\')!=std::string::npos){
                    feature = unquote_string(feature);
                }
                /** If there is a pattern to use to prune down the feature, use it */
                if (def.pattern.size()){
                    std::smatch sm;
                    std::regex_search( feature, sm, def.reg);
                    if (sm.size()>0) {
                        feature = sm.str();
                    }
                }

                /* Remove what follows after \t if this is a context file */
                size_t tab=feature.find('\t');
                if (tab!=std::string::npos) feature.erase(tab); // erase from tab to end

                /* Add the feature! */
                h.add(feature);
            }
            f.close();
        }
        catch (const std::exception &e) {
            std::cerr << "ERROR: " << e.what() << " generating histogram " << name << "\n";
        }

*Simson is here

        /* Output what we have to a new file ofname */
        std::stringstream real_suffix;

        real_suffix << def.suffix;
        if(histogram_counter>0) real_suffix << histogram_counter;

        std::string ofname = fname_suffix(real_suffix.str()); // histogram name
        std::ofstream o;
        o.open(ofname.c_str());         // open the file
        if(!o.is_open()){
            std::cerr << "Cannot open histogram output file: " << ofname << "\n";
            return;
        }

        AtomicUnicodeHistogram::FrequencyReportVector *fr = h.makeReport();
        if(fr->size()>0){
            banner_stamp(o,histogram_file_header);
            o << *fr;                   // sends the entire histogram
        }

        for(size_t i = 0;i<fr->size();i++){
            delete fr->at(i);
        }
        delete fr;
        o.close();

        if(f.is_open()==false){
            return;     // input file was closed
        }
    }
    std::cerr << "Looped " << max_histogram_files
              << " times on histogram; something seems wrong\n";
#endif
}


#if 0
void feature_recorder::dump_histogram(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const
{
    /* Inform that we are dumping this histogram */
    if(cb) cb(user,*this,def,"",0);

    /* If this is a memory histogram, dump it and return */
    mhistograms_t::const_iterator it = mhistograms.find(def);
    if(it!=mhistograms.end()){
        assert(cb!=0);
        mhistogram_callback mcbo(user,cb,def,*this,mhistogram_limit);
        it->second->dump_sorted(static_cast<void *>(&mcbo),mhistogram_callback::callback);
        return;
    }

#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    if (fs.flag_set(feature_recorder_set::ENABLE_SQLITE3_RECORDERS)) {
        dump_histogram_sqlite3(def,user,cb);
    }
#endif
    if (fs.flag_notset(feature_recorder_set::DISABLE_FILE_RECORDERS)) {
        dump_histogram_file(def,user,cb);
    }
}
#endif


/* Dump all of this feature recorders histograms */


#if 0
void feature_recorder::dump_histograms(void *user,feature_recorder::dump_callback_t cb,
                                           feature_recorder_set::xml_notifier_t xml_error_notifier) const
{
    /* If we are recording features to SQL and we have a histogram defintion
     * for this feature recorder, we need to create a base histogram first,
     * then we can create the extracted histograms if they are presented.
     */


    /* Loop through all the histograms and dump each one.
     * This now works for both memory histograms and non-memory histograms.
     */
    for (auto it:histogram_defs ){
        try {
            dump_histogram((it),user,cb);
        }
        catch (const std::exception &e) {
            std::cerr << "ERROR: histogram " << name << ": " << e.what() << "\n";
            if(xml_error_notifier){
                std::string error = std::string("<error function='phase3' histogram='")
                    + name + std::string("</error>");
                (*xml_error_notifier)(error);
            }
        }
    }
}
#endif


#if 0
void feature_recorder::add_histogram(const histogram_def &def)
{
    histogram_defs.insert(def);
}
#endif
