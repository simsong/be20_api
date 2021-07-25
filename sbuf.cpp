/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */

// config.h needed solely for mmap
#include "config.h"

#include <fcntl.h>
#include <sys/stat.h>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <algorithm>

#include "sbuf.h"
#include "dfxml_cpp/src/hash_t.h"
#include "formatter.h"
#include "unicode_escape.h"

/****************************************************************
 *** SBUF_T
 *** Implement the sbuf abstraction, which is the primary buffer management
 *** tool of bulk_extractor. sbufs maintain memory management.
 *** Could this be done with a smart pointer?
 ****************************************************************/

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Keep track of how many sbufs we have */
std::atomic<int> sbuf_t::sbuf_count = 0;

/* Make an empty sbuf */
sbuf_t::sbuf_t()
{
    sbuf_count += 1;
}

/****************************************************************
 ** from an offset
 ****************************************************************/
// start at offset and get the rest of the sbuf as a child
sbuf_t::sbuf_t(const sbuf_t &src, size_t offset):
    pos0(src.pos0 + (offset < src.bufsize ? offset : src.bufsize)),
    bufsize( offset < src.bufsize ? src.bufsize - offset : 0),
    pagesize( offset < src.pagesize ? src.pagesize - offset : 0),
    flags(src.flags), parent(&src), buf(src.buf+offset)
{
    parent->add_child(*this);
    sbuf_count += 1;
}

// start at offset for a given len
sbuf_t::sbuf_t(const sbuf_t &src, size_t offset, size_t len):
    pos0(src.pos0 + (offset < src.bufsize ? offset : src.bufsize)),
    bufsize( offset + len < src.bufsize ? len : (offset > src.bufsize ? 0 : src.bufsize - offset)),
    pagesize( offset + len < src.pagesize ? len : (offset > src.pagesize ? 0 : src.pagesize - offset)),
    flags(src.flags), parent(&src), buf(src.buf+offset)
{
    parent->add_child(*this);
    sbuf_count += 1;
}



/* Core allocator used by all others */
sbuf_t::sbuf_t(pos0_t pos0_, const sbuf_t *parent_,
               const uint8_t* buf_, size_t bufsize_, size_t pagesize_,
               int fd_, flags_t flags_):
    pos0(pos0_), bufsize(bufsize_), pagesize(pagesize_),
    flags(flags_), fd(fd_), parent(parent_), buf(buf_)
{
    if (parent) {
        parent->add_child(*this);
    }
    sbuf_count += 1;
}


sbuf_t::~sbuf_t()
{
    if (children != 0) {
        std::runtime_error(Formatter() << "sbuf.cpp: error: sbuf children=" << children);
    }
    if (parent) parent->del_child(*this);
    if (fd>0) {
#ifdef HAVE_MMAP
        munmap((void*)buf, bufsize);
#else
        std::runtime_error(Formatter() << "sbuf.cpp: fd>0 and HAVE_MMAP is not defined");
#endif
        ::close(fd);
    }
    if (malloced != nullptr) {
        free( malloced );
    }
    sbuf_count -= 1;
}

const uint8_t* sbuf_t::get_buf() const
{
    return buf;
}

void* sbuf_t::malloc_buf() const
{
    if (malloced == nullptr ) {
        throw std::runtime_error("malloc_buf called on sbuf_t that was not malloced");
    }
    return malloced;
}

void sbuf_t::add_child(const sbuf_t& child) const
{
    children   += 1;
    references += 1;
}

void sbuf_t::del_child(const sbuf_t& child) const
{
    children   -= 1;
    assert(children >= 0);
    references -= 1;
    // I'm not sure how to delete us when there are no children left. Perhaps we could be added to a list to be wiped?
    //dereference();                      // child no longer has my reference
}

#if 0
void sbuf_t::dereference()
{
    references--;
    if (references==0) {
        delete this;
    }
}
#endif

/****************************************************************
 ** Allocators.
 ****************************************************************/

/* a new sbuf with the data from one but the pos from another.
 * Buffer is not freed when sbuf is deleted.
 */
sbuf_t* sbuf_t::sbuf_new(pos0_t pos0_, const uint8_t* buf_, size_t bufsize_, size_t pagesize_)
{
    return new sbuf_t(pos0_, nullptr, // pos0, parent
                      buf_, bufsize_, std::min(bufsize_,pagesize_), // buf, bufsize, pagesize
                      NO_FD, flags_t()); // fd, flags
}

