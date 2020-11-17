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
static inline int hexval(char ch)
{
    switch (ch) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    }
    return 0;
}

/**
 * Unquote Python or octal-style quoting of a string
 */
std::string feature_recorder::unquote_string(const std::string &s)
{
    size_t len = s.size();
    if(len<4) return s;                 // too small for a quote

    std::string out;
    for (size_t i=0;i<len;i++){
        /* Look for octal coding */
        if(i+3<len && s[i]=='\\' && isodigit(s[i+1]) && isodigit(s[i+2]) && isodigit(s[i+3])){
            uint8_t code = (s[i+1]-'0') * 64 + (s[i+2]-'0') * 8 + (s[i+3]-'0');
            out.push_back(code);
            i += 3;                     // skip over the digits
            continue;
        }
        /* Look for hex coding */
        if(i+3<len && s[i]=='\\' && s[i+1]=='x' && isxdigit(s[i+2]) && isxdigit(s[i+3])){
            uint8_t code = (hexval(s[i+2])*16) | hexval(s[i+3]);
            out.push_back(code);
            i += 3;                     // skip over the digits
            continue;
        }
        out.push_back(s[i]);
    }
    return out;
}

/**
 * Get the feature which is defined as being between a \t and [\t\n]
 */

/*static*/ std::string feature_recorder::extract_feature(const std::string &line)
{
    size_t tab1 = line.find('\t');
    if(tab1==std::string::npos) return "";   // no feature
    size_t feature_start = tab1+1;
    size_t tab2 = line.find('\t',feature_start);
    if(tab2!=std::string::npos) return line.substr(feature_start,tab2-feature_start);
    return line.substr(feature_start);  // no context to remove
}

const std::string &feature_recorder::get_outdir() const // cannot be inline becuase it accesses fs
{
    return fs.get_outdir();
}

/*
 * Returns a filename for this feature recorder with a specific suffix.
 */
const std::string feature_recorder::fname_in_outdoor(std::string suffix) const
{
    return fs.get_outdir() + "/" + this->name + (suffix.size()>0 ? (std::string("_") + suffix) : "") + ".txt";
}




/**
 * Combine the pos0, feature and context into a single line and write it to the feature file.
 *
 * @param feature - The feature, which is valid UTF8 (but may not be exactly the bytes on the disk)
 * @param context - The context, which is valid UTF8 (but may not be exactly the bytes on the disk)
 *
 * Interlocking is done in write().
 */

void feature_recorder::write0(const pos0_t &pos0,const std::string &feature,const std::string &context)
{
#if defined(HAVE_SQLITE3_H) and defined(HAVE_LIBSQLITE3)
    if ( fs.flag_set(feature_recorder_set::ENABLE_SQLITE3_RECORDERS ) &&
         this->flag_notset(feature_recorder::FLAG_NO_FEATURES_SQL) ) {
        write0_sqlite3( pos0, feature, context);
    }
#endif
    if ( fs.flag_notset(feature_recorder_set::DISABLE_FILE_RECORDERS )) {
        std::stringstream ss;
        ss << pos0.shift( feature_recorder::offset_add).str() << '\t' << feature;
        if (flag_notset( FLAG_NO_CONTEXT ) && ( context.size()>0 )) ss << '\t' << context;
        this->write( ss.str() );
    }
}


/**
 * the main entry point of writing a feature and its context to the feature file.
 * processes the stop list
 */

void feature_recorder::quote_if_necessary(std::string &feature,std::string &context)
{
    /* By default quote string that is not UTF-8, and quote backslashes. */
    bool escape_bad_utf8  = true;
    bool escape_backslash = true;

    if(flags & FLAG_NO_QUOTE){          // don't quote either
        escape_bad_utf8  = false;
        escape_backslash = false;
    }

    if(flags & FLAG_XML){               // only quote bad utf8
        escape_bad_utf8  = true;
        escape_backslash = false;
    }

    feature = validateOrEscapeUTF8(feature, escape_bad_utf8,escape_backslash,validateOrEscapeUTF8_validate);
    if(feature.size() > opt_max_feature_size) feature.resize(opt_max_feature_size);
    if(flag_notset(FLAG_NO_CONTEXT)){
        context = validateOrEscapeUTF8(context,escape_bad_utf8,escape_backslash,validateOrEscapeUTF8_validate);
        if(context.size() > opt_max_context_size) context.resize(opt_max_context_size);
    }
}

/**
 * write() is the main entry point for writing a feature at a given position with context.
 * write() checks the stoplist and escapes non-UTF8 characters, then calls write0().
 */
