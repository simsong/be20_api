/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

#include "config.h"

#ifdef BULK_EXTRACTOR
#include "bulk_extractor.h"
#else
#include "bulk_extractor_i.h"
#endif

#include "unicode_escape.h"
#include "beregex.h"

#ifdef USE_HISTOGRAMS
#include "histogram.h"
#endif

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 65536
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef DEBUG_PEDANTIC
#define DEBUG_PEDANTIC    0x0001// check values more rigorously
#endif

#ifndef WIN32
pthread_t feature_recorder::main_threadid = 0;
#endif
size_t  feature_recorder::context_window_default=16;                    /* number of bytes of context */
int64_t feature_recorder::offset_add   = 0;
std::string  feature_recorder::banner_file;
uint32_t feature_recorder::opt_max_context_size=1024*1024;
uint32_t feature_recorder::opt_max_feature_size=1024*1024;
uint32_t feature_recorder::debug=0;


void feature_recorder::set_in_memory_histogram()
{
    MAINTHREAD();
    //TK
}

/** 
 * 1. Put the UTF-8 BOM in the file.
 * 2. Read the contents of the file pointed to by opt_banner and put each line into "f"
 * 3. Add the header
 */

//const string feature_recorder::UTF8_BOM("\xef\xbb\xbf");
//const string feature_recorder::BOM_EXPLAINATION("# UTF-8 Byte Order Marker; see http://unicode.org/faq/utf_bom.html\n");

void feature_recorder::banner_stamp(std::ostream &os,const std::string &header)
{
    int banner_lines = 0;
    //os << UTF8_BOM << BOM_EXPLAINATION;         // 
    if(banner_file!=""){
        ifstream i(banner_file.c_str());
        if(i.is_open()){
            string line;
            while(getline(i,line)){
                if(line.size()>0 && ((*line.end()=='\r') || (*line.end()=='\n'))){
                    line.erase(line.end()); /* remove the last character while it is a \n or \r */
                }
                os << "# " << line << "\n";
                banner_lines++;
            }
            i.close();
        }
    }
    if(banner_lines==0){
        os << "# BANNER FILE NOT PROVIDED (-b option)\n";
    }
    
    os << bulk_extractor_version_header;
    os << "# Feature-Recorder: " << name << "\n";
    if(input_fname.size()) os << "# Filename: " << input_fname << "\n";
    if(debug!=0){
        os << "# DEBUG: " << debug << " (";
        if(debug & DEBUG_PEDANTIC) os << " DEBUG_PEDANTIC ";
        os << ")\n";
    }
    os << header;
}


/**
 * Create a feature recorder object. Each recorder records a certain
 * kind of feature.  Features are stored in a file. The filename is
 * permutated based on the total number of threads and the current
 * thread that's recording. Each thread records to a different file,
 * and thus a different feature recorder, to avoid locking
 * problems. 
 *
 * @param name - the name of the feature being recorded.
 * @param histogram_enabled - whether or not a histogram should be created.
 */

feature_recorder::feature_recorder(string outdir_,string input_fname_,string name_):
    flags(0),histogram_enabled(false),
    outdir(outdir_),input_fname(input_fname_),name(name_),ignore_encoding(),count_(0),ios(),
    context_window_before(context_window_default),context_window_after(context_window_default),
    Mf(),Mr(),mem_histogram(),
    stop_list_recorder(0),
    file_number(0),carve_mode(CARVE_ENCODED)
{
}

/* Don't have to delete the stop_list_recorder because it is in the
 * feature_recorder_set and will be separately deleted.
 */
feature_recorder::~feature_recorder()
{
    if(ios.is_open()){
        ios.close();
    }
}


/**
 * Return the filename with a counter
 */
string feature_recorder::fname_counter(string suffix)
{
    return outdir + "/" + this->name + (suffix.size()>0 ? (string("_") + suffix) : "") + ".txt";
}


/**
 * open a feature recorder file in the specified output directory.
 */

