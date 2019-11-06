#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <set>

#include "gather_hashes.h"
#include "catch2/catch.hpp"
#include "cxxopts/cxxopts.hpp"

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace ch = std::chrono;
namespace fs = std::filesystem;

bool is_positive_integer(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

void prompt_duplicate_deletions(vector<vector<string>> duplicates)
{
    vector<string> keywords = {"a", "all", "n", "none"};
    cout << "\n" << "Found " << duplicates.size()
         << " files that have duplicates:\n\n";
    for (const auto &dup_vec : duplicates)
    {
        for (size_t i = 0; i < dup_vec.size(); i++)
        {
            cout << "[" << i << "] " << dup_vec[i] << "\n";
        }
        cout << endl;

        string input;
        while (true)
        {
            cout << "Select the file(s) to keep: [0-"
                 << dup_vec.size() - 1 << "], [a]ll or [n]one" << endl;
            getline(cin, input);
            if (is_positive_integer(input))
            {
                unsigned int kept = std::stoi(input);

                for (size_t i = 0; i < dup_vec.size(); i++)
                {
                    if (i != kept)
                    {
                        try
                        {
                            if (!fs::remove(dup_vec[i]))
                            {
                                std::cerr << "File \"" << dup_vec[i]
                                          << "\" not found, could not "
                                          << "delete it\n";
                            }
                        }
                        catch (const fs::filesystem_error &e)
                        {
                            std::cerr << e.what() << '\n';
                        }
                    }
                }
                break;
            }
            else if (std::find(keywords.begin(), keywords.end(), input) 
                    != keywords.end())
            {
                if (input == "n" || input == "none")
                {
                    for (auto path : dup_vec)
                    {
                        try
                        {
                            if (!fs::remove(path))
                            {
                                std::cerr << "File \"" << path
                                          << "\" not found, could not "
                                          << "delete it\n";
                            }
                        }
                        catch (const fs::filesystem_error &e)
                        {
                            std::cerr << e.what() << '\n';
                        }
                    }
                }
                break;
            }
        }        
    }
}

cxxopts::ParseResult parse(int argc, char* argv[])
{
    try
    {
        cxxopts::Options options(argv[0], " - find duplicate files");
        options
            .positional_help("path1 [path2] [path3]...")
            .show_positional_help();

        vector<string> paths;

        options.add_options()
            ("file", "File", cxxopts::value<vector<string>>(), "FILE");

        options.add_options("Optional")
            ("h,help", "Print help")
            ("l,list", "List found duplicates, don't prompt for deduplication", 
                cxxopts::value<bool>()->default_value("false"))
            ("s,summarize", "Print only a summary of found duplicates, "
                "don't prompt for deduplication",
                cxxopts::value<bool>()->default_value("false"))
            ("r,recursive", "Search the paths for duplicates recursively",
                cxxopts::value<bool>()->default_value("false"))
            ("b,bytes", "Number of bytes from the beginning of each file that"
                "are used in hash calculation",
                cxxopts::value<uint64_t>()->default_value("1024"), "N")
        ;

        options.parse_positional({"file"});

        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
            std::cout << options.help({"Optional"}) << endl;
            exit(0);
        }

        // std::cout << "Arguments remain = " << argc << std::endl;

        return result;
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cerr << "error parsing options: " << e.what() << "\n";
        exit(1);
    }

}

int main(int argc, char *argv[])
{
    auto result = parse(argc, argv);
    auto arguments = result.arguments();

    std::set<fs::path> paths_to_deduplicate;
    if (result.count("file"))
    {
        for (const auto path : result["file"].as<vector<string>>())
        {
            if (fs::exists(path))
            {
                paths_to_deduplicate.insert(fs::canonical(path));
            }
        }
    }
    else
    {
        cout << "Usage: " << argv[0] << " path1 [path2] [path3]..." << endl;
        return 0;
    }

    DedupTable dedup_table;
    bool recursive = result["recursive"].as<bool>();
    uint64_t bytes = result["bytes"].as<uint64_t>();
    auto start_time = ch::steady_clock::now();
    for (const auto path : paths_to_deduplicate)
    {
        cout << "Searching " << path << " for duplicates"
             << (recursive ? " recursively" : "") << endl;
        gather_hashes(path, dedup_table, bytes, recursive);
    }
    ch::duration<double, std::milli> elapsed_time =
        ch::steady_clock::now() - start_time;

    vector<vector<string>> duplicates;

    for (const auto &pair : dedup_table)
    {
        for (const auto &dup_vec : pair.second)
        {
            if (dup_vec.size() > 1)
            {
                duplicates.push_back(dup_vec);
            }
        }
    }

    bool list = result["list"].as<bool>();
    bool summarize = result["summarize"].as<bool>();

    if (duplicates.size() == 0)
    {
        cout << "Didn't find any duplicates." << endl;
    }
    else if (list)
    {
        cout << "\n" << "Found " << duplicates.size()
         << " files that have duplicates:\n\n";
        for (const auto &dup_vec : duplicates)
        {
            for (size_t i = 0; i < dup_vec.size(); i++)
            {
                cout << "[" << i << "] " << dup_vec[i] << "\n";
            }
            cout << endl;
        }
    }
    else if (summarize)
    {
        cout << "\n" << "Found " << duplicates.size()
         << " files that have duplicates" << endl;
    }
    
    else
    {
        prompt_duplicate_deletions(duplicates);
    }
    

    std::cout << "Gathering of hashes took " << elapsed_time.count()
              << " milliseconds." << std::endl;

    return 0;
}
