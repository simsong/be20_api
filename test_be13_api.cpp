/*
 * all be13_api test cases are in this file.
 * The goal is to have complete test coverage of the v2 API
 *
 * Todo list:
 * - test an individual feature recorder (they can exist without scanners)
 * - test a feature recorder set with two feature recorders.
 * - create a generic scanner that counts the number of empty pages.
 * - test the code for creating a scanner set that registering and enabling the scanner.
 * - test code for running the single scanner set.
 */


// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_MAIN
#include "config.h"                     // supposed to come after bulk_extractor_i.h


#include "catch.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <iostream>
#include <filesystem>

#include "atomic_unicode_histogram.h"
#include "sbuf.h"

// https://inversepalindrome.com/blog/how-to-create-a-random-string-in-cpp
std::string random_string(std::size_t length)
{
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;

    for (std::size_t i = 0; i < length; ++i)
    {
        random_string += CHARACTERS[distribution(generator)];
    }

    return random_string;
}

std::string get_tempdir()
{
    std::string tempdir = std::filesystem::temp_directory_path().string();
    if (tempdir.back()!='/'){
        tempdir += '/';
    }
    tempdir += random_string(8);
    std::filesystem::create_directory(tempdir);
    return tempdir;
}

#ifdef HAVE_MACH_O_DYLD_H
#include <mach-o/dyld.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

std::string get_exe()
{
    char rpath[PATH_MAX];
#ifdef HAVE_MACH_O_DYLD_H
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) ==0){
        realpath(path, rpath);
    } else {
        throw std::runtime_error("bufsize too small");
    }
#else
    readlink("/proc/self/exe",rpath, sizeof(rpath));
#endif
    return std::string(rpath);
}

std::string tests_dir()
{
    std::filesystem::path p( get_exe() );
    return  p.parent_path().string() + "/tests";
}


const char *hello="Hello world!";
const char *hello_sha1="d3486ae9136e7856bc42212385ea797094475802";
const uint8_t *hello_buf = reinterpret_cast<const uint8_t *>(hello);
const sbuf_t hello_sbuf() {
    pos0_t p0("hello");
    return sbuf_t(p0, hello_buf, strlen(hello), strlen(hello), 0, false, false, false);
}

const char *hello16="H\000e\000l\000l\000o\000 \000w\000o\000r\000l\000d\000!\000";
const uint8_t *hello16_buf = reinterpret_cast<const uint8_t *>(hello16);
const sbuf_t hello16_sbuf() {
    pos0_t p0("hello16");
    return sbuf_t(p0, hello16_buf, strlen(hello)*2, strlen(hello)*2, 0, false, false, false);
}


/****************************************************************
 * aftimer.h
 */
#include "aftimer.h"
TEST_CASE("aftimer", "[utils]") {
    aftimer t;
    REQUIRE( int(t.elapsed_seconds()) == 0);
}


/****************************************************************
 * atomic_set_map.h
 */
TEST_CASE( "atomic_set", "[histogram]" ){
    atomic_set<std::string> as;
    //REQUIRE( as.size() == 0);
    as.insert("one");
    as.insert("two");
    as.insert("three");
    REQUIRE( as.contains("one") == true );
    REQUIRE( as.contains("four") == false );
    //REQUIRE( as.size() == 3);
}

TEST_CASE( "atomic_map", "[histogram]" ){
    atomic_map<std::string,int> am;
    am.insert("one",1);
    am.insert("two",2);
    am.insert("three",3);
    am.insertIfNotContains("three",30);
    am.insertIfNotContains("four",4);
    REQUIRE( am.contains("one") == true );
    REQUIRE( am.contains("two") == true );
    REQUIRE( am.contains("three") == true );
    REQUIRE( am.contains("four") == true );
    REQUIRE( am.contains("five") == false );
    REQUIRE( am["one"]==1 );
    REQUIRE( am["three"]==3 );
}

/****************************************************************
 * atomic_unicode_histogram.h
 */
#include "atomic_unicode_histogram.h"
#include "histogram_def.h"
TEST_CASE( "atomic_histogram", "[histogram]" ){
    histogram_def d1("name","(.*)","suffix1",histogram_def::flags_t());
    AtomicUnicodeHistogram h(d1);
    h.add("foo");
    h.add("foo");
    h.add("foo");
    h.add("bar");

    /* Now make sure things were added in the right order, and the right counts */
    //AtomicUnicodeHistogram::FrequencyReportVector f = h.makeReport();
    //REQUIRE( f.at(0)->value=="foo");
    //REQUIRE( f.at(0)->tally.count==3);
    //REQUIRE( f.at(0)->tally.count16==0);
}


