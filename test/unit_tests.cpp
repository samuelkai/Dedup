#include <iostream>
#include "catch2/catch.hpp"
#include "gather_hashes.h"

TEST_CASE( "simple" )
{
    DedupTable testmap;
    gather_hashes("../../testi", testmap);
    REQUIRE( testmap.size() == 5 );
}