/* Allocate from a string, copying the string into an allocated buffer, and automatically calling free(buf_) when the sbuf is deleted.
 * No space is left for terminating \0.
 */
sbuf_t* sbuf_t::sbuf_malloc(pos0_t pos0, const std::string &str)
{
    sbuf_t *ret = sbuf_t::sbuf_malloc(pos0,  str.size());
    memcpy( ret->malloc_buf(), str.c_str(), str.size());
    return ret;
}


/*
 * Similar to a move operator, but reallocates.
 */
sbuf_t *sbuf_t::realloc(size_t newsize)
{
    if (parent!=nullptr) {
        throw std::runtime_error("sbuf_t::realloc called on sbuf that has a parent.");
    }
    if (malloced==nullptr) {
        throw std::runtime_error("sbuf_t::realloc called on buffer that was not malloced");
    }
    if (newsize >bufsize) {
        throw std::runtime_error("sbuf_t::realloc attempt to make sbuf bigger");
    }
    malloced = ::realloc(malloced, newsize);
    if (malloced==nullptr) {
        throw std::bad_alloc();
    }
    sbuf_t *ret = new sbuf_t(pos0, nullptr,
                             static_cast<const uint8_t *>(malloced), newsize, newsize,
                             0, flags);
    ret->malloced = malloced;           // ret will delete it
    malloced = nullptr;                 // prevent double deletion
    delete this;                        // this is a move
    return ret;
}

/** Allocate a subset of an sbuf's memory to a child sbuf.
 * from within an existing sbuf.
 * The allocated buf MUST be freed before the parent, since no copy is made...
 */
sbuf_t *sbuf_t::new_slice(size_t off, size_t len) const
{
    if (off > bufsize) throw range_exception_t(); // check to make sure off is in the buffer
    if (off+len > bufsize) throw range_exception_t(); // check to make sure off+len is in the buffer

    size_t new_pagesize = pagesize;
    if (off > pagesize) {
        new_pagesize -= off;            // we only have this much left
    }
    if (new_pagesize > len) {
        new_pagesize = len;             // we only have this much left
    }

    return new sbuf_t(pos0 + off, highest_parent(),
                      buf + off, len, new_pagesize,
                      NO_FD, flags_t());
}

/** Copy a subset of an sbuf's memory to a child sbuf.
 */
sbuf_t *sbuf_t::new_slice_copy(size_t off, size_t len) const
{
    auto src  = slice(off,len);
    auto *dst = sbuf_t::sbuf_malloc(src.pos0, src.bufsize);
    memcpy( dst->malloc_buf(), src.buf, src.bufsize);
    return dst;
}

sbuf_t sbuf_t::slice(size_t off, size_t len) const
{
    if (off > bufsize) throw range_exception_t(); // check to make sure off is in the buffer
    if (off+len > bufsize) throw range_exception_t(); // check to make sure off+len is in the buffer

    size_t new_pagesize = pagesize;
    if (off > pagesize) {
        new_pagesize -= off;            // we only have this much left
    }
    if (new_pagesize > len) {
        new_pagesize = len;             // we only have this much left
    }

    return sbuf_t(pos0 + off, highest_parent(),
                      buf + off, len, new_pagesize,
                      NO_FD, flags_t());
}

sbuf_t *sbuf_t::new_slice(size_t off) const
{
    return new_slice(off, bufsize - off);
}

sbuf_t sbuf_t::slice(size_t off) const
{
    return slice(off, bufsize - off);
}


/**
 *  Map a file; falls back to read if mmap is not available
 */
std::string sbuf_t::map_file_delimiter(sbuf_t::U10001C);

/* Map a file when we are given an open fd.
 * The fd is not closed when the file is unmapped.
 * If there is no mmap, just allocate space and read the file
 */

sbuf_t* sbuf_t::map_file(const std::filesystem::path fname) {
    flags_t flags;
    struct stat st;
    int mfd = ::open(fname.c_str(), O_RDONLY);
    if (fstat(mfd, &st)) {
        close(mfd);
        throw std::filesystem::filesystem_error("fstat", std::error_code(errno, std::generic_category()));
    }

#ifdef HAVE_MMAP
    uint8_t* mbuf = (uint8_t*)mmap(0, st.st_size, PROT_READ, MAP_FILE | MAP_SHARED, mfd, 0);
#else
    mmalloced = malloc(st.st_size);
    if (mmalloced == nullptr) { /* malloc failed */
        return 0;
    }
    uint8_t* mbuf = static_cast<uint8_t*>(malloced);
    lseek(mfd, 0, SEEK_SET); // go to beginning of file
    size_t r = (size_t)read(mfd, (void*)mbuf, st.st_size);
    if (r != (size_t)st.st_size) {
        free(malloced); /* read failed */
        return 0;
    }
    close(mfd);
    mfd = NO_FD;
#endif
    return new sbuf_t(pos0_t(fname.string() + sbuf_t::map_file_delimiter), nullptr,
                      mbuf, st.st_size, st.st_size,
                      mfd, flags);
}


