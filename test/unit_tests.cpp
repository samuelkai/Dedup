#include "find_duplicates.h"
#include "catch2/catch.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

TEST_CASE( "simple" )
{
    DedupTable<uint64_t> testmap;
    
    fs::path test_dir_path = fs::temp_directory_path() / "dedup_test98437524";
    fs::create_directory(test_dir_path);
    
    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    fs::resize_file(test_dir_path / "test3.txt", 100);

    gather_hashes(test_dir_path, testmap, 0, false);

    fs::remove_all(test_dir_path);

    REQUIRE( testmap.size() == 2 );
}