/****************************************************************
 * hash_t.h
 */
#include "dfxml/src/hash_t.h"
static std::string hash_name("sha1");
static std::string hash_func(const uint8_t *buf,size_t bufsize)
{
    if(hash_name=="md5" || hash_name=="MD5"){
        return dfxml::md5_generator::hash_buf(buf,bufsize).hexdigest();
    }
    if(hash_name=="sha1" || hash_name=="SHA1" || hash_name=="sha-1" || hash_name=="SHA-1"){
        return dfxml::sha1_generator::hash_buf(buf,bufsize).hexdigest();
    }
    if(hash_name=="sha256" || hash_name=="SHA256" || hash_name=="sha-256" || hash_name=="SHA-256"){
        return dfxml::sha256_generator::hash_buf(buf,bufsize).hexdigest();
    }
    std::cerr << "Invalid hash name: " << hash_name << "\n";
    std::cerr << "This version of bulk_extractor only supports MD5, SHA1, and SHA256\n";
    exit(1);
}

TEST_CASE("sha1", "[hash]") {
    REQUIRE( hash_func(reinterpret_cast<const uint8_t *>(hello), strlen(hello))==hello_sha1);
}

/****************************************************************
 * feature_recorder.h
 */
#include "scanner_config.h"
#include "feature_recorder.h"
#include "feature_recorder_set.h"
TEST_CASE("quote_if_necessary", "[feature_recorder]") {
    std::string f1("feature");
    std::string c1("context");

    feature_recorder_set::flags_t flags;
    flags.no_alert = true;

    feature_recorder_set frs( flags, "sha1", scanner_config::NO_INPUT, scanner_config::NO_OUTDIR);
    frs.named_feature_recorder("test", true);
    feature_recorder &ft = frs.named_feature_recorder("test");
    ft.quote_if_necessary(f1,c1);
    REQUIRE( f1=="feature" );
    REQUIRE( c1=="context" );
}

TEST_CASE("fname_in_outdir", "[feature_recorder]") {
    feature_recorder_set::flags_t flags;
    flags.no_alert = true;
    feature_recorder_set frs( flags, "sha1", scanner_config::NO_INPUT, get_tempdir());
    frs.named_feature_recorder("test", true);
    feature_recorder &ft = frs.named_feature_recorder("test");

    const std::string n = ft.fname_in_outdir("foo", feature_recorder::NO_COUNT);
    std::filesystem::path p(n);
    REQUIRE( p.filename()=="test_foo.txt");

    const std::string n1 = ft.fname_in_outdir("bar", feature_recorder::NEXT_COUNT);
    std::filesystem::path p1(n1);
    REQUIRE( p1.filename()=="test_bar_1.txt");

    const std::string n2 = ft.fname_in_outdir("bar", feature_recorder::NEXT_COUNT);
    std::filesystem::path p2(n2);
    REQUIRE( p2.filename()=="test_bar_2.txt");
}

/* Read all of the lines of a file and return them as a vector */
std::vector<std::string> getLines(const std::string &filename)
{
    std::vector<std::string> lines;
    std::string line;
    std::ifstream inFile;
    inFile.open(filename.c_str());
    if (!inFile.is_open()) {
        throw std::runtime_error("Cannot open file: "+filename);
    }
    while (std::getline(inFile, line)){
        if (line.size()>0){
            lines.push_back(line);
        }
    }
    return lines;
}

/****************************************************************
 * feature_recorder_set.h
 *
 * Create a simple set and write two features and make a histogram.
 */
TEST_CASE("write_features", "[feature_recorder_set]" ) {

    // Create a random directory for the output of the feature recorder
    std::string tempdir = get_tempdir();
    {
        feature_recorder_set::flags_t flags;
        flags.no_alert = true;
        flags.debug    = true;

        feature_recorder_set fs( flags, "sha1", scanner_config::NO_INPUT, tempdir);
        feature_recorder &fr = fs.named_feature_recorder("test", true);

        /* Make sure requesting a valid name does not generate an exception and an invalid name generates an exception */
        REQUIRE_NOTHROW( fs.named_feature_recorder("test") );
        REQUIRE_THROWS_AS( fs.named_feature_recorder("test_not"), feature_recorder_set::NoSuchFeatureRecorder);

        /* Ask the feature recorder to create a histogram */
        histogram_def h1("name","([0-9]+)","suffix1",histogram_def::flags_t());
        REQUIRE( fs.histogram_count() == 0);
        fs.histogram_add(h1);
        REQUIRE( fs.histogram_count() == 1);


        pos0_t p;
        fr.write(p,    "one", "context");
        fr.write(p+5,  "one", "context");
        fr.write(p+10, "two", "context");

        sbuf_t sb16 = hello16_sbuf();
        REQUIRE( sb16.size() == strlen(hello)*2 );

        fr.write_buf(sb16, 0, 64); // write the entire buffer as a single feature, no context

        std::ofstream o(fs.get_outdir() + "/histogram1.txt");
        //fr.generate_histogram(o, h1);

    }
#if 0
    /* get the last line of the test file and see if it is correct */
    std::string expected_lastline {"hello16-0\t"
        "H\\x00e\\x00l\\x00l\\x00o\\x00 \\x00w\\x00o\\x00r\\x00l\\x00d\\x00!\\x00"
        "\tH\\x00e\\x00l\\x00l\\x00o\\x00 \\x00w\\x00o\\x00r\\x00l\\x00d\\x00!\\x00"};
    std::vector<std::string> lines = getLines(tempdir+"/test.txt");

    REQUIRE( lines.back() == expected_lastline);
#endif
}

