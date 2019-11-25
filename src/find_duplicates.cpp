#include "find_duplicates.h"
#include "utilities.h"

#include "cxxopts/cxxopts.hpp"

#include <bitset>
#include <filesystem>
#include <iostream>
#include <set>
#include <unordered_map>
#include <vector>

using std::cout;
using std::cerr;
using std::cin;
using std::endl;
using std::string;
using std::vector;

namespace ch = std::chrono;
namespace fs = std::filesystem;

bool find_duplicate_file(const string &path, vector<vector<string>> &vec_vec)
{
    for (auto &dup_vec: vec_vec)
    {
        if (compare_files(path, dup_vec[0]))
        {
            dup_vec.push_back(path);
            return true;
        }                                
    }
    return false;
}

template <typename T>
void insert_into_dedup_table(const string &path, DedupTable<T> &dedup_table,
                             uint64_t bytes)
{
    if (fs::is_empty(path))
    {
        return;
    }

    auto hash = bytes == 0 ? hash_file(path) : hash_file(path, bytes);
    hash = static_cast<T>(hash);

    if (dedup_table[hash].empty())
    {
        dedup_table[hash].push_back(
            vector<string>{path});
    }
    else
    {
        if (!find_duplicate_file(
            path, dedup_table[hash]))
        {
            dedup_table[hash].push_back(
                vector<string>{path});
        }
    }
}

template <typename T>
void gather_hashes(const fs::path &path, DedupTable<T> &dedup_table,
                   uint64_t bytes, bool recursive)
{
    try
    {
        if (fs::exists(path))
        {
            if (fs::is_regular_file(path))
            {
                try {
                    insert_into_dedup_table(path, dedup_table, bytes);
                } catch (const std::exception &ex) {
                    cerr << ex.what() << ": file " << path << '\n';
                }
            }
            else if (fs::is_directory(path))
            {
                if (recursive)
                {                        
                    for (const fs::directory_entry &dir_entry :
                        fs::recursive_directory_iterator(path))
                    {
                        // Doesn't follow symlinks
                        if (fs::is_regular_file(fs::symlink_status(dir_entry)))
                        {
                            try {
                                const fs::path &dir_entry_path = dir_entry.path();
                                insert_into_dedup_table(dir_entry_path, dedup_table, bytes);
                            } catch (const std::exception &ex) {
                                cerr << ex.what() << ": file " << dir_entry << "\n\n";
                            }
                        }
                    }
                }
                else
                {
                    for (const fs::directory_entry &dir_entry :
                        fs::directory_iterator(path))
                    {
                        // Doesn't follow symlinks
                        if (fs::is_regular_file(fs::symlink_status(dir_entry)))
                        {
                            try {
                                const fs::path &dir_entry_path = dir_entry.path();
                                insert_into_dedup_table(dir_entry_path, dedup_table, bytes);
                            } catch (const std::exception &ex) {
                                cerr << ex.what() << ": file " << dir_entry << "\n\n";
                            }
                        }
                    }
                }
                
            }
            else
            {
                cout << path << " exists, but is not a regular file "
                                "or directory\n\n";
            }
        }
        else
        {
            cout << path << " does not exist\n\n";
        }
    }
    catch (const fs::filesystem_error &ex)
    {
        cerr << ex.what() << "\n\n";
    }
}

template void gather_hashes<uint8_t>(const std::filesystem::path &path,
                   DedupTable<uint8_t> &dedup_table, uint64_t bytes,
                   bool recursive);
template void gather_hashes<uint16_t>(const std::filesystem::path &path,
                   DedupTable<uint16_t> &dedup_table, uint64_t bytes,
                   bool recursive);
template void gather_hashes<uint32_t>(const std::filesystem::path &path,
                   DedupTable<uint32_t> &dedup_table, uint64_t bytes,
                   bool recursive);
template void gather_hashes<uint64_t>(const std::filesystem::path &path,
                   DedupTable<uint64_t> &dedup_table, uint64_t bytes,
                   bool recursive);

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

template <typename T>
void find_duplicates(const cxxopts::ParseResult &result, const std::set<fs::path> &paths_to_deduplicate)
{
    DedupTable<T> dedup_table;
    uint64_t bytes = result["bytes"].as<uint64_t>();
    bool recursive = result["recursive"].as<bool>();
    
    auto start_time = ch::steady_clock::now();
    for (const auto &path : paths_to_deduplicate)
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
    
    std::cout << "Gathering of hashes took " << elapsed_time.count()
              << " milliseconds." << std::endl;
}

template void find_duplicates<uint8_t>(const cxxopts::ParseResult &result, const std::set<fs::path> &paths_to_deduplicate);
template void find_duplicates<uint16_t>(const cxxopts::ParseResult &result, const std::set<fs::path> &paths_to_deduplicate);
template void find_duplicates<uint32_t>(const cxxopts::ParseResult &result, const std::set<fs::path> &paths_to_deduplicate);
template void find_duplicates<uint64_t>(const cxxopts::ParseResult &result, const std::set<fs::path> &paths_to_deduplicate);