void feature_recorder::write(const pos0_t &pos0,const std::string &feature_,const std::string &context_)
{
    if(flags & FLAG_DISABLED) return;           // disabled
    if(debug & DEBUG_PEDANTIC){
        if(feature_.size() > opt_max_feature_size){
            std::cerr << "feature_recorder::write : feature_.size()=" << feature_.size() << "\n";
            assert(0);
        }
        if(context_.size() > opt_max_context_size){
            std::cerr << "feature_recorder::write : context_.size()=" << context_.size() << "\n";
            assert(0);
        }
    }

    std::string feature = feature_;
    std::string context = flag_set(FLAG_NO_CONTEXT) ? "" : context_;
    std::string *feature_utf8 = HistogramMaker::make_utf8(feature); // a utf8 feature

    quote_if_necessary(feature,context);

    if(feature.size()==0){
        std::cerr << name << ": zero length feature at " << pos0 << "\n";
        if(debug & DEBUG_PEDANTIC) assert(0);
        return;
    }
    if(debug & DEBUG_PEDANTIC){
        /* Check for tabs or newlines in feature and and context */
        for(size_t i=0;i<feature.size();i++){
            if(feature[i]=='\t') assert(0);
            if(feature[i]=='\n') assert(0);
            if(feature[i]=='\r') assert(0);
        }
        for(size_t i=0;i<context.size();i++){
            if(context[i]=='\t') assert(0);
            if(context[i]=='\n') assert(0);
            if(context[i]=='\r') assert(0);
        }
    }

    /* First check to see if the feature is on the stop list.
     * Only do this if we have a stop_list_recorder (the stop list recorder itself
     * does not have a stop list recorder. If it did we would infinitely recurse.
     */
    if(flag_notset(FLAG_NO_STOPLIST) && stop_list_recorder){
        if(fs.stop_list
           && fs.stop_list->check_feature_context(*feature_utf8,context)){
            stop_list_recorder->write(pos0,feature,context);
            delete feature_utf8;
            return;
        }
    }

    /* The alert list is a special features that are called out.
     * If we have one of those, write it to the redlist.
     */
    if(flag_notset(FLAG_NO_ALERTLIST)
       && fs.alert_list
       && fs.alert_list->check_feature_context(*feature_utf8,context)){
        std::string alert_fn = fs.get_outdir() + "/ALERTS_found.txt";
        const std::lock_guard<std::mutex> lock(Mr);                // notice we are locking the alert list
        std::ofstream rf(alert_fn.c_str(),std::ios_base::app);
        if(rf.is_open()){
            rf << pos0.shift(feature_recorder::offset_add).str() << '\t' << feature << '\t' << "\n";
        }
    }

    /* Support in-memory histograms */
    for (auto it:mhistograms ){
        const histogram_def &def = it.first;
        mhistogram_t *m = it.second;
        std::string new_feature = *feature_utf8;
        if (def.require.size()==0 || new_feature.find_first_of(def.require)!=std::string::npos){
            /* If there is a pattern to use, use it to simplify the feature */
            if (def.pattern.size()){
                std::smatch sm;
                std::regex_search( new_feature, sm, def.reg);
                if (sm.size() == 0){
                    // no search match; avoid this feature
                    new_feature = "";
                }
                else {
                    new_feature = sm.str();
                }
            }
            if(new_feature.size()) m->add(new_feature,1);
        }
    }

    /* Finally write out the feature and the context */
    if(flag_notset(FLAG_NO_FEATURES)){
        this->write0(pos0,feature,context);
    }
    delete feature_utf8;
}

/**
 * Given a buffer, an offset into that buffer of the feature, and the length
 * of the feature, make the context and write it out. This is mostly used
 * for writing from within the lexical analyzers.
 */

void feature_recorder::write_buf(const sbuf_t &sbuf,size_t pos,size_t len)
{
#ifdef DEBUG_SCANNER
    if(debug & DEBUG_SCANNER){
        std::cerr << "*** write_buf " << name << " sbuf=" << sbuf << " pos=" << pos << " len=" << len << "\n";
        // for debugging, print Imagine that when pos= the location where the crash is happening.
        // then set a breakpoint at std::cerr.
        if(pos==9999999){
            std::cerr << "Imagine that\n";
        }
    }
#endif

    /* If we are in the margin, ignore; it will be processed again */
    if(pos >= sbuf.pagesize && pos < sbuf.bufsize){
        return;
    }

    if(pos >= sbuf.bufsize){    /* Sanity checks */
        std::cerr << "*** write_buf: WRITE OUTSIDE BUFFER. "
                  << " pos="  << pos
                  << " sbuf=" << sbuf << "\n";
        return;
    }

    /* Asked to write beyond bufsize; bring it in */
    if(pos+len > sbuf.bufsize){
        len = sbuf.bufsize - pos;
    }

    std::string feature = sbuf.substr(pos,len);
    std::string context;

    if((flags & FLAG_NO_CONTEXT)==0){
        /* Context write; create a clean context */
        size_t p0 = context_window < pos ? pos-context_window : 0;
        size_t p1 = pos+len+context_window;

        if(p1>sbuf.bufsize) p1 = sbuf.bufsize;
        assert(p0<=p1);
        context = sbuf.substr(p0,p1-p0);
    }
    this->write(sbuf.pos0+pos,feature,context);
#ifdef DEBUG_SCANNER
    if(debug & DEBUG_SCANNER){
        std::cerr << ".\n";
    }
#endif
}


/**
 * replace a character in a string with another
 */
std::string replace(const std::string &src,char f,char t)
{
    std::string ret;
    for(size_t i=0;i<src.size();i++){
        if(src[i]==f) ret.push_back(t);
        else ret.push_back(src[i]);
    }
    return ret;
}

/*
 write buffer to specified dirname/filename for writing data
 */
std::string feature_recorder::carve_data(const sbuf_t &sbuf, const std::string &ext)
{
    std::string dirname1 = fs.get_outdir()  + "/" + name;
    std::stringstream ss;
    ss << dirname1;
    std::string dirname2 = ss.str();
    std::string fname = dirname2 + std::string("/") + valid_dosname(filename);

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

    // To control multiple thread writing
    const std::lock_guard<std::mutex> lock(Mios);

    /* Write the file into the directory */
    int fd = ::open(fname.c_str(),O_CREAT|O_BINARY|O_RDWR,0666);
    if(fd<0){
        std::cerr << "*** carve: Cannot create " << fname << ": " << strerror(errno) << "\n";
        return std::string();
    }

    ssize_t ret = ::write(fd,data,len);
    if(ret<0){
        std::cerr << "*** carve: Cannot write geneated header "<< fname << ": " << strerror(errno) << "\n";
    }
    ::close(fd);
    return fname;
}