/****************************************************************
 * char_class.h
 */
#include "char_class.h"
TEST_CASE( "char_class", "[char_class]") {
    CharClass c;
    c.add('a');
    c.add('0');
    REQUIRE( c.range_A_Fi == 1);
    REQUIRE( c.range_g_z == 0);
    REQUIRE( c.range_G_Z == 0);
    REQUIRE( c.range_0_9 == 1);

    c.add(reinterpret_cast<const uint8_t *>("ab"),2);
    REQUIRE( c.range_A_Fi == 3);
    REQUIRE( c.range_g_z == 0);
    REQUIRE( c.range_G_Z == 0);
    REQUIRE( c.range_0_9 == 1);
}

/****************************************************************
 * histogram_def.h
 */
#include "histogram_def.h"
TEST_CASE( "histogram_def", "[histogram]" ){
    histogram_def h1("feature1","pattern1","suffix1",histogram_def::flags_t());
    histogram_def h2("feature2","pattern2","suffix2",histogram_def::flags_t());

    REQUIRE( h1 == h1);
    REQUIRE( h1 != h2);
    REQUIRE( h1 < h2);
}

/****************************************************************
 * atomic_unicode_histogram.h
 */
#include "atomic_unicode_histogram.h"
TEST_CASE( "atomic_unicode_histogram", "[histogram]") {
    /* Make sure that the histogram elements work */
    AtomicUnicodeHistogram::auh_t::AMReportElement e1("hello");
    AtomicUnicodeHistogram::auh_t::AMReportElement e2("world");

    REQUIRE( e1 == e1 );
    REQUIRE( e1 != e2);
    REQUIRE( e1 < e2);

    /* Try a simple histogram with just numbers */
    histogram_def::flags_t flags;
    flags.numeric = true;
    histogram_def h1("p","([0-9]+)","phones",flags);
    AtomicUnicodeHistogram hm(h1);

    hm.add("100");
    hm.add("200");
    hm.add("300");
    hm.add("200");
    hm.add("300");
    hm.add("foo 300 bar");

    AtomicUnicodeHistogram::auh_t::report r = hm.makeReport(0);
    REQUIRE( r.size() == 3);
    REQUIRE( r.at(0).key == "100");
    REQUIRE( r.at(0).value.count == 1);
    REQUIRE( r.at(0).value.count16 == 0);

    REQUIRE( r.at(1).key == "200");
    REQUIRE( r.at(1).value.count == 2);
    REQUIRE( r.at(1).value.count16 == 0);

    REQUIRE( r.at(2).key == "300");
    REQUIRE( r.at(2).value.count == 3);
    REQUIRE( r.at(2).value.count16 == 0);

    char buf300[6] {'\000','3','\000','0','\000','0'};
    std::string b300(buf300,6);
    hm.add(b300);
    r = hm.makeReport(0);
    REQUIRE( r.at(2).key == "300");
    REQUIRE( r.at(2).value.count == 4);
    REQUIRE( r.at(2).value.count16 == 1);
}


/****************************************************************
 *
 * pos0.h:
 */

#include "pos0.h"
TEST_CASE( "pos0_t", "[vector]" ){
    REQUIRE( stoi64("12345") == 12345);

    pos0_t p0("10000-hello-200-bar",300);
    pos0_t p1("10000-hello-200-bar",310);
    pos0_t p2("10000-hello-200-bar",310);
    REQUIRE( p0.path == "10000-hello-200-bar" );
    REQUIRE( p0.offset == 300);
    REQUIRE( p0.isRecursive() == true);
    REQUIRE( p0.firstPart() == "10000" );
    REQUIRE( p0.lastAddedPart() == "bar" );
    REQUIRE( p0.alphaPart() == "hello/bar" );
    REQUIRE( p0.imageOffset() == 10000 );
    REQUIRE( p0 + 10 == p1);
    REQUIRE( p0 < p1 );
    REQUIRE( p1 > p0 );
    REQUIRE( p0 != p1 );
    REQUIRE( p1 == p2 );
}


