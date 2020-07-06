// https://github.com/catchorg/Catch2/blob/master/docs/tutorial.md#top

#define CATCH_CONFIG_MAIN
#include "config.h"

// define stuff I need in the global environment. Only read it once.
#include "regex_vector.h"

#include "tests/catch.hpp"

TEST_CASE( "test regex_vector", "[vector]" ) {
    REQUIRE( regex_vector::has_metachars("this[1234]foo") == true );
}
//CESTER_TEST(test_regex_vector_2, inst, cester_assert_true(  regex_vector::has_metachars("this(1234)foo"));    )
//CESTER_TEST(test_regex_vector_3, inst, cester_assert_true(  regex_vector::has_metachars("this[1234].*foo"));    )
//CESTER_TEST(test_regex_vector_4, inst, cester_assert_false( regex_vector::has_metachars("this1234foo"));    )


