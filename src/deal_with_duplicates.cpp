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

bool is_positive_integer(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

void prompt_duplicate_deletions(const vector<vector<fs::path>> duplicates)
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
            if (std::find(keywords.begin(), keywords.end(), input) 
                    != keywords.end())
            {
                if (input == "n" || input == "none")
                {
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
                break;
            }
        }        
    }
}

void deal_with_duplicates(const cxxopts::ParseResult &result, const vector<vector<fs::path>> duplicates)
{
    bool list = result["list"].as<bool>();
    bool summarize = result["summarize"].as<bool>();

    if (duplicates.empty())
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
}