/****************************************************************
 * regex_vector.h & regex_vector.cpp
 */
#include "regex_vector.h"
TEST_CASE( "test regex_vector", "[vector]" ) {
    REQUIRE( regex_vector::has_metachars("this[1234]foo") == true );
    REQUIRE( regex_vector::has_metachars("this(1234)foo") == true );
    REQUIRE( regex_vector::has_metachars("this[1234].*foo") == true);
    REQUIRE( regex_vector::has_metachars("this1234foo") == false);

    regex_vector rv;
    rv.push_back("this.*");
    rv.push_back("check[1-9]");
    rv.push_back("thing");

    REQUIRE( rv.size() == 3);

    std::string found;
    REQUIRE(rv.search_all("hello1", &found) == false);
    REQUIRE(rv.search_all("check1", &found) == true);
    REQUIRE(found == "check1");

    REQUIRE(rv.search_all("before check2 after", &found) == true);
    REQUIRE(found == "check2");
}

/****************************************************************
 *
 * sbuf.h
 */


TEST_CASE("hello_sbuf","[sbuf]") {
    sbuf_t sb1 = hello_sbuf();
    REQUIRE( sb1.size()==strlen(hello));
    REQUIRE( sb1.offset(&hello_buf[2]) == 2);
    REQUIRE( sb1.asString() == std::string("Hello world!"));
    REQUIRE( sb1.get8uBE(0) == 'H');
    REQUIRE( sb1.find('o', 0) == 4);
    REQUIRE( sb1.find("world") == 6);

    std::string s;
    sb1.getUTF8(6, 5, s);
    REQUIRE(s == "world");
}

TEST_CASE("map_file","[sbuf]") {
    std::string tempdir = get_tempdir();
    std::ofstream os;
    std::string fname = tempdir+"/hello.txt";

    os.open( fname );
    REQUIRE( os.is_open() );
    os << hello;
    os.close();


    sbuf_t sb1 = sbuf_t::map_file(fname);
    REQUIRE( sb1.bufsize == strlen(hello));
    REQUIRE( sb1.bufsize == sb1.pagesize);
    for(int i=0;hello[i];i++){
        REQUIRE( hello[i] == sb1[i] );
    }
    REQUIRE( sb1[-1] == '\000' );
    REQUIRE( sb1[1000] == '\000' );
}



/****************************************************************
 * scanner_config.h:
 * holds the name=value configurations for all scanners.
 * A scanner_config is required to make a scanner_set.
 * The scanner_set then loads the scanners and creates a feature recorder set, which
 * holds the feature recorders. THey are called with the default values.
 */
#include "scanner_config.h"
TEST_CASE("scanner_config", "[scanner_config]") {
    std::string help_string {"   -S first-day=sunday    value for first-day (first-day)\n"
                             "   -S age=0    age in years (age)\n"};
    scanner_config sc;
    /* Normally the set_configs would be called by main()
    * These two would be called by -S first-day=monday  -S age=5
    */
    sc.set_config("first-day","monday");
    sc.set_config("age","5");

    std::string val {"sunday"};
    sc.get_config("first-day", &val, "value for first-day");
    REQUIRE(val == "monday");

    uint64_t ival {0};
    sc.get_config("age", &ival, "age in years");
    REQUIRE(ival == 5);

    REQUIRE(sc.help() == help_string);
}

/****************************************************************
 * scanner_params.h:
 * The interface used by scanners.
 */
#include "scanner_params.h"
TEST_CASE("scanner", "[scanner_params]") {
    /* check that scanner params made from an existing scanner params are deeper */
}

/****************************************************************
 * scanner_set.h:
 * Creates a place to hold all of the scanners.
 * The be13_api contains a single scanner for testing purposes:
 * scan_null, the null scanner, which writes metadata into a version.txt feature file.
 */
