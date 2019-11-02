#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>

#include "gather_hashes.h"
#include "catch2/catch.hpp"

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
        // Todo: Keep all or none
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

string prompt_path(string instruction, bool use_ending_word = false,
                   string ending_word = "")
{
    string input = "";
    while (true)
    {
        cout << instruction << endl;
        getline(cin, input);
        if (fs::exists(input) || (use_ending_word && input == ending_word))
        {
            break;
        }
        cout << "File or directory \"" << input << "\" not found" << endl;
    }
    return input;
}

int main(int argc, char *argv[])
{
    string input;
    if (argc < 2)
    {
        input = prompt_path("Input a path (file or directory) to "
                    "be deduplicated");
    }
    else
    {
        input = argv[1];
        if (!fs::exists(input))
        {
            input = prompt_path("Input a path (file or directory) "
                        "to be deduplicated");
        }
    }

    fs::path input_path = fs::canonical(input);
    vector<fs::path> paths_to_deduplicate;
    paths_to_deduplicate.push_back(input_path);
    cout << "Added path " << input_path
            << " to be included in the deduplication" << endl;

    while (!(input = prompt_path("Input another path to be included in "
                                 "the deduplication or finish with Enter",
                                 true))
                .empty())
    {
        input_path = fs::canonical(input);
        if (std::find(paths_to_deduplicate.begin(), paths_to_deduplicate.end(),
                      input_path) == paths_to_deduplicate.end())
        {
            cout << "Added path " << input_path
                 << " to be included in the deduplication" << endl;
            paths_to_deduplicate.push_back(input_path);
        }
        else
        {
            cout << "Path " << input_path
                 << " is already included in the deduplication" << endl;
        }
    }

    DedupTable dedup_table;
    auto start_time = ch::steady_clock::now();
    for (auto path : paths_to_deduplicate)
    {
        gather_hashes(path, dedup_table, 1024, true);
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

    if (duplicates.size() == 0)
    {
        cout << "Didn't find any duplicates." << endl;
    } else {
        prompt_duplicate_deletions(duplicates);
    }

    std::cout << "Gathering of hashes took " << elapsed_time.count()
              << " milliseconds." << std::endl;

    return 0;
}