/*
 * Allocate a new sbuf with a malloc and return a writable buffer.
 * In the future we will add guard bytes. Byte 0 is at pos0.
 * There's no parent, because this sbuf owns the memory.
 */
sbuf_t* sbuf_t::sbuf_malloc(pos0_t pos0_, size_t len_)
{
    void *new_malloced = malloc(len_);
    sbuf_t *ret = new sbuf_t(pos0_, nullptr,
                             static_cast<const uint8_t *>(new_malloced), len_, len_, NO_FD, flags_t());
    ret->malloced = new_malloced;
    ret->buf_writable = static_cast<uint8_t *>(new_malloced);
    assert(ret->buf == ret->malloced);
    assert(ret->buf == ret->buf_writable);
    return ret;
}

void sbuf_t::wbuf(size_t i, uint8_t val)
{
    if ( buf_writable==nullptr) {
        throw std::runtime_error("Attempt to write to unwritable sbuf");
    }
    if ( i<0 ){
        throw std::runtime_error("Attempt to write sbuf i<0");
    }
    if ( i>bufsize ){
        throw std::runtime_error("Attempt to write sbuf i>bufsize");
    }
    buf_writable[i] = val;
}

/**
 * rawdump the sbuf to an ostream.
 */
void sbuf_t::raw_dump(std::ostream& os, uint64_t start, uint64_t len) const {
    for (uint64_t i = start; i < start + len && i < bufsize; i++) { os << buf[i]; }
}

/**
 * rawdump the sbuf to a file descriptor
 */
void sbuf_t::raw_dump(int fd2, uint64_t start, uint64_t len) const {
    if (len > bufsize - start) len = bufsize - start; // maximum left
    uint64_t written = ::write(fd2, buf + start, len);
    if (written != len) {
        std::cerr << "write: cannot write sbuf.\n";
    }
}

static std::string hexch(unsigned char ch) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02x", ch);
    return std::string(buf);
}

/**
 * hexdump the sbuf.
 */
void sbuf_t::hex_dump(std::ostream& os, uint64_t start, uint64_t len) const {
    const size_t bytes_per_line = 32;
    size_t max_spaces = 0;
    for (uint64_t i = start; i < start + len && i < bufsize; i += bytes_per_line) {
        size_t spaces = 0;

        /* Print the offset */
        char b[64];
        snprintf(b, sizeof(b), "%04x: ", (int)i);
        os << b;
        spaces += strlen(b);

        for (size_t j = 0; j < bytes_per_line && i + j < bufsize && i + j < start + len; j++) {
            unsigned char ch = (*this)[i + j];
            os << hexch(ch);
            spaces += 2;
            if (j % 2 == 1) {
                os << " ";
                spaces += 1;
            }
        }
        if (spaces > max_spaces) max_spaces = spaces;
        for (; spaces < max_spaces; spaces++) { os << ' '; }
        for (size_t j = 0; j < bytes_per_line && i + j < bufsize && i + j < start + len; j++) {
            unsigned char ch = (*this)[i + j];
            if (ch >= ' ' && ch <= '~')
                os << ch;
            else
                os << '.';
        }
        os << "\n";
    }
}

/* Write to a file descriptor */
ssize_t sbuf_t::write(int fd_, size_t loc, size_t len) const {
    if (loc >= bufsize) return 0;                 // cannot write
    if (loc + len > bufsize) len = bufsize - loc; // clip at the end
    return ::write(fd_, buf + loc, len);
}

/* Write to a FILE */
ssize_t sbuf_t::write(FILE* f, size_t loc, size_t len) const {
    if (loc >= bufsize) return 0;                 // cannot write
    if (loc + len > bufsize) len = bufsize - loc; // clip at the end
    return ::fwrite(buf + loc, 1, len, f);
}

/* Write to an output stream */
ssize_t sbuf_t::write(std::ostream& os) const {
    os.write(reinterpret_cast<const char*>(buf), bufsize);
    if (os.bad()) { return 0; }
    return bufsize;
}

