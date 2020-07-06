// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_MAIN
#include "config.h"

// define stuff I need in the global environment. Only read it once.
#include "regex_vector.h"
#include "atomic_set_map.h"

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
