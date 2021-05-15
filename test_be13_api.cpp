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
#define CATCH_CONFIG_CONSOLE_WIDTH 120
#include "config.h"                     // supposed to come after bulk_extractor_i.h

#include "catch.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <iostream>
#include <filesystem>

#include "atomic_unicode_histogram.h"
#include "sbuf.h"


/****************************************************************
 *** Support code
 ****************************************************************/

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

// A single tempdir for testing
std::string get_tempdir()
{
    static std::string tempdir;

    if (tempdir==""){
        tempdir = std::filesystem::temp_directory_path().string();
        if (tempdir.back()!='/'){
            tempdir += '/';
        }
        tempdir += random_string(8);
        std::filesystem::create_directory(tempdir);
        std::cerr << "Test results in: " << tempdir << "\n";
    }
    return tempdir;
}

#ifdef HAVE_MACH_O_DYLD_H
#include <mach-o/dyld.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LIMITS_LIMITS_H
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
    ssize_t ret = readlink("/proc/self/exe",rpath, sizeof(rpath));
    if (ret<0){
        throw std::runtime_error("readlink failed");
    }
    if (ret==sizeof(rpath)) {
        throw std::runtime_error("rpath too small");
    }
    rpath[ret] = 0;                     // terminate the string
#endif
    return std::string(rpath);
}

