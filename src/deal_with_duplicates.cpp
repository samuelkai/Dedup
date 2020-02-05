#include "deal_with_duplicates.h"

#include "cxxopts/cxxopts.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::vector;

namespace fs = std::filesystem;

/**
 * Returns true if the given string is a positive integer.
 */
bool is_positive_integer(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

/**
 * Asks the user to select on the command line which files are kept 
 * in each set of duplicates.
 */
void prompt_duplicate_deletions(const vector<vector<fs::path>> duplicates)
{
    cout << endl;
    // For each set of duplicates
    for (const auto &dup_vec : duplicates)
    {
        for (size_t i = 1; i <= dup_vec.size(); i++)
        {
            cout << "[" << i << "] " << dup_vec[i-1].string() << "\n";
        }
        cout << endl;

        string input;
        bool valid_input = false;
        while (!valid_input)
        {
            cout << "Select the file(s) to keep: [1-"
                 << dup_vec.size() << "], [a]ll or [n]one" << endl;
            getline(cin, input);
            if (is_positive_integer(input))
            {
                try
                {
                    unsigned int kept = std::stoi(input);
                    // Given number must specify one of the duplicates
                    if (kept <= dup_vec.size())
                    {
                        for (size_t i = 1; i <= dup_vec.size(); i++)
                        {
                            // Remove all others
                            if (i != kept)
                            {
                                valid_input = true;
                                try
                                {
                                    if (!fs::remove(dup_vec[i-1]))
                                    {
                                        std::cerr << "File \"" << dup_vec[i-1]
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
                    }
                }
                catch(const std::out_of_range& e)
                {
                }
            }
            else if (input == "n" || input == "none") // Remove all
            {
                valid_input = true;
                for (const auto &path : dup_vec)
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
            else if (input == "a" || input == "all") // Keep all
            {
                valid_input = true;
            }
        }        
    }
}

/**
 * Deal with the given duplicates according to the given parameters.
 * List, summarize or prompt for deletion.
 */
void deal_with_duplicates(const cxxopts::ParseResult &result, 
                          const vector<vector<fs::path>> duplicates)
{
    bool list = result["list"].as<bool>();
    bool summarize = result["summarize"].as<bool>();

    if (duplicates.empty())
    {
        cout << "Didn't find any duplicates." << endl;
        return;
    }

    unsigned int number_of_duplicate_files = 0;
    for (const auto &dup_vec : duplicates)
    {
        // A set of n identical files has n - 1 duplicate files
        number_of_duplicate_files += dup_vec.size() - 1;
    }

    cout << "\n" << "Found " << number_of_duplicate_files
         << " duplicate files in " << duplicates.size() << " sets." << endl; 

    if (list) // List found duplicates, don't prompt their deletion
    {
        cout << "\n";
        for (const auto &dup_vec : duplicates)
        {
            for (size_t i = 0; i < dup_vec.size(); i++)
            {
                cout << dup_vec[i].string() << "\n";
            }
            cout << endl;
        }
    }
    else if (summarize)
    {
        // Just the previously printed summary, don't prompt deletion
    }    
    else
    {
        prompt_duplicate_deletions(duplicates);
    }
}
