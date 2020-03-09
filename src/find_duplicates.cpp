#include "find_duplicates.h"
#include "utilities.h"

#include "cxxopts/cxxopts.hpp"

#include <filesystem>
#include <iostream>
#include <set>
#include <unordered_map>
#include <vector>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

namespace fs = std::filesystem;

// Stores file paths and hashes of file contents.
// The key is the hash of the beginning N bytes of a file,
// where N is a program argument.
// The key type T is one of {uint8_t, uint16_t, uint32_t, uint64_t}.
// The value contains the paths of all files that produce the same hash.
// Because files can differ after the first N bytes, the outer vector contains
// inner vectors that contain files whose whole content is the same.
template <typename T>
using DedupTable = std::unordered_map<
    uintmax_t,
    std::unordered_map<
        T,
        vector<
            vector<
                string>
            >
        > 
    >;

using FileSizeTable = std::unordered_map<uintmax_t, vector<string>>;

/**
 * Checks the given vector of duplicate file vectors for a file that has the
 * same content as the file in the given path. If found, inserts the path
 * to the duplicate file vector and returns true.
 */ 
bool find_duplicate_file(const string &path,
                         vector<vector<string>> &vec_vec)
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
        catch(const FileException &e)
        {
            // By catching here we can compare to the other elements of dup_vec
            std::cerr << e.what() << '\n';
        }                           
    }
    return false;
}

/**
 * Inserts the given path into the deduplication table.
 */
template <typename T>
void insert_into_dedup_table(const string &path, uintmax_t size,
                             DedupTable<T> &dedup_table, uint64_t bytes)
{   
        // Truncate the hash to the specified length
        const auto hash = static_cast<T>(hash_file(path, bytes));

        if (dedup_table[size][hash].empty()) // First file that produces this hash
        {
            dedup_table[size][hash].push_back(
                vector<string>{path});
        }
        else
        {
            if (!find_duplicate_file(
                path, dedup_table[size][hash]))
            {
                // File differs from others with the same hash
                dedup_table[size][hash].push_back(
                    vector<string>{path});
            }
        }
}

template <typename T>
class InsertOperation {
        DedupTable<T> &dedup_table;
        const uint64_t bytes;
        size_t current_count;
        const size_t total_count;
        const size_t step_size;
    
    public:
        InsertOperation(DedupTable<T> &d, uint64_t b, size_t t_c, size_t s_s)
            : dedup_table(d), bytes(b), current_count(0), total_count(t_c), 
              step_size( s_s == 0 ? 1 : s_s ) {}; // Prevent zero step size

        void insert(const string &path, uintmax_t size)
        {
            insert_into_dedup_table(path, size, dedup_table, bytes);
            ++current_count;
        }

        size_t get_current_count() const {return current_count;}
        size_t get_total_count() const {return total_count;}
        size_t get_step_size() const {return step_size;}
};

class CountOperation {
        size_t count;
        uintmax_t size;
        FileSizeTable &file_size_table;
    public:
        CountOperation(FileSizeTable &f)
            : count(0), size(0), file_size_table(f)
             {};

