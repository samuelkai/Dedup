#include "gather_hashes.h"
#include "catch2/catch.hpp"
#include <iostream>

TEST_CASE( "simple" )
{
    DedupTable testmap;
    gather_hashes("../../testi", testmap, 0, false);
    REQUIRE( testmap.size() == 5 );
}