/* Write to path */
void sbuf_t::write(std::filesystem::path path) const {
    std::ofstream os;
    os.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!os.is_open()) {
        perror(path.c_str());
        throw std::runtime_error(Formatter() << "cannot open file for writing:" << path);
    }
    this->write(os);
    os.close();
    if (os.bad()) { throw std::runtime_error(Formatter() << "error writing file " << path); }
}

/* Return a substring */
const std::string sbuf_t::substr(size_t loc, size_t len) const {
    if (loc >= bufsize) return std::string("");   // cannot write
    if (loc + len > bufsize) len = bufsize - loc; // clip at the end
    return std::string((const char*)buf + loc, len);
}

bool sbuf_t::is_constant(size_t off, size_t len, uint8_t ch) const // verify that it's constant
{
    while (len > 0) {
        if (((*this)[off]) != ch) return false;
        off++;
        len--;
    }
    return true;
}

void sbuf_t::hex_dump(std::ostream& os) const { hex_dump(os, 0, bufsize); }

/**
 * Convert a binary blob to a hex representation
 */

#ifndef NSRL_HEXBUF_UPPERCASE
#define NSRL_HEXBUF_UPPERCASE 0x01
#define NSRL_HEXBUF_SPACE2 0x02
#define NSRL_HEXBUF_SPACE4 0x04
#endif

/* Determine if the sbuf consists of a repeating ngram */
size_t sbuf_t::find_ngram_size(const size_t max_ngram) const {
    for (size_t ngram_size = 1; ngram_size < max_ngram; ngram_size++) {
        bool ngram_match = true;
        for (size_t i = ngram_size; i < pagesize && ngram_match; i++) {
            if ((*this)[i % ngram_size] != (*this)[i]) ngram_match = false;
        }
        if (ngram_match) return ngram_size;
    }
    return 0; // no ngram size
}

bool sbuf_t::getline(size_t& pos, size_t& line_start, size_t& line_len) const
{
    /* Scan forward until pos is at the beginning of a line */
    if (pos >= this->pagesize) return false;
    if (pos > 0) {
        while ((pos < this->pagesize) && (*this)[pos - 1] != '\n') { ++(pos); }
        if (pos >= this->pagesize) return false; // didn't find another start of a line
    }
    line_start = pos;
    /* Now scan to end of the line, or the end of the buffer */
    while (++pos < this->pagesize) {
        if ((*this)[pos] == '\n') { break; }
    }
    line_len = (pos - line_start);
    return true;
}

ssize_t sbuf_t::find(uint8_t ch, size_t start) const
{
    for (; start < pagesize; start++) {
        if (buf[start] == ch) return start;
    }
    return -1;
}

ssize_t sbuf_t::find(const char* str, size_t start ) const
{
    if (str[0] == 0) return -1; // nothing to search for

    for (; start < pagesize; start++) {
        const uint8_t* p = (const uint8_t*)memchr(buf + start, str[0], bufsize - start);
        if (p == 0) return -1; // first character not present,
        size_t loc = p - buf;
        for (size_t i = 0; loc + i < bufsize && str[i]; i++) {
            if (buf[loc + i] != str[i]) break;
            if (str[i + 1] == 0) return loc; // next char is null, so we are found!
        }
        start = loc + 1; // advance to character after found character
    }
    return -1;
}

std::ostream& operator<<(std::ostream& os, const sbuf_t& t) {
    os << "sbuf[pos0=" << t.pos0 << " " << "buf[0..8]=";

    for (size_t i=0; i < std::min( 8UL, t.bufsize); i++){
        os << hexch(t[i]) << ' ';
    }
    os << " (" ;
    for (size_t i=0; i < std::min( 8UL, t.bufsize); i++){
        if (isprint(t[i])) {
            os << t[i];
        }
    }
    os << " )" ;
    os << " buf= "          << static_cast<const void *>(t.buf)
       << " malloced= "     << static_cast<const void *>(t.malloced)
       << " buf_writable= " << static_cast<const void *>(t.buf_writable)
       << " bufsize="       << t.bufsize
       << " pagesize="      << t.pagesize
       << " children="      << t.children
       << " references="    << t.references
       << " fd="            << t.fd
       << " depth="         << t.depth()
       << "]";
    return os;
}

/**
 * Read the requested number of UTF-8 format string octets including any \0.
 */
