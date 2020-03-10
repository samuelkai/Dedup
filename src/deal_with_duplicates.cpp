#include "deal_with_duplicates.h"
#include "utilities.h"

#include "cxxopts/cxxopts.hpp"

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
 * Checks if the given string is a valid number for selecting kept duplicates.
 * If it is, returns the number.
 * If it is not, returns 0 (which is not valid).
 */
size_t get_valid_number(const string &s, size_t upper_limit)
{
    std::istringstream iss(s);
    size_t number;
    iss >> number;
    if (iss.fail())
    {
        return 0;
    }
    else if (1 <= number && number <= upper_limit)
    {
        return number;
    }
    return 0;
}

/**
 * Splits the given string into a vector of tokens using the given delimiter.
 */
std::vector<std::string> split(const std::string &s, char delimiter)
{
   std::vector<std::string> tokens;
   std::string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}

/**
 * Removes files in [paths] which are not specified to be kept in [kept].
 * Kept includes indexes of kept files, and its indexing starts at 1 because of 
 * UI reasons.
 */
void remove_files(const vector<size_t> &kept, const vector<string> &paths)
{
    for (size_t i = 1; i <= paths.size(); ++i)
    {
        if (std::find(kept.begin(), kept.end(), i) == kept.end())
        {
            try
            {
                if (fs::remove(paths[i-1]))
                {
                    cout << "Removed file \"" << paths[i-1] << "\"\n";
                }
                else
                {
                    std::cerr << "File \"" << paths[i-1]
                            << "\" not found, could not "
                            << "delete it\n\n";
                }
            }
            catch (const fs::filesystem_error &e)
            {
                std::cerr << e.what() << "\n\n";
            }
        }
    }
    cout << '\n';
}

/**
 * Asks the user to select on the command line which files are kept 
 * in each set of duplicates.
 */
void prompt_duplicate_deletions(const vector<vector<string>> &duplicates)
{
    cout << '\n';
    // For each set of duplicates
    for (const auto &dup_vec : duplicates)
    {
        // Print the paths of the set of duplicates
        for (size_t i = 1; i <= dup_vec.size(); ++i)
        {
            cout << "[" << i << "] " << dup_vec[i-1] << '\n';
        }
        cout << endl;

        bool valid_input = false;
        while (!valid_input)
        {
            cout << "Select the file(s) to keep: [1-"
                 << dup_vec.size() << "], [a]ll or [n]one. "
                 "Separate numbers with space." << endl;

            string input;
            getline(cin, input);

            if (input == "n" || input == "none") // Remove all
            {
                valid_input = true;
                vector<size_t> empty;
                remove_files(empty, dup_vec);
            }
            else if (input == "a" || input == "all") // Keep all
            {
                valid_input = true;
            }
            else // Check if valid numbers were given
            {
                const vector<string> vec = split(input, ' ');
                vector<size_t> kept;
                for (const string &s : vec)
                {
                    size_t number = get_valid_number(s, dup_vec.size());
                    if (number != 0)
                    {
                        valid_input = true;
                        kept.push_back(number);
                    }
                    else
                    {
                        valid_input = false;
                        break;
                    }
                }

                if (valid_input)
                {
                    remove_files(kept, dup_vec);
                }
            }
        }        
    }
}

/**
 * Deal with the given duplicates according to the given parameters.
 * List, summarize or prompt for deletion.
 */
void deal_with_duplicates(const cxxopts::ParseResult &result, 
                          const vector<vector<string>> &duplicates)
{
    const bool list = result["list"].as<bool>();
    const bool summarize = result["summarize"].as<bool>();

    if (duplicates.empty())
    {
        cout << "Didn't find any duplicates." << endl;
        return;
    }

    size_t number_of_duplicate_files = 0;
    uintmax_t duplicates_size = 0;
    for (const auto &dup_vec : duplicates)
    {
        // A set of n identical files has n - 1 duplicate files
        number_of_duplicate_files += dup_vec.size() - 1;
        duplicates_size += (dup_vec.size() - 1) * fs::file_size(dup_vec[0]);
    }

    cout << "Found " << number_of_duplicate_files
         << " duplicate file" << (number_of_duplicate_files > 1 ? "s" : "") 
         << " in " << duplicates.size() 
         << " set" << (duplicates.size() > 1 ? "s" : "") << ".\n"
         << format_bytes(duplicates_size) << " could be freed." << endl; 

    if (list) // List found duplicates, don't prompt their deletion
    {
        cout << '\n';
        for (const auto &dup_vec : duplicates)
        {
            for (size_t i = 0; i < dup_vec.size(); ++i)
            {
                cout << dup_vec[i] << '\n';
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
