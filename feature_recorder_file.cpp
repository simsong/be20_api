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
#include "histogram.h"
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
    /*
     * If the feature recorder set is disabled, just return.
     */
    if ( fs.flag_set_disabled ) return;
    //if ( fs.flag_set(feature_recorder_set::DISABLE_FILE_RECORDERS)) return;

    /* Open the file recorder for output.
     * If the file exists, seek to the end and find the last complete line, and start there.
     */
    std::string fname = fname_suffix("");
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
        std::cerr << "*** feature_recorder::open CANNOT OPEN FEATURE FILE FOR WRITING "
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



#if 0
/* I'm not sure that this is needed */
void feature_recorder::flush()
{
    const std::lock_guard<std::mutex> lock(Mf);            // get the lock; released when object is deallocated.
    ios.flush();
}
#endif


static inline bool isodigit(char c)
{
    return c>='0' && c<='7';
}

/* statics */
const std::string feature_recorder_file::feature_file_header("# Feature-File-Version: 1.1\n");
const std::string feature_recorder_file::histogram_file_header("# Histogram-File-Version: 1.1\n");
const std::string feature_recorder_file::bulk_extractor_version_header("# " PACKAGE_NAME "-Version: " PACKAGE_VERSION " ($Rev: 10844 $)\n");

#if 0
void feature_recorder::set_flag(uint32_t flags_)
{
    flags|=flags_;
}

void feature_recorder::unset_flag(uint32_t flags_)
{
    flags &= (~flags_);
}
#endif

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

/****************************************************************
 *** PHASE HISTOGRAM (formerly phase 3): Create the histograms
 ****************************************************************/

/**
 * We now have three kinds of histograms:
 * 1 - Traditional post-processing histograms specified by the histogram library
 *   1a - feature-file based traditional ones
 *   1b - SQL-based traditional ones.
 * 2 - In-memory histograms (used primarily by beapi)
 */


size_t feature_recorder::count_histograms() const
{
    return histogram_defs.size();
}


