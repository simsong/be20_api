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
#include "atomic_set_map.h"
int dump_cb(void *user, const std::string &val, const int &count){
    int *called = (int *)user;
    switch (*called) {
    case 0:
        REQUIRE( val == "bar" );
        REQUIRE( count == 10 );
        break;
    case 1:
        REQUIRE( val == "foo" );
        REQUIRE( count == 6 );
        break;
    default:
        REQUIRE( false );
    }
    (*called) += 1;
    return 0;
}

TEST_CASE( "test atomic_histogram", "[vector]" ){
    atomic_histogram<std::string, int> ahist;
    ahist.add("foo", 1);
    ahist.add("foo", 2);
    ahist.add("foo", 3);
    ahist.add("bar", 10);

    /* Now make sure things were added in the right order, and the right counts */
    int called = 0;
    ahist.dump_sorted( &called, dump_cb );
    REQUIRE( called == 2);
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

TEST_CASE("hash_func", "[sha1]") {
    const char *hello="hello";
    REQUIRE( hash_func(reinterpret_cast<const uint8_t *>(hello), strlen(hello))=="aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d");
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
    frs.create_named_feature_recorder("test");
    feature_recorder *ft = frs.get_name("test");
    REQUIRE( ft!=nullptr );
    ft->quote_if_necessary(f1,c1);
    REQUIRE( f1=="feature" );
    REQUIRE( c1=="context" );
}

/* Read all of the lines of a file and return them as a vector */
std::vector<std::string> getLines(const std::string &filename)
{
    std::vector<std::string> lines;
    std::string line;
    std::ifstream inFile;
    std::cerr << "filename: " << filename << "\n";
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

        feature_recorder_set frs( flags, "sha1", scanner_config::NO_INPUT, tempdir);
        frs.create_named_feature_recorder("test");
        REQUIRE( frs.get_name("test") != nullptr);
        REQUIRE( frs.get_name("test_not") == nullptr);

        feature_recorder *fr = frs.get_name("test");
        pos0_t p;
        fr->write(p, "one", "context");
        fr->write(p+5, "one", "context");
        fr->write(p+10, "two", "context");

        /* Ask the feature recorder to create a histogram */


    }
    /* get the last line of the test file and see if it is correct */
    std::string expected_lastline {"10\ttwo\tcontext"};
    std::vector<std::string> lines = getLines(tempdir+"/test.txt");

    REQUIRE( lines.back() == expected_lastline);


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
TEST_CASE( "histogram_def", "[histogram_def]" ){
    histogram_def h1("feature1","pattern1","suffix1",histogram_def::flags_t());
    histogram_def h2("feature2","pattern2","suffix2",histogram_def::flags_t());

    REQUIRE( h1 == h1);
    REQUIRE( h1 != h2);
    REQUIRE( h1 < h2);
}

/****************************************************************
 * histogram_maker.h
 */
#include "histogram_maker.h"
TEST_CASE( "histogram_maker", "[histogram_maker]") {
    HistogramMaker::ReportElement e1("hello");
    HistogramMaker::ReportElement e2("world");

    REQUIRE( e1 == e1 );
    REQUIRE( e1 != e2);
    REQUIRE( e1 < e2);

    histogram_def h1("p","([0-9]+)","phones",histogram_def::flags_t());
    HistogramMaker hm(h1);

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

TEST_CASE( "test atomic_set_map", "[vector]" ){
    atomic_set<std::string> as;
    as.insert("one");
    as.insert("two");
    as.insert("three");
    REQUIRE( as.contains("one") == true );
    REQUIRE( as.contains("four") == false );
}

/****************************************************************
 *
 * sbuf.h
 */
#include "sbuf.h"

const char *hello="Hello world!";
const uint8_t *hello_buf = reinterpret_cast<const uint8_t *>(hello);
sbuf_t hello_sbuf() {
    pos0_t p0("hello");
    return sbuf_t(p0, hello_buf, strlen(hello), strlen(hello), 0, false, false, false);
}

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


    sbuf_t *sb0 = sbuf_t::map_file(fname);
    REQUIRE(sb0 != nullptr);
    sbuf_t &sb1 = *sb0;
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
        REQUIRE( ss.get_scanner_by_name("no_such_scanner") == nullptr );
        REQUIRE( ss.get_scanner_by_name("sha1") == scan_sha1 );
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
    //REQUIRE( ss.count_histograms() == 1);
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
