// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_MAIN
#include "config.h"
#include "tests/catch.hpp"

// define stuff I need in the global environment. Only read it once.
#include "regex_vector.h"
#include "atomic_set_map.h"
#include "sbuf.h"

#include "word_and_context_list.h"
#include "feature_recorder_set.h"
#include "unicode_escape.h"

/* atomic_set_map.h */


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

/* feature_recorder.h */


/* regex_vector.h & regex_vector.cpp */

TEST_CASE( "test regex_vector", "[vector]" ) {
    REQUIRE( regex_vector::has_metachars("this[1234]foo") == true );
    REQUIRE( regex_vector::has_metachars("this(1234)foo") == true );
    REQUIRE( regex_vector::has_metachars("this[1234].*foo") == true);
    REQUIRE( regex_vector::has_metachars("this1234foo") == false);

    regex_vector rv;
    rv.push_back("this.*");
    rv.push_back("check[1-9]");
    rv.push_back("thing");
    std::cout << rv;
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

/* sbuf.h */

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


/* word_and_context_list.h */
TEST_CASE("word_and_context_list", "[vector]") {
    REQUIRE( word_and_context_list::rstrcmp("aaaa1", "bbbb0") < 0 );
    REQUIRE( word_and_context_list::rstrcmp("aaaa1", "aaaa1") == 0 );
    REQUIRE( word_and_context_list::rstrcmp("bbbb0", "aaaa1") > 0 );

}

static std::string hash_name("md5");
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

static feature_recorder_set::hash_def my_hasher(hash_name,hash_func);

#if 0
feature_recorder_set::feature_recorder_set(uint32_t flags_,const feature_recorder_set::hash_def &hasher_,
                                           const std::string &input_fname_, const std::string &outdir_):
    flags(flags_),seen_set(),input_fname(input_fname_),
    outdir(outdir_),
    frm(),
    histogram_defs(),
    db3(),
    alert_list(),stop_list(),
    scanner_stats(),hasher(hasher_)
{
}

feature_recorder *feature_recorder_set::create_name_factory(const std::string &name_){return 0;}
void feature_recorder_set::create_name(const std::string &name,bool create_stop_also){}
bool feature_recorder_set::check_previously_processed(const uint8_t *buf, size_t bufsize){return 0;}
feature_recorder *feature_recorder_set::get_name(const std::string &name) const{return 0;}
feature_recorder *feature_recorder_set::get_alert_recorder() const{return 0;}
void feature_recorder_set::get_feature_file_list(std::vector<std::string> &ret){}
#endif


TEST_CASE("feature_recorder_sql", "[sql]") {
    const char *dbfile = "test.sql3";
    char *errmsg = 0;
    sqlite3 *db=0;

    feature_recorder_set fs(0,my_hasher,"","/tmp");

    unlink(dbfile);
    fs.db_create();

    /* Create an email table */
    fs.db_create_table("email");
        
    /* Lets throw a million features into the table as a test */
    sqlite3_exec(db,"BEGIN TRANSACTION",NULL,NULL,&errmsg);
    //beapi_sql_stmt s(db,"email");
    for(int i=0;i<1000000;i++){
        pos0_t p;
        pos0_t p1 = p+i;
        
        if(i%10000==0) printf("i=%d\n",i);
        
        char feature[64];
        snprintf(feature,sizeof(feature),"user%d@company.com",i);
        char context[64];
        snprintf(context,sizeof(context),"this is the context user%d@company.com yes it is!",i);
        //insert_statement(stmt,p1,feature,context);
    }
    sqlite3_exec(db,"COMMIT TRANSACTION",NULL,NULL,&errmsg);
    fs.db_close();

    /* Now verify that they are there */
}


TEST_CASE("featrure_recorder_set", "[frs]") {
}


TEST_CASE("unicode_escape", "[unicode]") {
    const char *U1F601 = "\xF0\x9F\x98\x81";        // UTF8 for Grinning face with smiling eyes
    REQUIRE( hexesc('a') == "\\x61");               // escape the escape!
    REQUIRE( utf8cont('a') == false );
    REQUIRE( utf8cont( U1F601[0] ) == false); // the first character is not a continuation character
    REQUIRE( utf8cont( U1F601[1] ) == true);  // the second is
    REQUIRE( utf8cont( U1F601[2] ) == true);  // the third is 
    REQUIRE( utf8cont( U1F601[3] ) == true);  // the fourth is
}

    
