#include "find_duplicates.h"
#include "catch2/catch.hpp"
#include "parse.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using std::cin;
using std::cout;
using std::string;
using std::vector;

namespace fs = std::filesystem;

TEST_CASE( "simple" )
{
    fs::path test_dir_path = fs::temp_directory_path() / "dedup_test98437524";

    fs::create_directory(test_dir_path);
    
    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    fs::resize_file(test_dir_path / "test3.txt", 100);

    int argc = 2;
    char* argv[] = {const_cast<char *>("dedup"), const_cast<char *>("-s")};

    auto result = parse(argc, argv);

    std::set<fs::path> paths_to_deduplicate = {test_dir_path};

    auto duplicates = find_duplicates<uint64_t>(result, paths_to_deduplicate);

    fs::remove_all(test_dir_path);

    REQUIRE( duplicates.size() == 1 );
}

TEST_CASE( "simple2" )
{
    fs::path test_dir_path = fs::temp_directory_path() / "dedup_test98437524";

    fs::remove_all(test_dir_path);
    fs::create_directory(test_dir_path);
    
    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    fs::resize_file(test_dir_path / "test3.txt", 100);

    fs::copy_file(test_dir_path / "test3.txt", test_dir_path / "test4.txt");

    int argc = 2;
    char* argv[] = {const_cast<char *>("dedup"), const_cast<char *>("-s")};

    auto result = parse(argc, argv);

    std::set<fs::path> paths_to_deduplicate = {test_dir_path};

    auto duplicates = find_duplicates<uint64_t>(result, paths_to_deduplicate);

    fs::remove_all(test_dir_path);

    REQUIRE( duplicates.size() == 2 );
}
