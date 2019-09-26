#include <iostream>
#include "catch2/catch.hpp"
#include "gather_hashes.h"

TEST_CASE( "simple" )
{
    std::unordered_map<uint64_t, std::vector<std::string>> testmap;
    gather_hashes("../../testi", testmap);
    REQUIRE( testmap.size() == 5 );
}
