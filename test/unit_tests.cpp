#include "find_duplicates.h"
#include "catch2/catch.hpp"

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

/**
 * Needed in order to get the ParseResult.
 */
cxxopts::ParseResult parse(int argc, char* argv[])
{
    const vector<int> hash_sizes = {1,2,4,8};
    const int DEFAULT_HASH_SIZE = 8;
    string hash_sizes_str;
    for (size_t i = 0; i < hash_sizes.size(); ++i) {
        hash_sizes_str += std::to_string(hash_sizes[i]);
        if (i+1 != hash_sizes.size())
        {
            hash_sizes_str += ", ";
        }
    }

    cxxopts::Options options(argv[0], " - find duplicate files");
    options
        .positional_help("path1 [path2] [path3]...")
        .show_positional_help();

    options.add_options()
        ("path", "path", cxxopts::value<vector<string>>(), "PATH");

    options.add_options("Optional")
        ("a,hash", "Hash digest size in bytes, valid values are " + 
            hash_sizes_str, cxxopts::value<int>()->default_value(
            std::to_string(DEFAULT_HASH_SIZE)), "N")
        ("b,bytes", "Number of bytes from the beginning of each file that "
            "are used in hash calculation. "
            "0 means that the whole file is hashed.",
            cxxopts::value<uint64_t>()->default_value("4096"), "N")
        ("c,skip-count", "Skip initial file counting. Speeds up the "
            "process but disables the progress bar.",
            cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print this help")
        ("l,list", "List found duplicates, don't prompt for deduplication", 
            cxxopts::value<bool>()->default_value("false"))
        ("r,recursive", "Search the paths for duplicates recursively",
            cxxopts::value<bool>()->default_value("false"))
        ("s,summarize", "Print only a summary of found duplicates, "
            "don't prompt for deduplication",
            cxxopts::value<bool>()->default_value("false"))
    ;

    options.parse_positional({"path"});

    auto result = options.parse(argc, argv);

    return result;
}

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
