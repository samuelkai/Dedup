#include "deal_with_duplicates.h"
#include "find_duplicates.h"
#include "catch2/catch.hpp"
#include "parse.h"
#include "sys/stat.h"
#include "utilities.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

using std::string;
using std::vector;

namespace fs = std::filesystem;

size_t count_files(fs::path dir)
{
    size_t count = 0;
    for (const auto &path : fs::directory_iterator(dir))
    {
        if (fs::is_regular_file(path))
        {
            ++count;
        }
    }
    return count;
}

fs::path create_test_dir()
{
    const fs::path test_dir_path = 
    fs::temp_directory_path() / "dedup_test98437524";
    fs::create_directory(test_dir_path);
    return test_dir_path;
}

ArgMap parse_cl_args(vector<string> arguments)
{
    std::vector<char*> argv;
    for (auto &arg : arguments)
        argv.push_back(arg.data());
    argv.push_back(nullptr);

    return parse(argv.size() - 1, argv.data());
}

ino_t get_inode(fs::path path)
{
    struct stat s;
    stat(path.string().c_str(), &s);
    return s.st_ino;
}

TEST_CASE( "test_delete" )
{
    const fs::path test_dir_path = create_test_dir(); 

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    std::vector<std::string> arguments = 
        {"dedup", "-dd", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path) == 1);    
}

TEST_CASE( "test_hardlink" )
{
    const fs::path test_dir_path = create_test_dir(); 

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    std::vector<std::string> arguments = 
        {"dedup", "-k", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::hardlink, duplicates);

    REQUIRE (count_files(test_dir_path) == 3);
    REQUIRE (get_inode(test_dir_path / "test.txt") == 
             get_inode(test_dir_path / "test2.txt"));
}

TEST_CASE( "test_symlink" )
{
    const fs::path test_dir_path = create_test_dir(); 

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test2.txt");
    fs::copy_file(test_dir_path / "test.txt", test_dir_path / "test3.txt");

    std::vector<std::string> arguments = 
        {"dedup", "-y", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::symlink, duplicates);

    REQUIRE (count_files(test_dir_path) == 3);
    REQUIRE (fs::read_symlink(test_dir_path / "test2.txt") == 
             (test_dir_path / "test.txt"));
    REQUIRE (fs::read_symlink(test_dir_path / "test3.txt") == 
             (test_dir_path / "test.txt"));
}

TEST_CASE( "test_priority_dir" )
{
    const fs::path test_dir_path = create_test_dir();

    fs::create_directory(test_dir_path / "dir1");
    fs::create_directory(test_dir_path / "dir2");
    fs::create_directory(test_dir_path / "dir3");
    
    std::ofstream outfile (test_dir_path / "dir2" / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "dir2" / "test.txt", 
                  test_dir_path / "dir3" / "test.txt");
    
    fs::copy_file(test_dir_path / "dir2" / "test.txt", 
                  test_dir_path / "dir1" / "test.txt");

    std::vector<std::string> arguments = 
        {"dedup", "-dd", (test_dir_path / "dir3").string(), 
                         (test_dir_path / "dir1").string(),
                         (test_dir_path / "dir2").string()
        };

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path / "dir1") == 0);    
    REQUIRE (count_files(test_dir_path / "dir2") == 0);    
    REQUIRE (count_files(test_dir_path / "dir3") == 1);    
}

TEST_CASE( "test_priority_age" )
{
    const fs::path test_dir_path = create_test_dir();

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::copy_file(test_dir_path / "test.txt", 
                  test_dir_path / "test2.txt");
    
    fs::copy_file(test_dir_path / "test2.txt", 
                  test_dir_path / "test3.txt");

    fs::last_write_time(test_dir_path / "test.txt", 
        fs::last_write_time(test_dir_path / "test2.txt") + 
        std::chrono::seconds(1));

    fs::last_write_time(test_dir_path / "test3.txt", 
        fs::last_write_time(test_dir_path / "test2.txt") + 
        std::chrono::seconds(1));

    // Now test2.txt has the earliest modification time
    
    std::vector<std::string> arguments =
        {"dedup", "-dd", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE_FALSE (fs::exists(test_dir_path / "test.txt"));    
    REQUIRE (fs::exists(test_dir_path / "test2.txt"));    
    REQUIRE_FALSE (fs::exists(test_dir_path / "test3.txt"));    
}

TEST_CASE( "test_same_path_skip" )
{
    const fs::path test_dir_path = create_test_dir();
    fs::create_directory(test_dir_path / "a_dir");

    std::ofstream outfile (test_dir_path / "a_dir" / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    std::vector<std::string> arguments =
        {"dedup", "-dd", (test_dir_path / "a_dir").string(), 
                         (test_dir_path / "a_dir" / ".." / "a_dir").string()
        };

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path / "a_dir") == 1);    
}

TEST_CASE( "test_symlink_inside_dir_skip" )
{
    const fs::path test_dir_path = create_test_dir();

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::create_symlink(test_dir_path / "test.txt", test_dir_path / "link");

    std::vector<std::string> arguments =
        {"dedup", "-dd", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path) == 2);    
}

TEST_CASE( "test_symlink_as_given_path_skip" )
{
    const fs::path test_dir_path = create_test_dir();

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();

    fs::create_symlink(test_dir_path / "test.txt", test_dir_path / "link");

    std::vector<std::string> arguments =
        {"dedup", "-dd", (test_dir_path / "test.txt").string(),
                         (test_dir_path / "link").string()
        };

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);

    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path) == 2);    
}

TEST_CASE( "test_recursive" )
{
    const fs::path test_dir_path = create_test_dir();

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();
    
    fs::create_directory(test_dir_path / "a_dir");

    std::ofstream outfile2 (test_dir_path / "a_dir" / "test.txt");
    outfile2 << "Test text!" << std::endl;
    outfile2.close();

    std::vector<std::string> arguments =
        {"dedup", "-rdd", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);
    
    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path) == 1); 
    REQUIRE (count_files(test_dir_path / "a_dir") == 0); 
}

TEST_CASE( "test_not_recursive" )
{
    const fs::path test_dir_path = create_test_dir();

    std::ofstream outfile (test_dir_path / "test.txt");
    outfile << "Test text!" << std::endl;
    outfile.close();
    
    fs::create_directory(test_dir_path / "a_dir");

    std::ofstream outfile2 (test_dir_path / "a_dir" / "test.txt");
    outfile2 << "Test text!" << std::endl;
    outfile2.close();

    std::vector<std::string> arguments =
        {"dedup", "-dd", test_dir_path.string()};

    ArgMap cl_args = parse_cl_args(arguments);

    const auto duplicates = find_duplicates(cl_args);
    
    deal_with_duplicates(Action::no_prompt_delete, duplicates);

    REQUIRE (count_files(test_dir_path) == 1); 
    REQUIRE (count_files(test_dir_path / "a_dir") == 1); 
}
