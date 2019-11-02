#include <unordered_map>
#include <vector>
#include <filesystem>
#include <iostream>

#include "gather_hashes.h"
#include "utilities.h"

using std::cin;
using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::vector;

namespace fs = std::filesystem;

bool find_duplicate_file(const fs::path path, vector<vector<string>> &vec_vec)
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

void insert_into_dedup_table(const fs::path path,
                             DedupTable &dedup_table, uint64_t bytes)
{
    auto hash = bytes == 0 ? hash_file(path) : hash_file(path, bytes);
    if (dedup_table[hash].size() == 0)
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

void gather_hashes(const fs::path path, DedupTable &dedup_table,
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
                                fs::path dir_entry_path = dir_entry.path();
                                insert_into_dedup_table(dir_entry_path, dedup_table, bytes);
                            } catch (const std::exception &ex) {
                            cerr << ex.what() << ": file " << dir_entry << '\n';
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
                                fs::path dir_entry_path = dir_entry.path();
                                insert_into_dedup_table(dir_entry_path, dedup_table, bytes);
                            } catch (const std::exception &ex) {
                            cerr << ex.what() << ": file " << dir_entry << '\n';
                            }
                        }
                    }
                }
                
            }
            else
            {
                cout << path << " exists, but is not a regular file "
                                "or directory\n";
            }
        }
        else
        {
            cout << path << " does not exist\n";
        }
    }
    catch (const fs::filesystem_error &ex)
    {
        cerr << ex.what() << '\n';
    }
}