std::string tests_dir()
{
    /* if srcdir is defined, then return srcdir/tests */
    const char *srcdir = getenv("srcdir");
    if (srcdir) {
        return std::string(srcdir) + "/tests";
    }

    /* Otherwise, return relative to this executable */
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

/* Read all of the lines of a file and return them as a vector */
std::vector<std::string> getLines(const std::string &filename)
{
    std::vector<std::string> lines;
    std::string line;
    std::ifstream inFile;
    inFile.open(filename.c_str());
    if (!inFile.is_open()) {
        throw std::runtime_error("getLines: Cannot open file: "+filename);
    }
    while (std::getline(inFile, line)){
        if (line.size()>0){
            lines.push_back(line);
        }
    }
    return lines;
}

/*************************
 *** UNIT TESTS FOLLOW ***
 *************************/

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
#include "atomic_set.h"
TEST_CASE( "atomic_set", "[atomic]" ){
    atomic_set<std::string> as;
    REQUIRE( as.size() == 0);
    as.insert("one");
    as.insert("two");
    as.insert("three");
    REQUIRE( as.contains("one") == true );
    REQUIRE( as.contains("four") == false );
    REQUIRE( as.size() == 3);
}

#include "atomic_map.h"
TEST_CASE( "atomic_map", "[atomic]" ){
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
 * histogram_def.h
 */
#include "histogram_def.h"
TEST_CASE( "histogram_def comparision functions", "[histogram_def]" ){
    histogram_def h1("name1","feature1","pattern1","","suffix1",histogram_def::flags_t());
    histogram_def h2("name2","feature2","pattern2","","suffix2",histogram_def::flags_t());

    REQUIRE( h1 == h1);
    REQUIRE( h1 != h2);
    REQUIRE( h1  < h2);
}

TEST_CASE( "histogram_def 8-bit regular expression functions" , "[histogram_def]") {
    histogram_def d0("numbers", "numbers", "([0-9]+)", "", "s0", histogram_def::flags_t());
    REQUIRE ( d0.match("123" ) == true);
    REQUIRE ( d0.match("abc" ) == false);

    std::string s1;
    REQUIRE ( d0.match("abc123def", &s1) == true);
    REQUIRE ( s1 == "123" );

    histogram_def d1("extraction", "extraction", "^(.....)", "", "", histogram_def::flags_t());
    REQUIRE ( d1.match("abcdefghijklmnop", &s1) == true);
    REQUIRE ( s1 == "abcde" );

};


/****************************************************************
 * atomic_unicode_histogram.h
 */
#include "atomic_unicode_histogram.h"
#include "histogram_def.h"
TEST_CASE( "First AtomicUnicodeHistogram test", "[atomic][regex]" ){
    /* Histogram that matches everything */
    histogram_def d1("name","feature_file","(.*)","","suffix1",histogram_def::flags_t());
    AtomicUnicodeHistogram h(d1);
    h.add("foo");
    h.add("foo");
    h.add("foo");
    h.add("bar");

    /* Now make sure things were added with the right counts */
    AtomicUnicodeHistogram::FrequencyReportVector f = h.makeReport();
    REQUIRE( f.size() == 2);
    REQUIRE( f.at(0).key=="foo");
    REQUIRE( f.at(0).value.count==3);
    REQUIRE( f.at(0).value.count16==0);

    REQUIRE( f.at(1).key=="bar");
    REQUIRE( f.at(1).value.count==1);
    REQUIRE( f.at(1).value.count16==0);

    f = h.makeReport(1);

    REQUIRE( f.size() == 1);
    REQUIRE( f.at(0).key=="foo");
    REQUIRE( f.at(0).value.count==3);
    REQUIRE( f.at(0).value.count16==0);
}

TEST_CASE( "Second AtomicUnicodeHistogram test", "[atomic][regex]" ){
    /* Histogram that matches everything */
    histogram_def d1("extraction", "extraction", "^(.....)", "", "", histogram_def::flags_t());
    AtomicUnicodeHistogram h(d1);
    h.add("abcdefghijklmnop");

    /* Now make sure things were added with the right counts */
    AtomicUnicodeHistogram::FrequencyReportVector f = h.makeReport();
    REQUIRE( f.at(0).key=="abcde");
    REQUIRE( f.at(0).value.count==1);
    REQUIRE( f.at(0).value.count16==0);
}

TEST_CASE( "Third AtomicUnicodeHistogram test", "[histogram]") {
    /* Make sure that the histogram elements work */
    AtomicUnicodeHistogram::auh_t::AMReportElement e1("hello");
    AtomicUnicodeHistogram::auh_t::AMReportElement e2("world");

    REQUIRE( e1 == e1 );
    REQUIRE( e1 != e2);
    REQUIRE( e1 < e2);

    /* Try a simple histogram with just numbers */
    histogram_def::flags_t flags;
    flags.numeric = true;
    histogram_def h1("phones", "p","([0-9]+)","", "phones",flags);
    AtomicUnicodeHistogram hm(h1);

    hm.add("100");
    hm.add("200");
    hm.add("300");
    hm.add("200");
    hm.add("300");
    hm.add("foo 300 bar");

    AtomicUnicodeHistogram::auh_t::report r = hm.makeReport(0);
    REQUIRE( r.size() == 3);

    /* Make sure that the tallies were correct. */
    REQUIRE( r.at(0).key == "300");
    REQUIRE( r.at(0).value.count == 3);
    REQUIRE( r.at(0).value.count16 == 0);

    REQUIRE( r.at(1).key == "200");
    REQUIRE( r.at(1).value.count == 2);
    REQUIRE( r.at(1).value.count16 == 0);

    REQUIRE( r.at(2).key == "100");
    REQUIRE( r.at(2).value.count == 1);
    REQUIRE( r.at(2).value.count16 == 0);

    /* Add a UTF-16 300 */
    char buf300[6] {'\000','3','\000','0','\000','0'};
    std::string b300(buf300,6);
    hm.add(b300);
    r = hm.makeReport(0);
    REQUIRE( r.at(0).key == "300");
    REQUIRE( r.at(0).value.count == 4);
    REQUIRE( r.at(0).value.count16 == 1);

    /* write the histogram to a file */
    std::string tempdir = get_tempdir();
    std::string fname = tempdir + "/histogram1.txt";
    {
        std::ofstream out(fname.c_str());
        out << r;
        out.close();
    }

    /* And verify the histogram that was written */
    {
        std::vector<std::string> lines {getLines(fname)};
        REQUIRE( lines.size() == 3);
        REQUIRE( lines[0] == "n=4\t300\t(utf16=1)" );
        REQUIRE( lines[1] == "n=2\t200" );
        REQUIRE( lines[2] == "n=1\t100" );
    }
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
#include "feature_recorder_file.h"
TEST_CASE("quote_if_necessary", "[feature_recorder]") {
    std::string f1("feature");
    std::string c1("context");

    feature_recorder_set::flags_t flags;
    flags.no_alert = true;

    feature_recorder_set frs( flags, "sha1", scanner_config::NO_INPUT, scanner_config::NO_OUTDIR);
    frs.create_feature_recorder("test");
    feature_recorder &ft = frs.named_feature_recorder("test");
    ft.quote_if_necessary(f1,c1);
    REQUIRE( f1=="feature" );
    REQUIRE( c1=="context" );
}

TEST_CASE("fname in outdir", "[feature_recorder]") {
    feature_recorder_set::flags_t flags;
    flags.no_alert = true;
    feature_recorder_set frs( flags, "sha1", scanner_config::NO_INPUT, get_tempdir());
    frs.create_feature_recorder("test");
    feature_recorder &ft = frs.named_feature_recorder("test");

    const std::string n = ft.fname_in_outdir("foo", feature_recorder::NO_COUNT);
    std::filesystem::path p(n);
    REQUIRE( p.filename()=="test_foo.txt");

    const std::string n1 = ft.fname_in_outdir("bar", feature_recorder::NEXT_COUNT);
    std::filesystem::path p1(n1);
    REQUIRE( p1.filename()=="test_bar.txt");

    const std::string n2 = ft.fname_in_outdir("bar", feature_recorder::NEXT_COUNT);
    std::filesystem::path p2(n2);
    REQUIRE( p2.filename()=="test_bar_1.txt");
}

/****************************************************************
 * feature_recorder_set.h
 *
 * Create a simple set and write two features and make a histogram.
 */
TEST_CASE("feature_recorder_def", "[feature_recorder_set]") {
    feature_recorder_def d1("test1");
    feature_recorder_def d2("test1");
    feature_recorder_def d3("test1"); d3.flags.xml = true;
    feature_recorder_def d4("test4");

    REQUIRE ( d1.name=="test1");
    REQUIRE ( d1 == d2 );
    REQUIRE ( d1.name == d3.name );
    REQUIRE ( d1.flags != d3.flags );
    REQUIRE ( d1 != d3 );
    //REQUIRE ( d1 < d4 );
}


TEST_CASE("write_features", "[feature_recorder_set]" ) {

    // Create a random directory for the output of the feature recorder
    std::string tempdir = get_tempdir();
    {
        feature_recorder_set::flags_t flags;
        flags.no_alert = true;
        flags.debug    = true;

        feature_recorder_set fs( flags, "sha1", scanner_config::NO_INPUT, tempdir);
        feature_recorder &fr = fs.create_feature_recorder("feature_file");

        /* Make sure requesting a valid name does not generate an exception and an invalid name generates an exception */
        REQUIRE_NOTHROW( fs.named_feature_recorder("feature_file") );
        REQUIRE_THROWS_AS( fs.named_feature_recorder("test_not"), feature_recorder_set::NoSuchFeatureRecorder);

        /* Ask the feature recorder to create a histogram */
        histogram_def h1("name","feature_file","([0-9]+)","","suffix1",histogram_def::flags_t());
        REQUIRE( fs.histogram_count() == 0);
        fs.histogram_add(h1);
        REQUIRE( fs.histogram_count() == 1);

        pos0_t p;
        fr.write(p,    "one", "context");
        fr.write(p+5,  "one", "context");
        fr.write(p+10, "two", "context");

        sbuf_t sb16 = hello16_sbuf();
        REQUIRE( sb16.size() == strlen(hello)*2 );
    }
#if 0
    std::vector<std::string> lines = getLines(tempdir+"/name_suffix1.txt");
    std::cerr << "here is the histogram:\n";
    for (auto line: lines){
        std::cerr << line << "\n";
    }
    /* get the last line of the test file and see if it is correct */
    std::string expected_lastline {"hello16-0\t"
        "H\\x00e\\x00l\\x00l\\x00o\\x00 \\x00w\\x00o\\x00r\\x00l\\x00d\\x00!\\x00"
        "\tH\\x00e\\x00l\\x00l\\x00o\\x00 \\x00w\\x00o\\x00r\\x00l\\x00d\\x00!\\x00"};

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
 *
 * pos0.h:
 */

#include "pos0.h"
TEST_CASE( "pos0_t", "[feature_recorder]" ){
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
TEST_CASE( "test regex_vector", "[regex]" ) {
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
TEST_CASE("scanner_config", "[scanner]") {
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

    sc.push_scanner_command("scanner1",scanner_config::scanner_command::ENABLE);
    sc.push_scanner_command("scanner2",scanner_config::scanner_command::DISABLE);
    REQUIRE(sc.scanner_commands.size()==2);

}

/****************************************************************
 * scanner_params.h:
 * The interface used by scanners.
 */
#include "scanner_params.h"
TEST_CASE("scanner", "[scanner]") {
    /* check that scanner params made from an existing scanner params are deeper */
}

/****************************************************************
 * scanner_set.h:
 * Creates a place to hold all of the scanners.
 * The be13_api contains a single scanner for testing purposes:
 * scan_null, the null scanner, which writes metadata into a version.txt feature file.
 */
#include "scanner_set.h"
#include "scan_sha1_test.h"
TEST_CASE("enable/disable", "[scanner]") {
    scanner_config sc;
    sc.outdir = get_tempdir();
    sc.hash_alg = "sha1";

    /* Enable the scanner */
    const std::string SHA1_TEST = "sha1_test";

    struct feature_recorder_set::flags_t f;
    {
        sc.push_scanner_command(SHA1_TEST, scanner_config::scanner_command::ENABLE);
        scanner_set ss(sc, f);
        ss.add_scanner(scan_sha1_test);
        ss.apply_scanner_commands();        // applied after all scanners are added

        /*  Make sure that the scanner was added  */
        std::cerr << "FOO1\n";
        REQUIRE_NOTHROW( ss.get_scanner_by_name("sha1_test") );
        std::cerr << "FOO2\n";
        REQUIRE( ss.get_scanner_by_name("sha1_test") == scan_sha1_test );
        REQUIRE_THROWS_AS(ss.get_scanner_by_name("no_such_scanner"), scanner_set::NoSuchScanner );

        /* Make sure that the sha1 scanner is enabled and only the sha1 scanner is enabled */
        REQUIRE( ss.is_scanner_enabled(SHA1_TEST) == true );
        REQUIRE( ss.is_find_scanner_enabled() == false); // only sha1 scanner is enabled

        /* Make sure that the scanner set has a two feature recorders:
         * the alert recorder and the sha1_bufs recorder
         */
        REQUIRE( ss.feature_recorder_count() == 2);

        /* Make sure it has a single histogram */
        REQUIRE( ss.histogram_count() == 1);
    }
    {
        /* Try it again, but this time turning on all of the commands */
        sc.push_scanner_command(scanner_config::scanner_command::ALL_SCANNERS, scanner_config::scanner_command::ENABLE);
        scanner_set ss(sc, f);
        ss.add_scanner(scan_sha1_test);
        ss.apply_scanner_commands();        // applied after all scanners are adde
        REQUIRE( ss.is_scanner_enabled(SHA1_TEST) == true );
    }
    {
        /* Try it again, but this time turning on the scanner, and then turning all off*/
        sc.push_scanner_command(SHA1_TEST, scanner_config::scanner_command::ENABLE);
        sc.push_scanner_command(scanner_config::scanner_command::ALL_SCANNERS, scanner_config::scanner_command::DISABLE);
        scanner_set ss(sc, f);
        ss.add_scanner(scan_sha1_test);
        ss.apply_scanner_commands();        // applied after all scanners are adde
        REQUIRE( ss.is_scanner_enabled(SHA1_TEST) == false );
    }
}

/* This test runs a scan on the hello_sbuf() with the sha1 scanner. */
TEST_CASE("run", "[scanner]") {
    scanner_config sc;
    sc.outdir = get_tempdir();
    sc.hash_alg = "sha1";               // it's faster than SHA1!
    sc.push_scanner_command(std::string("sha1_test"), scanner_config::scanner_command::ENABLE); /* Turn it onn */

    struct feature_recorder_set::flags_t f;
    scanner_set ss(sc, f);
    ss.add_scanner(scan_sha1_test);
    ss.apply_scanner_commands();

    /* Make sure we got a sha1_test feature recorder */
    feature_recorder &fr = ss.named_feature_recorder("sha1_bufs");

    REQUIRE( fr.name == "sha1_bufs");

    /* Check to see if the histogram of the first 5 characters of the SHA1 code of each buffer was properly created. */
    /* There should be 1 histogram */
    REQUIRE( ss.histogram_count() == 1);
    REQUIRE( fr.histogram_count() == 1);
    /* It should be a regular expression that extracts the first 5 characters */
    /* Test the regular expression */
    /* And it should write to a feature file that has a suffix of "_first5" */


    REQUIRE( fr.histograms[0]->def.feature == "sha1_bufs");
    REQUIRE( fr.histograms[0]->def.pattern == "^(.....)");
    REQUIRE( fr.histograms[0]->def.suffix == "first5");
    REQUIRE( fr.histograms[0]->def.flags.lowercase == true);
    REQUIRE( fr.histograms[0]->def.flags.numeric == false);

    /* Perform a simulated scan */
    ss.phase_scan();                    // start the scanner phase
    ss.process_sbuf( hello_sbuf() );    // process a single sbuf
    puts("calling ss.shutdown");
    ss.shutdown();                      // shutdown; this will write out the in-memory histogram.

    /* Make sure that the feature recorder output was created */
    std::vector<std::string> lines;
    std::string fname_fr   = get_tempdir()+"/sha1_bufs.txt";
    std::string cmd = "ls -l "+get_tempdir();
    int ret = system(cmd.c_str());
    if (ret != 0){
        throw std::runtime_error("could not list tempdir???");
    }

    lines = getLines(fname_fr);
    REQUIRE( lines.size() == 5);

    /* The sha1 scanner makes a single histogram. Make sure we got it. */
    REQUIRE( ss.histogram_count() == 1);
    std::string fname_hist = get_tempdir()+"/sha1_bufs_first5.txt";
    lines = getLines(fname_hist);
    REQUIRE( lines.size() == 1);
}


/****************************************************************
 *  word_and_context_list.h
 */
#include "word_and_context_list.h"
TEST_CASE("word_and_context_list", "[feature_recorder]") {
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

    /* Try some round-trips */
    std::u32string u32s = U"我想玩";
    REQUIRE( convert_utf8_to_utf32(convert_utf32_to_utf8( u32s) ) == u32s );

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

TEST_CASE("Show the output directory", "[end]") {
    std::string cmd = "ls -l "+get_tempdir();
    std::cout << cmd << "\n";
    int ret = system(cmd.c_str());
    if (ret != 0){
        throw std::runtime_error("could not list tempdir???");
    }
}
