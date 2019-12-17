#include "find_duplicates.h"
#include "utilities.h"

#include "cxxopts/cxxopts.hpp"

#include <chrono>
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

/**
 * Checks the given vector of duplicate file vectors for a file that has the
 * same content as the file in the given path. If found, inserts the path
 * to the duplicate file vector and returns true.
 */ 
bool find_duplicate_file(const fs::path &path,
                         vector<vector<fs::path>> &vec_vec)
{
    // vec_vec contains files that have the same hash
    // dup_vec contains files whose whole content is the same
    for (auto &dup_vec: vec_vec)
    {
        try
        {
            if (compare_files(path, dup_vec[0]))
            {
                dup_vec.push_back(path);
                return true;
            }             
        }
        catch(const FileException& e)
        {
            std::cerr << e.what() << '\n';
        }                           
    }
    return false;
}

/**
 * Inserts the given path into the deduplication table.
 */
template <typename T>
void insert_into_dedup_table(const fs::path &path, DedupTable<T> &dedup_table,
                             uint64_t bytes)
{
    try
    {
        if (fs::is_empty(path))
        {
            return;
        }

        
        auto hash = hash_file(path, bytes);
        // Truncate the hash to the specified length
        hash = static_cast<T>(hash);

        if (dedup_table[hash].empty()) // First file that produces this hash
        {
            dedup_table[hash].push_back(
                vector<fs::path>{path});
        }
        else
        {
            if (!find_duplicate_file(
                path, dedup_table[hash]))
            {
                // File differs from others with the same hash
                dedup_table[hash].push_back(
                    vector<fs::path>{path});
            }
        }
    }
    catch(const std::exception& e)
    {
        cerr << e.what() << ": file " << path << '\n';
    }
}

/**
 * Traverses the given path for files, which are then checked for duplicates.
 * Only regular files are checked.
 * Directories are traversed recursively if wanted.
 */
template <typename T>
void traverse_path(const fs::path &path, DedupTable<T> &dedup_table,
                   uint64_t bytes, bool recursive)
{
    try
    {
        if (fs::exists(path))
        {
            if (fs::is_directory(path))
            {
                if (recursive)
                {
                    try
                    {
                        fs::recursive_directory_iterator iter(path);
                        fs::recursive_directory_iterator end;
                        for (; iter != end; ++iter)
                        {
                            const fs::path iter_path(iter->path());

                            // Check directory permissions before accessing it,
                            // so the iterator won't be destroyed on access
                            // denied error 
                            if (fs::is_directory(iter_path))
                            {
                                try
                                {
                                    fs::path path_before_change
                                             = fs::current_path();
                                    // Try to cd to the directory
                                    // so its files can be accessed
                                    fs::current_path(iter_path);
                                    fs::current_path(path_before_change);
                                    // Try to read the file list 
                                    // of the directory
                                    fs::directory_iterator read_chk(iter_path);
                                }
                                catch(const std::exception& e)
                                {
                                    cerr << "Cannot access directory " 
                                         << iter_path << "\n";
                                    iter.disable_recursion_pending();
                                }
                            }
                            else if (fs::is_regular_file(
                                     fs::symlink_status(iter_path)))
                            {
                                insert_into_dedup_table(
                                    iter_path, dedup_table, bytes);
                            }
                        }
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                        return;
                    }
                }
                else // not recursive
                {
                    fs::directory_iterator iter(path);                    
                    fs::directory_iterator end; 
                    for (; iter != end; ++iter)
                    {
                        const fs::path iter_path(iter->path());
                        if (fs::is_regular_file(fs::symlink_status(iter_path)))
                        {
                            insert_into_dedup_table(
                                iter_path, dedup_table, bytes);
                        }
                    }
                }
            }
            else if (fs::is_regular_file(fs::symlink_status(path)))
            {
                insert_into_dedup_table(path, dedup_table, bytes);
            }
            else // not a regular file or directory
            {
                cout << path << " exists, but is not a regular file "
                                "or directory\n\n";
            }
        }
        else // does not exist
        {
            cout << path << " does not exist\n\n";
        }
    }
    catch (const fs::filesystem_error &e)
    {
        cerr << e.what() << "\n\n";
    }
}

template void traverse_path<uint8_t>(const std::filesystem::path &path,
                   DedupTable<uint8_t> &dedup_table, uint64_t bytes,
                   bool recursive);
template void traverse_path<uint16_t>(const std::filesystem::path &path,
                   DedupTable<uint16_t> &dedup_table, uint64_t bytes,
                   bool recursive);
template void traverse_path<uint32_t>(const std::filesystem::path &path,
                   DedupTable<uint32_t> &dedup_table, uint64_t bytes,
                   bool recursive);
template void traverse_path<uint64_t>(const std::filesystem::path &path,
                   DedupTable<uint64_t> &dedup_table, uint64_t bytes,
                   bool recursive);

/**
 * Calculates hash values of files and stores them
 * in the deduplication table.
 * 
 * Path can be a file or a directory.
 * If it is a file, its hash is calculated.
 * If it is a directory, the hash of each regular file in the directory 
 * (recursively or not, depending on the argument) will be calculated.
 * 
 * Returns a vector whose elements are vectors of duplicate files.
 */
template <typename T>
vector<vector<fs::path>> find_duplicates(const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate)
{
    DedupTable<T> dedup_table;
    uint64_t bytes = result["bytes"].as<uint64_t>();
    bool recursive = result["recursive"].as<bool>();
    
    auto start_time = ch::steady_clock::now();
    for (const auto &path : paths_to_deduplicate)
    {
        cout << "Searching " << path << " for duplicates"
            << (recursive ? " recursively" : "") << endl;
        traverse_path(path, dedup_table, bytes, recursive);
    }
    ch::duration<double, std::milli> elapsed_time =
        ch::steady_clock::now() - start_time;
    cout << "Gathering of hashes took " << elapsed_time.count()
              << " milliseconds." << endl;
    
    // Includes vectors of files whose whole content is the same
    vector<vector<fs::path>> duplicates;

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

    return duplicates;
}

template vector<vector<fs::path>> find_duplicates<uint8_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<vector<fs::path>> find_duplicates<uint16_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<vector<fs::path>> find_duplicates<uint32_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<vector<fs::path>> find_duplicates<uint64_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
