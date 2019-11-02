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

void gather_hashes(const fs::path path, DedupTable &dedup_table)
{
    try
    {
        if (fs::exists(path))
        {
            if (fs::is_regular_file(path))
            {
                try {
                    dedup_table[hash_file(path)].push_back(path);
                } catch (const std::exception &ex) {
                    cerr << ex.what() << ": file " << path << '\n';
                }
            }
            else if (fs::is_directory(path))
            {
                for (const fs::directory_entry &dir_entry :
                     fs::directory_iterator(path))
                {
                    // Doesn't follow symlinks
                    if (fs::is_regular_file(fs::symlink_status(dir_entry)))
                    {
                        try {
                            fs::path dir_entry_path = dir_entry.path();
                            dedup_table[hash_file(dir_entry_path)]
                                .push_back(dir_entry_path);
                        } catch (const std::exception &ex) {
                           cerr << ex.what() << ": file " << dir_entry << '\n';
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