#include "scanner_set.h"
#include "scan_sha1.h"
TEST_CASE("enable/disable", "[scanner_set]") {
    scanner_config sc;
    sc.outdir = get_tempdir();
    sc.hash_alg = "sha1";

    struct feature_recorder_set::flags_t f;
    scanner_set ss(sc, f);
    ss.add_scanner(scan_sha1);


    SECTION(" Make sure that the scanner was added ") {
        REQUIRE_NOTHROW( ss.get_scanner_by_name("sha1") );
        REQUIRE( ss.get_scanner_by_name("sha1") == scan_sha1 );
        REQUIRE_THROWS_AS(ss.get_scanner_by_name("no_such_scanner"), scanner_set::NoSuchScanner );
    }

    /* Enable the scanner */
    ss.set_scanner_enabled("sha1", true);

    SECTION("Make sure that the sha1 scanner is enabled and only the sha1 scanner is enabled") {
        REQUIRE( ss.is_scanner_enabled("sha1") == true );
        REQUIRE( ss.is_find_scanner_enabled() == false); // only sha1 scanner is enabled
    }

    SECTION("Turn it off and make sure that it's disabled") {
        ss.set_scanner_enabled("sha1", false);
        REQUIRE( ss.is_scanner_enabled("sha1") == false );
    }

    SECTION("Try enabling all ") {
        ss.set_scanner_enabled(scanner_set::ALL_SCANNERS,true);
        REQUIRE( ss.is_scanner_enabled("sha1") == true );
    }

    SECTION("  /* Try disabling all ") {
        ss.set_scanner_enabled(scanner_set::ALL_SCANNERS,false);
        REQUIRE( ss.is_scanner_enabled("sha1") == false );
    }
}

/* This test runs a scan on the hello_sbuf() with the sha1 scanner. */
TEST_CASE("run", "[scanner_set]") {
    scanner_config sc;
    sc.outdir = get_tempdir();
    sc.hash_alg = "sha1";               // it's faster than SHA1!

    struct feature_recorder_set::flags_t f;
    scanner_set ss(sc, f);
    ss.add_scanner(scan_sha1);
    ss.set_scanner_enabled("sha1", true); /* Turn it onn */

    /* Might as well use it! */
    ss.phase_scan();                    // start the scanner phase
    ss.process_sbuf( hello_sbuf() );
    ss.process_histograms();
    ss.shutdown(nullptr);
    /* The sha1 scanner makes a single histogram. Make sure we got it. */
    //REQUIRE( ss.histogram_count() == 1);
}


/****************************************************************
 *  word_and_context_list.h
 */
#include "word_and_context_list.h"
TEST_CASE("word_and_context_list", "[vector]") {
    REQUIRE( word_and_context_list::rstrcmp("aaaa1", "bbbb0") < 0 );
    REQUIRE( word_and_context_list::rstrcmp("aaaa1", "aaaa1") == 0 );
    REQUIRE( word_and_context_list::rstrcmp("bbbb0", "aaaa1") > 0 );
}


/****************************************************************
 * unicode_escape.h
 * unicode_escape.cpp
 * Our test suite for the utf8 package and our uses of it.
 */
#include "unicode_escape.h"
TEST_CASE("unicode_escape", "[unicode]") {
    const char *U1F601 = "\xF0\x9F\x98\x81";        // UTF8 for Grinning face with smiling eyes
    REQUIRE( hexesc('a') == "\\x61");               // escape the escape!
    REQUIRE( utf8cont('a') == false );
    REQUIRE( utf8cont( U1F601[0] ) == false); // the first character is not a continuation character
    REQUIRE( utf8cont( U1F601[1] ) == true);  // the second is
    REQUIRE( utf8cont( U1F601[2] ) == true);  // the third is
    REQUIRE( utf8cont( U1F601[3] ) == true);  // the fourth is
    REQUIRE( valid_utf8codepoint(0x01) == true);
    REQUIRE( valid_utf8codepoint(0xffff) == false);

    std::string hellos("hello");
    /* Try all combinations with valid UTF8 */
    for(int a=0;a<2;a++){
        for(int b=0;b<2;b++){
            for(int c=0;c<2;c++) {
                REQUIRE( validateOrEscapeUTF8(hellos, a, b, c) == hellos);
            }
        }
    }
    REQUIRE( validateOrEscapeUTF8("backslash=\\", false, true, false) == "backslash=\\x5C");
}

TEST_CASE("unicode_detection", "[unicode]") {
    sbuf_t sb16 = sbuf_t::map_file(tests_dir() + "/unilang.htm");

    bool little_endian = false;
    bool t16 = looks_like_utf16(sb16.asString(), little_endian);
    REQUIRE( t16 == true);
    REQUIRE( little_endian == true);

    sbuf_t sb8 = sbuf_t::map_file(tests_dir() + "/unilang8.htm");
    bool t8 = looks_like_utf16(sb8.asString(), little_endian);
    REQUIRE( t8 == false);
}