/** Dump a specific histogram */
void feature_recorder::dump_histogram_file(const histogram_def &def,
                                           void *user,
                                           feature_recorder::dump_callback_t cb) const
{
    /* This is a file based histogram. We will be reading from one file and writing to another */
    std::string ifname = fname_suffix("");  // source of features
    std::ifstream f(ifname.c_str());
    if(!f.is_open()){
        std::cerr << "Cannot open histogram input file: " << ifname << "\n";
        return;
    }

    /* Read each line of the feature file and add it to the histogram.
     * If we run out of memory, dump that histogram to a file and start
     * on the next histogram.
     */
    for(int histogram_counter = 0;histogram_counter<max_histogram_files;histogram_counter++){

        HistogramMaker h(def.flags);            /* of seen features, created in pass two */
        try {
            std::string line;
            while(getline(f,line)){
                if(line.size()==0) continue; // empty line
                if(line[0]=='#') continue;   // comment line
                truncate_at(line,'\r');      // truncate at a \r if there is one.

                /** If there is a string required in the line and it isn't present, don't use this line */
                if(def.require.size()){
                    if(line.find_first_of(def.require)==std::string::npos){
                        continue;
                    }
                }

                std::string feature = extract_feature(line);
                if (feature.find('\\')!=std::string::npos){
                    feature = unquote_string(feature);  // reverse \xxx encoding
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
                if(tab!=std::string::npos) feature.erase(tab); // erase from tab to end
                h.add(feature);
            }
            f.close();
        }
        catch (const std::exception &e) {
            std::cerr << "ERROR: " << e.what() << " generating histogram "
                      << name << "\n";
        }

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

        HistogramMaker::FrequencyReportVector *fr = h.makeReport();
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
}


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


/* Dump all of this feature recorders histograms */


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


void feature_recorder::add_histogram(const histogram_def &def)
{
    histogram_defs.insert(def);
}
#endif


/****************************************************************
 *** WRITING SUPPORT
 ****************************************************************/

/* write to the file.
 * this is the only place where writing happens.
 * so it's an easy place to do utf-8 validation in debug mode.
 */
void feature_recorder_file::write(const std::string &str)
{
    if (fs.debug & DEBUG_PEDANTIC){
        if (utf8::find_invalid(str.begin(),str.end()) != str.end()){
            std::cerr << "******************************************\n";
            std::cerr << "feature recorder: " << name << "\n";
            std::cerr << "invalid utf-8 in write: " << str << "\n";
            assert(0);
        }
    }

    /* this is where the writing happens. lock the output and write */
    if (fs.flag_set(feature_recorder_set::DISABLE_FILE_RECORDERS)) {
        return;
    }

    const std::lock_guard<std::mutex> lock(Mios);
    if(ios.is_open()){
        if (ios.tellg()==0){
            banner_stamp(ios, feature_file_header);
        }

        ios << str << '\n';
        if (ios.fail()){
            throw std::runtime_error("Disk full. Free up space and re-restart.");
        }
        //features_written += 1;
    }
}

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



/****************************************************************
 *** carving support
 ****************************************************************
 *
 * carving support.
 * 2014-04-24 - $ is no longer valid either
 * 2013-08-29 - replace invalid characters in filenames
 * 2013-07-30 - automatically bin directories
 * 2013-06-08 - filenames are the forensic path.
 */

std::string valid_dosname(std::string in)
{
    std::string out;
    for(size_t i=0;i<in.size();i++){
        uint8_t ch = in.at(i);
        if(ch<=32 || ch>=128
           || ch=='"' || ch=='*' || ch=='+' || ch==','
           || ch=='/' || ch==':' || ch==';' || ch=='<'
           || ch=='=' || ch=='>' || ch=='?' || ch=='\\'
           || ch=='[' || ch==']' || ch=='|' || ch=='$' ){
            out.push_back('_');
        } else {
            out.push_back(ch);
        }
    }
    return out;
}


//const feature_recorder::hash_def &feature_recorder::hasher()
//{
//    return fs.hasher;
//}


#if 0
const std::string feature_recorder::hash(const unsigned char *buf, size_t buffsize)
{
    return (*fs.hasher.func)(buf,buffsize);
}
#endif

#include <iomanip>
/**
 * @param sbuf   - the buffer to carve
 * @param pos    - offset in the buffer to carve
 * @param len    - how many bytes to carve
 *
 */
std::string feature_recorder::carve(const sbuf_t &sbuf,size_t pos,size_t len, const std::string &ext)
{
    if(flags & FLAG_DISABLED) return std::string();           // disabled

    /* If we are in the margin, ignore; it will be processed again */
    if(pos >= sbuf.pagesize && pos < sbuf.bufsize){
        return std::string();
    }
    assert(pos < sbuf.bufsize);



    /* Carve to a file depending on the carving mode.  The purpose
     * of CARVE_ENCODED is to allow us to carve JPEGs when they are
     * embedded in, say, GZIP files, but not carve JPEGs that are
     * bare.  The difficulty arises when you have a tool that can go
     * into, say, ZIP files. In this case, we don't want to carve
     * every ZIP file, just the (for example) XORed ZIP files. So the
     * ZIP carver doesn't carve every ZIP file, just the ZIP files
     * that are in HIBER files.  That is, we want to not carve a path
     * of ZIP-234234 but we do want to carve a path of
     * 1000-HIBER-33423-ZIP-2343.  This is implemented by having an
     * do_not_carve_encoding. the ZIP carver sets it to ZIP so it won't
     * carve things that are just found in a ZIP file. This means that
     * it won't carve disembodied ZIP files found in unallocated
     * space. You might want to do that.  If so, set ZIP's carve mode
     * to CARVE_ALL.
     */
    switch(carve_mode){
    case CARVE_NONE:
        return std::string();                         // carve nothing
    case CARVE_ENCODED:
        if (sbuf.pos0.path.size() == 0 ) return std::string(); // not encoded
        if (sbuf.pos0.alphaPart() == do_not_carve_encoding) return std::string(); // ignore if it is just encoded with this
        break;                                      // otherwise carve
    case CARVE_ALL:
        break;
    }

    /* If the directory doesn't exist, make it.
     * If two threads try to make the directory,
     * that's okay, because the second one will fail.
     */

    sbuf_t cbuf(sbuf,pos,len);          // the buf we are going to carve
    std::string carved_hash_hexvalue = hash(cbuf);

    /* See if this is in the cache */
    bool in_cache = carve_cache.check_for_presence_and_insert(carved_hash_hexvalue);


    uint64_t this_file_number = file_number_add(in_cache ? 0 : 1); // increment if we are not in the cache
    std::string dirname1 = fs.get_outdir() + "/" + name;

    std::stringstream ss;
    ss << dirname1 << "/" << std::setw(3) << std::setfill('0') << (this_file_number / 1000);

    std::string dirname2 = ss.str();
    std::string fname         = dirname2 + std::string("/") + valid_dosname(cbuf.pos0.str() + ext);
    std::string fname_feature = fname.substr(fs.get_outdir().size()+1);

    /* Record what was found in the feature file.
     */
    if (in_cache){
        fname="";             // no filename
        fname_feature="<CACHED>";
    }

    // write to the feature file
    ss.str(std::string()); // clear the stringstream
    ss << "<fileobject>";
    if (!in_cache) ss << "<filename>" << fname << "</filename>";
    ss << "<filesize>" << len << "</filesize>";
    ss << "<hashdigest type='" << fs.hasher.name << "'>" << carved_hash_hexvalue << "</hashdigest></fileobject>";
    this->write(cbuf.pos0,fname_feature,ss.str());

    if (in_cache) return fname;               // do not make directories or write out if we are cached

    /* Make the directory if it doesn't exist.  */
    if (access(dirname2.c_str(),R_OK)!=0){
#ifdef WIN32
        mkdir(dirname1.c_str());
        mkdir(dirname2.c_str());
#else
        mkdir(dirname1.c_str(),0777);
        mkdir(dirname2.c_str(),0777);
#endif
    }
    /* Check to make sure that directory is there. We don't just the return code
     * because there could have been two attempts to make the directory simultaneously,
     * so the mkdir could fail but the directory could nevertheless exist. We need to
     * remember the error number because the access() call may clear it.
     */
    int oerrno = errno;                 // remember error number
    if (access(dirname2.c_str(),R_OK)!=0){
        std::cerr << "Could not make directory " << dirname2 << ": " << strerror(oerrno) << "\n";
        return std::string();
    }

    /* Write the file into the directory */
    int fd = ::open(fname.c_str(),O_CREAT|O_BINARY|O_RDWR,0666);
    if(fd<0){
        std::cerr << "*** carve: Cannot create " << fname << ": " << strerror(errno) << "\n";
        return std::string();
    }

    ssize_t ret = cbuf.write(fd,0,len);
    if(ret<0){
        std::cerr << "*** carve: Cannot write(pos=" << fd << "," << pos << " len=" << len << "): "<< strerror(errno) << "\n";
    }
    ::close(fd);
    return fname;
}

/*
 This is based on feature_recorder::carve and append carving record to specified filename
 */
std::string feature_recorder::carve_records(const sbuf_t &sbuf, size_t pos, size_t len,
                                            const std::string &filename)
{
    if(flags & FLAG_DISABLED) return std::string();           // disabled

    if(pos >= sbuf.pagesize && pos < sbuf.bufsize){
        return std::string();
    }
    assert(pos < sbuf.bufsize);

    sbuf_t cbuf(sbuf,pos,len);          // the buf we are going to carve
    std::string carved_hash_hexvalue = (*fs.hasher.func)(cbuf.buf,cbuf.bufsize);

    /* See if this is in the cache */
    bool in_cache = carve_cache.check_for_presence_and_insert(carved_hash_hexvalue);
    std::string dirname1 = fs.get_outdir()  + "/" + name;

    std::stringstream ss;
    ss << dirname1;

    std::string dirname2 = ss.str();
    std::string fname = dirname2 + std::string("/") + valid_dosname(filename);
    std::string fname_feature = fname.substr(fs.get_outdir().size()+1);

    //    std::string fname = dirname2 + std::string("/") + valid_dosname(cbuf.pos0.str() + ext);
    //std::string fname_feature = fname.substr(fs.get_outdir().size()+1);

    /* Record what was found in the feature file.
     */
    if (in_cache){
        fname="";             // no filename
        fname_feature="<CACHED>";
    }

    // write to the feature file
    ss.str(std::string()); // clear the stringstream
    ss << len;
    this->write(cbuf.pos0,fname_feature,ss.str());

    if (in_cache) return fname;               // do not make directories or write out if we are cached

    /* Make the directory if it doesn't exist.  */
    if (access(dirname2.c_str(),R_OK)!=0){
#ifdef WIN32
        mkdir(dirname1.c_str());
        mkdir(dirname2.c_str());
#else
        mkdir(dirname1.c_str(),0777);
        mkdir(dirname2.c_str(),0777);
#endif
    }

    int oerrno = errno;                 // remember error number
    if (access(dirname2.c_str(),R_OK)!=0){
        std::cerr << "Could not make directory " << dirname2 << ": " << strerror(oerrno) << "\n";
        return std::string();
    }

    /* Write the file into the directory */
    int fd = ::open(fname.c_str(),O_APPEND|O_CREAT|O_BINARY|O_RDWR,0666);
    if(fd<0){
        std::cerr << "*** carve: Cannot create " << fname << ": " << strerror(errno) << "\n";
        return std::string();
    }

    ssize_t ret = cbuf.write(fd,0,len);
    if(ret<0){
        std::cerr << "*** carve: Cannot write(pos=" << fd << "," << pos << " len=" << len << "): "<< strerror(errno) << "\n";
    }
    ::close(fd);
    return fname;
}

/**
 * Currently, we need strptime() and utimes() to set the time.
 */
void feature_recorder::set_carve_mtime(const std::string &fname, const std::string &mtime_iso8601)
{
    if(flags & FLAG_DISABLED) return;           // disabled
#if defined(HAVE_STRPTIME) && defined(HAVE_UTIMES)
    if(fname.size()){
        struct tm tm;
        if(strptime(mtime_iso8601.c_str(),"%Y-%m-%dT%H:%M:%S",&tm)){
            time_t t = mktime(&tm);
            if(t>0){
                const struct timeval times[2] = {{t,0},{t,0}};
                utimes(fname.c_str(),times);
            }
        }
    }
#endif
}

#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
/*** SQL Routines Follow ***
 *
 * Time results with ubnist1 on R4:
 * no SQL - 79 seconds
 * no pragmas - 651 seconds
 * "PRAGMA synchronous =  OFF", - 146 second
 * "PRAGMA synchronous =  OFF", "PRAGMA journal_mode=MEMORY", - 79 seconds
 *
 * Time with domexusers:
 * no SQL -
 */

#define SQLITE_EXTENSION ".sqlite"
#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

/* This creates the base histogram. Note that the SQL fails if the histogram exists */
static const char *schema_hist[] = {
    "CREATE TABLE h_%s (count INTEGER(12), feature_utf8 TEXT)",
    "CREATE INDEX h_%s_idx1 ON h_%s(count)",
    "CREATE INDEX h_%s_idx2 ON h_%s(feature_utf8)",
    0};

/* This performs the histogram operation */
static const char *schema_hist1[] = {
    "INSERT INTO h_%s select COUNT(*),feature_utf8 from f_%s GROUP BY feature_utf8",
    0};

#ifdef HAVE_SQLITE3_CREATE_FUNCTION_V2
static const char *schema_hist2[] = {
    "INSERT INTO h_%s select sum(count),BEHIST(feature_utf8) from h_%s where BEHIST(feature_utf8)!='' GROUP BY BEHIST(feature_utf8)",
    0};
#endif


#define DB_INSERT_STMT "INSERT INTO f_%s (offset,path,feature_eutf8,feature_utf8,context_eutf8) VALUES (?1, ?2, ?3, ?4, ?5)"
const char *feature_recorder::db_insert_stmt = DB_INSERT_STMT;

void feature_recorder::besql_stmt::insert_feature(const pos0_t &pos,
                                                        const std::string &feature,
                                                        const std::string &feature8, const std::string &context)
{
    assert(stmt!=0);
    const std::lock_guard<std::mutex> lock(Mstmt);           // grab a lock
    const std::string &path = pos.str();
    sqlite3_bind_int64(stmt, 1, pos.imageOffset()); // offset
    sqlite3_bind_text(stmt, 2, path.data(), path.size(), SQLITE_STATIC); // path
    sqlite3_bind_text(stmt, 3, feature.data(), feature.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, feature8.data(), feature8.size(), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, context.data(), context.size(), SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr,"sqlite3_step failed\n");
    }
    sqlite3_reset(stmt);
};

feature_recorder::besql_stmt::besql_stmt(sqlite3 *db3,const char *sql):Mstmt(),stmt()
{
    assert(db3!=0);
    assert(sql!=0);
    sqlite3_prepare_v2(db3,sql, strlen(sql), &stmt, NULL);
    assert(stmt!=0);
}

feature_recorder::besql_stmt::~besql_stmt()
{
    assert(stmt!=0);
    sqlite3_finalize(stmt);
    stmt = 0;
}

/* Hook for writing feature to SQLite3 database */
void feature_recorder::write0_sqlite3(const pos0_t &pos0,const std::string &feature,const std::string &context)
{
    /**
     * Note: this is not very efficient, passing through a quoted feature and then unquoting it.
     * We could make this more efficient.
     */
    std::string *feature8 = HistogramMaker::convert_utf16_to_utf8(feature_recorder::unquote_string(feature));
    assert(bs!=0);
    bs->insert_feature(pos0,feature,
                         feature8 ? *feature8 : feature,
                         flag_set(feature_recorder::FLAG_NO_CONTEXT) ? "" : context);
    if (feature8) delete feature8;
}

/* Hook for writing histogram
 */
static int callback_counter(void *param, int argc, char **argv, char **azColName)
{
    int *counter = reinterpret_cast<int *>(param);
    (*counter)++;
    return 0;
}

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

void feature_recorder::dump_histogram_sqlite3(const histogram_def &def,void *user,feature_recorder::dump_callback_t cb) const
{
    /* First check to see if there exists a feature histogram summary. If not, make it */
    std::string query = "SELECT name FROM sqlite_master WHERE type='table' AND name='h_" + def.feature +"'";
    char *errmsg=0;
    int rowcount=0;
    if ( sqlite3_exec(fs.db3,query.c_str(),callback_counter,&rowcount,&errmsg)){
        std::cerr << "sqlite3: " << errmsg << "\n";
        return;
    }
    if (rowcount==0){
        const char *feature = def.feature.c_str();
        fs.db_send_sql( fs.db3, schema_hist, feature, feature); // creates the histogram
        fs.db_send_sql( fs.db3, schema_hist1, feature, feature); // creates the histogram
    }
#ifdef HAVE_SQLITE3_CREATE_FUNCTION_V2
    /* Now create the summarized histogram for the regex, if it is not existing, but only if we have
     * sqlite3_create_function_v2
     */
    if (def.pattern.size()>0){
        /* Create the database where we will add the histogram */
        std::string hname = def.feature + "_" + def.suffix;

        /* Remove any "-" characters if present */
        for(size_t i=0;i<hname.size();i++){
            if (hname[i]=='-') hname[i]='_';
        }

        if(debug) std::cerr << "CREATING TABLE = " << hname << "\n";
        if (sqlite3_create_function_v2(fs.db3,"BEHIST",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                       (void *)&def,dump_hist,0,0,0)) {
            std::cerr << "could not register function BEHIST\n";
            return;
        }
        const char *fn = def.feature.c_str();
        const char *hn = hname.c_str();
        fs.db_send_sql(fs.db3,schema_hist, hn , hn); // create the table
        fs.db_send_sql(fs.db3,schema_hist2, hn , fn); // select into it from a function of the old histogram table

        /* erase the user defined function */
        if (sqlite3_create_function_v2(fs.db3,"BEHIST",1,SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                       (void *)&def,0,0,0,0)) {
            std::cerr << "could not remove function BEHIST\n";
            return;
        }
    }
#endif
}
#endif