void sbuf_t::getUTF8(size_t i, size_t num_octets_requested, std::string& utf8_string) const {
    // clear any residual value
    utf8_string = "";

    if (i >= bufsize) {
        // past EOF
        return;
    }
    if (i + num_octets_requested > bufsize) {
        // clip at EOF
        num_octets_requested = bufsize - i;
    }
    utf8_string = std::string((const char*)buf + i, num_octets_requested);
}

/**
 * Read UTF-8 format code octets into string up to but not including \0.
 */
void sbuf_t::getUTF8(size_t i, std::string& utf8_string) const {
    // clear any residual value
    utf8_string = "";

    // read octets
    for (size_t off = i; off < bufsize; off++) {
        uint8_t octet = get8u(off);

        // stop before \0
        if (octet == 0) {
            // at \0
            break;
        }

        // accept the octet
        utf8_string.push_back(octet);
    }
}

/**
 * Read the requested number of UTF-16 format code units into wstring including any \U0000.
 */
void sbuf_t::getUTF16(size_t i, size_t num_code_units_requested, std::wstring& utf16_string) const {
    // clear any residual value
    utf16_string = std::wstring();

    if (i >= bufsize) {
        // past EOF
        return;
    }
    if (i + num_code_units_requested * 2 + 1 > bufsize) {
        // clip at EOF
        num_code_units_requested = ((bufsize - 1) - i) / 2;
    }
    // NOTE: we can't use wstring constructor because we require 16 bits,
    // not whatever sizeof(wchar_t) is.
    // utf16_string = std::wstring((const char *)buf+i,num_code_units_requested);

    // get code units individually
    for (size_t j = 0; j < num_code_units_requested; j++) { utf16_string.push_back(get16u(i + j * 2)); }
}

/**
 * Read UTF-16 format code units into wstring up to but not including \U0000.
 */
void sbuf_t::getUTF16(size_t i, std::wstring& utf16_string) const {
    // clear any residual value
    utf16_string = std::wstring();

    // read the code units
    size_t off;
    for (off = i; off < bufsize - 1; off += 2) {
        uint16_t code_unit = get16u(off);
        // cout << "sbuf.cpp getUTF16 i: " << i << " code unit: " << code_unit << "\n";

        // stop before \U0000
        if (code_unit == 0) {
            // at \U0000
            break;
        }

        // accept the code unit
        utf16_string.push_back(code_unit);
    }
}

/**
 * Read the requested number of UTF-16 format code units using the specified byte order into wstring including any
 * \U0000.
 */
void sbuf_t::getUTF16(size_t i, size_t num_code_units_requested, byte_order_t bo, std::wstring& utf16_string) const {
    // clear any residual value
    utf16_string = std::wstring();

    if (i >= bufsize) {
        // past EOF
        return;
    }
    if (i + num_code_units_requested * 2 + 1 > bufsize) {
        // clip at EOF
        num_code_units_requested = ((bufsize - 1) - i) / 2;
    }
    // NOTE: we can't use wstring constructor because we require 16 bits,
    // not whatever sizeof(wchar_t) is.
    // utf16_string = std::wstring((const char *)buf+i,num_code_units_requested);

    // get code units individually
    for (size_t j = 0; j < num_code_units_requested; j++) { utf16_string.push_back(get16u(i + j, bo)); }
}

/**
 * Read UTF-16 format code units using the specified byte order into wstring up to but not including \U0000.
 */
void sbuf_t::getUTF16(size_t i, byte_order_t bo, std::wstring& utf16_string) const {
    // clear any residual value
    utf16_string = std::wstring();

    // read the code units
    size_t off;
    for (off = i; off < bufsize - 1; off += 2) {
        uint16_t code_unit = get16u(off, bo);
        // cout << "sbuf.cpp getUTF16 i: " << i << " code unit: " << code_unit << "\n";

        // stop before \U0000
        if (code_unit == 0) {
            // at \U0000
            break;
        }

        // accept the code unit
        utf16_string.push_back(code_unit);
    }
}

std::string sbuf_t::hash() const {
    const std::lock_guard<std::mutex> lock(Mhash); // protect this function
    if (hash_.size() == 0) {
        /* hasn't been hashed yet, so hash it */
        hash_ = dfxml::sha1_generator::hash_buf(buf, bufsize).hexdigest();
    }
    return hash_;
}

/* Similar to above, but does not cache, so it is inherently threadsafe */
std::string sbuf_t::hash(hash_func_t func) const {
    return func(buf, bufsize);
}

/* Report if the hash exists */
bool sbuf_t::has_hash() const {
    const std::lock_guard<std::mutex> lock(Mhash); // protect this function}
    return (hash_.size() > 0 );
}
