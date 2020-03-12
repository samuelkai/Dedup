#include "find_duplicates.h"
#include "catch2/catch.hpp"
#include "parse.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using std::string;

namespace fs = std::filesystem;

TEST_CASE( "simple" )
{
    const fs::path test_dir_path = 
        fs::temp_directory_path() / "dedup_test98437524";

    fs::create_directory(test_dir_path);
    
    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    fs::resize_file(test_dir_path / "test3.txt", 100);

    constexpr int argc = 2;
    char* argv[] = {const_cast<char *>("dedup"), const_cast<char *>("-s")};

    const auto result = parse(argc, argv);

    const std::set<fs::path> paths_to_deduplicate = {test_dir_path};

    const auto duplicates = 
        find_duplicates<uint64_t>(result, paths_to_deduplicate);

    fs::remove_all(test_dir_path);

    REQUIRE( duplicates.size() == 1 );
}

TEST_CASE( "simple2" )
{
    const fs::path test_dir_path = 
        fs::temp_directory_path() / "dedup_test98437524";

    fs::remove_all(test_dir_path);
    fs::create_directory(test_dir_path);
    
    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    fs::resize_file(test_dir_path / "test3.txt", 100);

    fs::copy_file(test_dir_path / "test3.txt", test_dir_path / "test4.txt");

    constexpr int argc = 2;
    char* argv[] = {const_cast<char *>("dedup"), const_cast<char *>("-s")};

    const auto result = parse(argc, argv);

    const std::set<fs::path> paths_to_deduplicate = {test_dir_path};

    const auto duplicates = 
        find_duplicates<uint64_t>(result, paths_to_deduplicate);

    fs::remove_all(test_dir_path);

    REQUIRE( duplicates.size() == 2 );
}

TEST_CASE( "simple3" )
{
    const fs::path test_dir_path = 
        fs::temp_directory_path() / "dedup_test98437524";

    fs::remove_all(test_dir_path);
    fs::create_directory(test_dir_path);
    
    std::ofstream outfile (test_dir_path / "test.txt");
    for (int i = 0; i < 1024; ++i)
    {
        outfile << "Test";
    }
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    outfile.open(test_dir_path / "test1.txt");
    outfile << "1";
    outfile.close();

    outfile.open(test_dir_path / "test2.txt");
    outfile << "1";
    outfile.close();

    outfile.open(test_dir_path / "test3.txt");
    outfile << "3";
    outfile.close();

    constexpr int argc = 2;
    char* argv[] = {const_cast<char *>("dedup"), const_cast<char *>("-s")};

    const auto result = parse(argc, argv);

    const std::set<fs::path> paths_to_deduplicate = {test_dir_path};

    const auto duplicates = 
        find_duplicates<uint64_t>(result, paths_to_deduplicate);

    fs::remove_all(test_dir_path);

    REQUIRE( duplicates.size() == 1 );
    
    int no_duplicates = 0;
    for (const auto &vec : duplicates)
    {
        no_duplicates += vec.size() - 1;
    }

    REQUIRE ( no_duplicates == 1 );
}