void feature_recorder::open()
{ 
    string fname = fname_counter("");
    ios.open(fname.c_str(),ios_base::in|ios_base::out|ios_base::ate);
    if(ios.is_open()){                  // opened existing stream
        ios.seekg(0L,ios_base::end);
        while(ios.is_open()){
            /* Get current position */
            if(int(ios.tellg())==0){            // at beginning of file; stamp and return
                ios.seekp(0L,ios_base::beg);    // be sure we are at the beginning of the file
                return;
            }
            ios.seekg(-1,ios_base::cur); // backup to once less than the end of the file
            if (ios.peek()=='\n'){           // we are finally on the \n
                ios.seekg(1L,ios_base::cur); // move the getting one forward
                ios.seekp(ios.tellg(),ios_base::beg); // put the putter at the getter location
                count_ = 1;                            // greater than zero
                return;
            }
        }
    }
    // Just open the stream for output
    ios.open(fname.c_str(),ios_base::out);
    if(!ios.is_open()){
        cerr << "*** CANNOT OPEN FEATURE FILE "
             << fname << ":" << strerror(errno) << "\n";
        exit(1);
    }
}

void feature_recorder::close()
{
    if(ios.is_open()){
        ios.close();
    }
}

void feature_recorder::flush()
{
    cppmutex::lock lock(Mf);            // get the lock; released when object is deallocated.
    ios.flush();
}


static inline bool isodigit(char c)
{
    return c>='0' && c<='7';
}

/* statics */
const std::string feature_recorder::feature_file_header("# Feature-File-Version: 1.1\n");
const std::string feature_recorder::histogram_file_header("# Histogram-File-Version: 1.1\n");
const std::string feature_recorder::bulk_extractor_version_header("# "PACKAGE_NAME "-Version: " PACKAGE_VERSION " ($Rev: 10844 $)\n");

static inline int hexval(char ch)
{
    if(ch>='0' && ch<='9') return ch-'0';
    if(ch>='a' && ch<='f') return ch-'a'+10;
    if(ch>='A' && ch<='F') return ch-'a'+10;
    return 0;
}

/**
 * Unquote Python or octal-style quoting of a string
 */
