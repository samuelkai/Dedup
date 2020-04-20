#include "find_duplicates.h"
#include "catch2/catch.hpp"
#include "parse.h"
#include "utilities.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
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

    std::vector<std::string> arguments = 
        {"dedup", "-s", test_dir_path.string()};

    std::vector<char*> argv;
    for (auto& arg : arguments)
        argv.push_back(arg.data());
    argv.push_back(nullptr);

    ArgMap cl_args = parse(argv.size() - 1, argv.data());

    cl_args["paths"] = std::vector<fs::path>{test_dir_path};

    const auto duplicates = find_duplicates<uint64_t>(cl_args);

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

    std::vector<std::string> arguments = 
        {"dedup", "-s", test_dir_path.string()};

    std::vector<char*> argv;
    for (auto& arg : arguments)
        argv.push_back(arg.data());
    argv.push_back(nullptr);

    ArgMap cl_args = parse(argv.size() - 1, argv.data());

    const auto duplicates = find_duplicates<uint64_t>(cl_args);

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

    std::vector<std::string> arguments = 
        {"dedup", "-s", test_dir_path.string()};

    std::vector<char*> argv;
    for (auto& arg : arguments)
        argv.push_back(arg.data());
    argv.push_back(nullptr);

    ArgMap cl_args = parse(argv.size() - 1, argv.data());

    const auto duplicates = find_duplicates<uint64_t>(cl_args);

    fs::remove_all(test_dir_path);

    REQUIRE( duplicates.size() == 1 );
    
    int no_duplicates = 0;
    for (const auto &vec : duplicates)
    {
        no_duplicates += vec.size() - 1;
    }

    REQUIRE ( no_duplicates == 1 );
}