        void insert(const fs::directory_entry &entry)
        {
            try
            {
                if (fs::is_regular_file(fs::symlink_status(entry.path()))
                    && !fs::is_empty(entry.path()))
                {
                    ++count;
                    size += entry.file_size();
                    file_size_table[entry.file_size()]
                        .push_back(entry.path().string());
                }
            }
            catch(const fs::filesystem_error &e)
            {
                std::cerr << e.what() << '\n';
            }
            catch(const std::runtime_error &e)
            {
                std::cerr << e.what() << " ["
                    << entry.path().string() << "]\n";
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }

        size_t get_count() const {return count;}
        uintmax_t get_size() const {return size;}
};

/**
 * Draws a bar on the terminal showing the progress on finding duplicates.
 */
template <typename T>
void draw_progress(InsertOperation<T> &op) {
    const size_t curr_f_cnt = op.get_current_count();
    const size_t tot_f_cnt = op.get_total_count();
    const size_t step_size = op.get_step_size();

    if (step_size > 0 && curr_f_cnt % step_size == 0)
    {
        const float progress = static_cast<float>(curr_f_cnt) 
            / static_cast<float>(tot_f_cnt);
        cout << "\r" << "File " << curr_f_cnt << "/" << tot_f_cnt 
            << " (" << int(progress * 100.0) << " %)";
        cout.flush();
    }
}

/**
 * Traverses the given path for files, which are then checked for duplicates.
 * Only regular files are checked.
 * Directories are traversed recursively if wanted.
 */
template <typename T>
void traverse_path(const fs::path &path, bool recurse, CountOperation &op)
{
    if (fs::is_directory(path))
    {
        if (recurse)
        {            
            for (const auto &p : fs::recursive_directory_iterator(path, 
                fs::directory_options::skip_permission_denied))
            {
                op.insert(p);
            }
        }
        else
        {
            for (const auto &p : fs::directory_iterator(path, 
                fs::directory_options::skip_permission_denied))
            {
                op.insert(p);
            }
        }
    }
    else
    {
        op.insert(fs::directory_entry(path));
    }
}

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
vector<vector<string>> find_duplicates(const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate)
{
    DedupTable<T> dedup_table;

    const uint64_t bytes = result["bytes"].as<uint64_t>();
    const bool recurse = result["recurse"].as<bool>();
    
    FileSizeTable file_size_table;
    cout << "Counting number and size of files in given paths..." << endl;
    CountOperation cop = CountOperation(file_size_table);
    
    for (const auto &path : paths_to_deduplicate)
    {
        try
        {
            traverse_path<T>(path, recurse, cop);
        }
        catch(const std::exception &e)
        {
            cerr << e.what() << '\n';
        }
        
    }
    
    const size_t total_count = cop.get_count();
    const uintmax_t total_size = cop.get_size();
    cout << "Counted " << total_count << " files occupying "
            << format_bytes(total_size) << "." << endl;

    {
        FileSizeTable::iterator iter = file_size_table.begin();
        FileSizeTable::iterator end_iter = file_size_table.end();

        size_t no_unique_file_sizes = 0;

        for(; iter != end_iter; )
        {
            if (iter->second.size() == 1)
            {
                iter = file_size_table.erase(iter);
                ++no_unique_file_sizes;
            }
            else
            {
                ++iter;
            }
        }

        cout << "Removed " << no_unique_file_sizes << " files with unique size "
        "from deduplication.\n";

        dedup_table.reserve(file_size_table.size());
    }

    {
        InsertOperation<T> iop = InsertOperation<T>(dedup_table, 
        bytes, total_count, total_count / 20);

        FileSizeTable::iterator iter = file_size_table.begin();
        FileSizeTable::iterator end_iter = file_size_table.end();

        for (; iter != end_iter;)
        {
            for (const auto &path : iter->second)
            {
                try
                {
                    iop.insert(path, iter->first);
                }
                catch(const fs::filesystem_error &e)
                {
                    std::cerr << e.what() << '\n';
                }
                catch(const std::runtime_error &e)
                {
                    std::cerr << e.what() << " ["
                        << path << "]\n";
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }
                draw_progress(iop);
            }
            iter = file_size_table.erase(iter);
        }
    }

    cout << "\r" << "Done checking." << endl;

    // Includes vectors of files whose whole content is the same
    vector<vector<string>> duplicates;

    for (const auto &pair : dedup_table)
    {
        for (const auto &inner_pair : pair.second)
        {
            for (const auto &dup_vec : inner_pair.second)
            {
                if (dup_vec.size() > 1)
                {
                    duplicates.push_back(dup_vec);
                }
            }
        }
    }

    return duplicates;
}

template vector<vector<string>> find_duplicates<uint8_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<vector<string>> find_duplicates<uint16_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<vector<string>> find_duplicates<uint32_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<vector<string>> find_duplicates<uint64_t>(
    const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate);