string feature_recorder::unquote_string(const string &s)
{
    size_t len = s.size();
    if(len<4) return s;                 // too small for a quote

    string out;
    for(size_t i=0;i<len;i++){
        /* Look for octal coding */
        if(i+3<len && s[i]=='\\' && isodigit(s[i+1]) && isodigit(s[i+2]) && isodigit(s[i+3])){
            uint8_t code = (s[i+1]-'0') * 64 + (s[i+2]-'0') * 8 + (s[i+3]-'0');
            out.push_back(code);
            i += 3;                     // skip over the digits
            continue;
        }
        /* Look for hex coding */
        if(i+3<len && s[i]=='\\' && s[i+1]=='x' && isxdigit(s[i+2]) && isxdigit(s[i+3])){
            uint8_t code = (hexval(s[i+2])<<4) | hexval(s[i+3]);
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

/*static*/ string feature_recorder::extract_feature(const string &line)
{
    size_t tab1 = line.find('\t');
    if(tab1==string::npos) return "";   // no feature
    size_t feature_start = tab1+1;
    size_t tab2 = line.find('\t',feature_start);
    if(tab2!=string::npos) return line.substr(feature_start,tab2-feature_start);
    return line.substr(feature_start);  // no context to remove
}

#ifdef USE_HISTOGRAMS
/**
 *  Create a histogram for this feature recorder and an extraction pattern.
 */

void truncate_at(string &line,char ch)
{
    size_t pos = line.find(ch);
    if(pos!=string::npos) line.erase(pos);
}

void feature_recorder::make_histogram(const class histogram_def &def)
{
    beregex reg(def.pattern,REG_EXTENDED);
    string ifname = fname_counter("");  // source of features

    ifstream f(ifname.c_str());
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
            string line;
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

                string feature = extract_feature(line);
                if(feature.find('\\')!=string::npos){
                    feature = unquote_string(feature);  // reverse \xxx encoding
                }
                /** If there is a pattern to use to prune down the feature, use it */
                if(def.pattern.size()){
                    string new_feature = feature;
                    if(!reg.search(feature,&new_feature,0,0)){
                        // no search match; avoid this feature
                        continue;               
                    }
                    feature = new_feature;
                }
        
                /* Remove what follows after \t if this is a context file */
                size_t tab=feature.find('\t');
                if(tab!=string::npos) feature.erase(tab); // erase from tab to end
                h.add(feature);
            }
            f.close();
        }
        catch (const std::exception &e) {
            std::cerr << "ERROR: " << e.what() << " generating histogram "
                      << name << "\n";
        }
            
        /* Output what we have */
        stringstream real_suffix;

        real_suffix << def.suffix;
        if(histogram_counter>0) real_suffix << histogram_counter;
        string ofname = fname_counter(real_suffix.str()); // histogram name
        ofstream o;
        o.open(ofname.c_str());
        if(!o.is_open()){
            cerr << "Cannot open histogram output file: " << ofname << "\n";
            return;
        }

        HistogramMaker::FrequencyReportVector *fr = h.makeReport();
        if(fr->size()>0){
            banner_stamp(o,histogram_file_header);
            o << *fr;                   // sends the entire histogram
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
#endif


/****************************************************************
 *** WRITING SUPPORT
 ****************************************************************/

/* Write to the file.
 * This is the only place where writing happens.
 * So it's an easy place to do UTF-8 validation in debug mode.
 */
void feature_recorder::write(const std::string &str)
{
    if(debug & DEBUG_PEDANTIC){
        if(utf8::find_invalid(str.begin(),str.end()) != str.end()){
            std::cerr << "******************************************\n";
            std::cerr << "feature recorder: " << name << "\n";
            std::cerr << "invalid UTF-8 in write: " << str << "\n";
            std::cerr << "ABORT\n";
            exit(1);
        }
    }
    cppmutex::lock lock(Mf);
    if(ios.is_open()){
        if(count_==0){
            banner_stamp(ios,feature_file_header);
        }

        ios << str << '\n';
        if(ios.fail()){
            cerr << "DISK FULL\n";
            ios.close();
        }
        count_++;
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


/**
 * the main entry point of writing a feature and its context to the feature file.
 * processes the stop list
 */

void feature_recorder::write(const pos0_t &pos0,const string &feature_,const string &context_)
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

    string feature = validateOrEscapeUTF8(feature_, escape_bad_utf8,escape_backslash);

    string context;
    if((flags & FLAG_NO_CONTEXT)==0){
        context = validateOrEscapeUTF8(context_,escape_bad_utf8,escape_backslash);
    }

    if(feature.size() > opt_max_feature_size) feature = feature.substr(0,opt_max_feature_size);
    if(context.size() > opt_max_context_size) context = context.substr(0,opt_max_context_size);
    if(feature.size()==0){
        cerr << "zero length feature at " << pos0 << "\n";
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
        


#ifdef USE_STOP_LIST
    /* First check to see if the feature is on the stop list.
     * Only do this if we have a stop_list_recorder (the stop list recorder itself
     * does not have a stop list recorder. If it did we would infinitely recurse.
     */
    if(((flags & FLAG_NO_STOPLIST)==0) && stop_list_recorder){          
        if(stop_list.check_feature_context(feature,context)){
            stop_list_recorder->write(pos0,feature,context);
            return;
        }
    }
#endif

#ifdef USE_ALERT_LIST
    /* The alert list is a special features that are called out.
     * If we have one of those, write it to the redlist.
     */
    if(((flags & FLAG_NO_ALERTLIST)==0) && alert_list.check_feature_context(feature,context)){
        string alert_fn = outdir + "/ALERTS_found.txt";

        cppmutex::lock lock(Mr);                // notce we are locking the redlist
        ofstream rf(alert_fn.c_str(),ios_base::app);
        if(rf.is_open()){
            rf << pos0.shift(feature_recorder::offset_add).str() << '\t' << feature << '\t' << "\n";
        }
    }
#endif

    /* Finally write out the feature and the context */
    stringstream ss;
    ss << pos0.shift(feature_recorder::offset_add).str() << '\t' << feature;
    if(((flags & FLAG_NO_CONTEXT)==0) && (context.size()>0)) ss << '\t' << context;
    this->write(ss.str());
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

    string feature = sbuf.substr(pos,len);
    string context;

    if((flags & FLAG_NO_CONTEXT)==0){
        /* Context write; create a clean context */
        size_t p0 = context_window_before < pos ? pos-context_window_before : 0;
        size_t p1 = pos+len+context_window_after;
        
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

/****************************************************************
 *** CARVING SUPPORT
 ****************************************************************
 *
 * Carving support.
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
           || ch=='[' || ch==']' || ch=='|'){
            out.push_back('_');
        } else {
            out.push_back(ch);
        }
    }
    return out;
}
        

#include <iomanip>
/**
 * @param sbuf   - the buffer to carve
 * @param pos    - offset in the buffer to carve
 * @param len    - how many bytes to carve
 * @param hasher - to compute the hash of the carved object.
 *
 */
std::string feature_recorder::carve(const sbuf_t &sbuf,size_t pos,size_t len,
                                    const std::string &ext,
                                    const be13::hash_def &hasher)
{
    if(flags & FLAG_DISABLED) return std::string();           // disabled

    /* If we are in the margin, ignore; it will be processed again */
    if(pos >= sbuf.pagesize && pos < sbuf.bufsize){
        return std::string();
    }

    if(pos >= sbuf.bufsize){    /* Sanity checks */
        cerr << "*** carve: WRITE OUTSIDE BUFFER.  pos=" << pos << " sbuf=" << sbuf << "\n";
        return std::string();
    }

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
     * ignore_encoding. the ZIP carver sets it to ZIP so it won't
     * carve things that are just found in a ZIP file. This means that
     * it won't carve disembodied ZIP files found in unallocated
     * space. You might want to do that.  If so, set ZIP's carve mode
     * to CARVE_ALL.
     */
    switch(carve_mode){
    case CARVE_NONE:
        return std::string();                         // carve nothing
    case CARVE_ENCODED:
        if(sbuf.pos0.path.size()==0) return std::string(); // not encoded
        if(sbuf.pos0.alphaPart()==ignore_encoding) return std::string(); // ignore if it is just encoded with this
        break;                                      // otherwise carve
    case CARVE_ALL:
        break;
    }

    /* If the directory doesn't exist, make it.
     * If two threads try to make the directory,
     * that's okay, because the second one will fail.
     */

#ifdef HAVE___SYNC_ADD_AND_FETCH
    uint64_t this_file_number = __sync_add_and_fetch(&file_number,1);
#else
    uint64_t this_file_number = 0;
    {
        cppmutex::lock lock(Mf);
        this_file_number = file_number++;
    }
#endif

    std::string dirname1 = outdir + "/" + name;
    std::stringstream ss;

    ss << dirname1 << "/" << std::setw(3) << std::setfill('0') << (this_file_number / 1000);

    std::string dirname2 = ss.str(); 
    std::string fname    = dirname2 + std::string("/") + valid_dosname(sbuf.pos0.str() + ext);
    std::string carved_hash_hexvalue = (*hasher.func)(sbuf.buf,sbuf.bufsize);

    /* Record what was found in the feature file.
     */
    ss.str(std::string()); // clear the stringstream
    ss << "<fileobject><filename>" << fname << "</filename><filesize>" << len << "</filesize>"
       << "<hashdigest type='" << hasher.name << "'>" << carved_hash_hexvalue << "</hashdigest></fileobject>";
    this->write(sbuf.pos0+len,fname,ss.str());
    
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
        cerr << "Could not make directory " << dirname2 << ": " << strerror(oerrno) << "\n";
        return std::string();
    }

    /* Write the file into the directory */
    int fd = ::open(fname.c_str(),O_CREAT|O_BINARY|O_RDWR,0666);
    if(fd<0){
        cerr << "*** carve: Cannot create " << fname << ": " << strerror(errno) << "\n";
        return std::string();
    }

    ssize_t ret = sbuf.write(fd,pos,len);
    if(ret<0){
        cerr << "*** carve: Cannot write(pos=" << fd << "," << pos << " len=" << len << "): "<< strerror(errno) << "\n";
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

/**
 * Tagging support.
 * Tags are a system of providing identifiers for regions of the input image.
 * Currently tags are just used for fragment type.
 */
void feature_recorder::write_tag(const pos0_t &pos0,size_t len,const string &tagName)
{
    if(flags & FLAG_DISABLED) return;           // disabled

    stringstream ss;
    string desc = pos0.alphaPart();

    // This allows you to set a breakpoint at a specific position
    // it could be a configurable variable, I guess...
#ifdef DEBUG_OFFSET
    if(len==DEBUG_OFFSET){
        std::cerr << "write_tag debug point pos0=" << pos0 << " len=" << len <<" name=" << tagName << "\n";
    }
#endif    

    /* offset is either the sbuf offset or the path offset */
    uint64_t offset = pos0.offset>0 ? pos0.offset : stoi64(pos0.path);

    /** Create what will got to the feature file */
    ss << offset << ":" << len << "\t";
    if(desc.size()>0) ss << desc << '/';
    ss << tagName;
    
    this->write(ss.str());
}

