// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_MAIN
#include "config.h"

// define stuff I need in the global environment. Only read it once.
#include "regex_vector.h"
#include "atomic_set_map.h"
#include "sbuf.h"

#include "tests/catch.hpp"


TEST_CASE( "test regex_vector", "[vector]" ) {
    REQUIRE( regex_vector::has_metachars("this[1234]foo") == true );
    REQUIRE( regex_vector::has_metachars("this(1234)foo") == true );
    REQUIRE( regex_vector::has_metachars("this[1234].*foo") == true);
    REQUIRE( regex_vector::has_metachars("this1234foo") == false);
}

TEST_CASE( "test atomic_set_map", "[vector]" ){
    atomic_set<std::string> as;
    as.insert("one");
    as.insert("two");
    as.insert("three");
    REQUIRE( as.contains("one") == true );
    REQUIRE( as.contains("four") == false );
}

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

TEST_CASE( "sbuf", "[vector]" ){
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







    
