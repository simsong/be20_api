// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_MAIN

#include "config.h"

//#ifdef DFXML_GNUC_HAS_IGNORED_SHADOW_PRAGMA
//#pragma GCC diagnostic ignored "-Wshadow"
//#endif

//#ifdef DFXML_GNUC_HAS_IGNORED_UNUSED_VARIABLE_PRAGMA
//#pragma GCC diagnostic ignored "-Wunused-variable"
//#endif

//#ifdef DFXML_GNUC_HAS_IGNORED_UNUSED_LABEL_PRAGMA
//#pragma GCC diagnostic ignored "-Wunused-label"
//#endif


// define stuff I need in the global environment. Only read it once.
#include "regex_vector.h"

#include "tests/catch.hpp"

TEST_CASE( "test regex_vector", "[vector]" ) {
    REQUIRE( regex_vector::has_metachars("this[1234]foo") == true );
}
//CESTER_TEST(test_regex_vector_2, inst, cester_assert_true(  regex_vector::has_metachars("this(1234)foo"));    )
//CESTER_TEST(test_regex_vector_3, inst, cester_assert_true(  regex_vector::has_metachars("this[1234].*foo"));    )
//CESTER_TEST(test_regex_vector_4, inst, cester_assert_false( regex_vector::has_metachars("this1234foo"));    )


