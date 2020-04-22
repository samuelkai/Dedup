#include "deal_with_duplicates.h"
#include "utilities.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using std::cerr;
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
    if (1 <= number && number <= upper_limit)
    {
        return number;
    }
    return 0;
}

/**
 * Splits the given string into a vector of tokens using the given delimiter.
 */
vector<string> split(const string &s, char delimiter)
{
   vector<string> tokens;
   string token;
   std::istringstream tokenStream(s);
   while (std::getline(tokenStream, token, delimiter))
   {
      tokens.push_back(token);
   }
   return tokens;
}

/**
 * Removes files in [files] which are not specified to be kept in [kept].
 * Kept includes indexes of kept files, and its indexing starts at 1 because of 
 * UI reasons.
 */
void remove_files(const vector<size_t> &kept, const DuplicateVector &files)
{
    for (size_t i = 1; i <= files.size(); ++i)
    {
        if (std::find(kept.begin(), kept.end(), i) == kept.end())
        {
            try
            {
                if (fs::last_write_time(files[i-1].path) > files[i-1].m_time)
                {
                    cerr << "File \"" << files[i-1].path << "\" has been "
                    "modified after it was scanned. Did not delete it.\n";
                }
                else
                {
                    if (fs::remove(files[i-1].path))
                    {
                        cout << "Deleted file \"" << files[i-1].path << "\"\n";
                    }
                    else
                    {
                        cerr << "File \"" << files[i-1].path
                             << "\" not found, could not delete it\n\n";
                    }
                }                
            }
            catch (const fs::filesystem_error &e)
            {
                cerr << e.what() << "\n\n";
            }
        }
        else
        {
            cout << "Kept file \"" << files[i-1].path << "\"\n";
        }
    }
    cout << '\n';
}

/**
 * Asks the user to select on the command line which files are kept 
 * in each set of duplicates.
 */
void prompt_duplicate_deletions(const vector<DuplicateVector> &duplicates)
{
    // For each set of duplicates
    for (const auto &dup_vec : duplicates)
    {
        // Print the paths of the set of duplicates
        for (size_t i = 1; i <= dup_vec.size(); ++i)
        {
            cout << "[" << i << "] " << dup_vec[i-1].path << '\n';
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
 * Keep only one file in each set of duplicates and replace the others with 
 * links to the one kept. Use hard link or symlink based on argument. Done in 
 * three stages in order to enable error recovery.
 */
void link_files(const DuplicateVector &files, bool hard_link)
{
    for (size_t i = 2; i <= files.size(); ++i)
    {
        fs::path target = files[0].path;
        fs::path link = files[i-1].path;

        fs::path temp_path = 
            (fs::path(link)
            += std::to_string(std::time(nullptr))) // seconds since the Epoch
            += ".deduptemp";
        
        try
        {
            if (fs::last_write_time(link) > files[i-1].m_time)
            {
                cerr << "File " << link << " has been "
                "modified after it was scanned. Did not hard link it.\n";
            }
            else
            {
                // First rename the duplicate to a temporary name
                try
                {
                    fs::rename(link, temp_path);
                }
                catch(const fs::filesystem_error &e)
                {
                    cerr << "Linking failed: " << e.what() << '\n';
                    continue;
                }

                // Then create the link
                try
                {
                    if (hard_link)
                    {
                        fs::create_hard_link(target, link);
                        cout << "Hard linked file " << link << " to file " 
                            << target << '\n';                     
                    }
                    else
                    {
                        fs::create_symlink(target, link);
                        cout << "Symlinked file " << link << " to file " 
                            << target << '\n';               
                    }
                }
                catch(const fs::filesystem_error &e)
                {
                    cerr << "Linking failed: " << e.what() << '\n';
                    // Recover the original name of the duplicate
                    fs::rename(temp_path, link);
                    continue;
                }

                // Finally remove the duplicate
                try
                {
                    if (!fs::remove(temp_path))
                    {
                        cerr << "Linking failed: Could not delete duplicate\n";
                        // Remove the created link
                        fs::remove(link);
                        // Recover the original name of the duplicate
                        fs::rename(temp_path, link);
                        continue;
                    }
                }
                catch(const fs::filesystem_error &e)
                {
                    cerr << "Linking failed: " << e.what() << '\n';
                    // Remove the created link
                    fs::remove(link);
                    // Recover the original name of the duplicate
                    fs::rename(temp_path, link);
                    continue;
                }
            }                
        }
        catch (const fs::filesystem_error &e)
        {
            cerr << "Linking failed: " << e.what() << '\n';
        }
    }
    cout << '\n';
}

/**
 * Deal with the given duplicates using the given action.
 */
void deal_with_duplicates(Action action, vector<DuplicateVector> duplicates)
{
    if (duplicates.empty())
    {
        cout << "Didn't find any duplicates." << endl;
        return;
    }

    size_t number_of_duplicate_files = 0;
    uintmax_t duplicates_size = 0;
    for (auto &dup_vec : duplicates)
    {
        // A set of n identical files has n - 1 duplicate files
        number_of_duplicate_files += dup_vec.size() - 1;
        duplicates_size += (dup_vec.size() - 1) 
            * fs::file_size(dup_vec[0].path);
        
        // Sort the duplicate vectors, file with oldest modification time first
        std::sort(dup_vec.begin(), dup_vec.end(), 
            [](const File &f1, const File &f2) 
            {
                return f1.m_time < f2.m_time;
            }
        );
    }

    cout << "Found " << number_of_duplicate_files
         << " duplicate file" << (number_of_duplicate_files > 1 ? "s" : "") 
         << " in " << duplicates.size() 
         << " set" << (duplicates.size() > 1 ? "s" : "") << ".\n"
         << format_bytes(duplicates_size) << " could be freed." << endl;     

    switch (action)
    {
    case Action::list:
        cout << '\n';
        for (const auto &dup_vec : duplicates)
        {
            for (const auto &file : dup_vec)
            {
                cout << file.path << '\n';
            }
            cout << endl;
        }
        break;

    case Action::no_prompt_delete:
        cout << '\n';
        for (const auto &dup_vec : duplicates)
        {
            vector<size_t> keep_only_first{1};
            remove_files(keep_only_first, dup_vec);
        }
        break;

    case Action::prompt_delete:
        cout << '\n';
        prompt_duplicate_deletions(duplicates);
        break;

    case Action::hardlink:
        cout << '\n';
        for (const auto &dup_vec : duplicates)
        {
            link_files(dup_vec, true);
        }
        break;

    case Action::symlink:
        cout << '\n';
        for (const auto &dup_vec : duplicates)
        {
            link_files(dup_vec, false);
        }
        break;

    default:
        break;
    }
}
