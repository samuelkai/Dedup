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

class DirectoryOperation {
        bool b_has_progress;
    public:
        DirectoryOperation(bool p)
            : b_has_progress(p) {}

        virtual void insert(const fs::path&) = 0;
        bool has_progress()
        {
            return b_has_progress;
        }
};

template <typename T>
class InsertOperation : public DirectoryOperation {
        DedupTable<T> &dedup_table;
        uint64_t bytes;
        size_t current_count;
        uintmax_t current_size;
    public:
        InsertOperation(DedupTable<T> &d, uint64_t b, bool p = false)
            : DirectoryOperation(p), dedup_table(d), bytes(b), current_count(0),
              current_size(0) {}
        
        void insert(const fs::path &path) override
        {
            insert_into_dedup_table(path, dedup_table, bytes);
            current_count++;
            current_size += fs::file_size(path);
        }

        virtual size_t get_current_count() {return current_count;}
        virtual size_t get_current_size() {return current_size;}
};

template <typename T>
class ProgressInsertOperation : public InsertOperation<T> {
        size_t total_count;
        size_t step_size;
    public:
        ProgressInsertOperation(DedupTable<T> &d, uint64_t b, size_t t_c, 
            size_t s)
            : InsertOperation<T>(d, b, true), total_count(t_c)
        {
            if (s == 0) {
                step_size = 1;
            }
            else {
                step_size = s;
            }
        }

        size_t get_total_count() {return total_count;}
        size_t get_step_size() {return step_size;}
};

class CountOperation : public DirectoryOperation {
        size_t count;
        uintmax_t size;
    public:
        CountOperation()
            : DirectoryOperation(false), count(0), size(0) {};

        void insert(const fs::path &path) override
        {
            count++;
            size += fs::file_size(path);
        }

        size_t get_count() {return count;}
        uintmax_t get_size() {return size;}
};

/**
 * Draws a bar on the terminal showing the progress on finding duplicates.
 */
template <typename T>
void draw_progress(ProgressInsertOperation<T> &op) {
    size_t curr_f_cnt = op.get_current_count();
    size_t tot_f_cnt = op.get_total_count();
    size_t step_size = op.get_step_size();

    if (step_size > 0 && curr_f_cnt % step_size == 0)
    {
        float progress = static_cast<float>(curr_f_cnt) 
            / static_cast<float>(tot_f_cnt);
        cout << "\r" << "File " << curr_f_cnt << "/" << tot_f_cnt 
            << " (" << int(progress * 100.0) << " %)";
        cout.flush();
    }
}

template <typename T>
inline void handle_file_path(const fs::path &path, DirectoryOperation &op)
{
    op.insert(path);
    if (op.has_progress())
    {
        draw_progress(static_cast<ProgressInsertOperation<T>&>(op));
    }
}

template <typename T>
void traverse_directory_recursively(const fs::path &directory, DirectoryOperation &op)
{    
    fs::recursive_directory_iterator iter(directory);
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
            handle_file_path<T>(iter_path, op);
        }
    }
}

/**
 * Traverses the given path for files, which are then checked for duplicates.
 * Only regular files are checked.
 * Directories are traversed recursively if wanted.
 */
template <typename T>
void traverse_path(const fs::path &path, bool recursive, DirectoryOperation &op)
{            
    if (fs::is_directory(path))
    {
        if (recursive)
        {
            traverse_directory_recursively<T>(path, op);
        }
        else
        {
            fs::directory_iterator iter(path);                    
            fs::directory_iterator end; 
            for (; iter != end; ++iter)
            {
                const fs::path iter_path(iter->path());
                if (fs::is_regular_file(fs::symlink_status(iter_path)))
                {
                    handle_file_path<T>(iter_path, op);
                }
            }
        }
    }
    else if (fs::is_regular_file(fs::symlink_status(path)))
    {
        handle_file_path<T>(path, op);
    }
    else // not a regular file or directory
    {
        cout << path << " exists, but is not a regular file "
                        "or directory\n\n";
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
vector<vector<fs::path>> find_duplicates(const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate)
{
    DedupTable<T> dedup_table;
    uint64_t bytes = result["bytes"].as<uint64_t>();
    bool recursive = result["recursive"].as<bool>();
    bool skip_count = result["skip-count"].as<bool>();

    if (!skip_count)
    {
        cout << "Counting number and size of files in given paths..." << endl;
        CountOperation cop = CountOperation();
        
        for (const auto &path : paths_to_deduplicate)
        {
            traverse_path<T>(path, recursive, cop);
        }
        
        size_t total_count = cop.get_count();
        uintmax_t total_size = cop.get_size();
        cout << "Counted " << total_count << " files occupying "
             << format_bytes(total_size) << "." << endl;


        ProgressInsertOperation piop = ProgressInsertOperation<T>(dedup_table, 
        bytes, total_count, total_count / 20);
        for (const auto &path : paths_to_deduplicate)
        {
            try
            {
                cout << "Searching " << path << " for duplicates"
                    << (recursive ? " recursively" : "") << "..." << endl;
                traverse_path<T>(path, recursive, piop);
            }
            catch (const std::exception &e)
            {
                cerr << e.what() << "\n\n";
            }
        }
        cout << "\r" << "Done checking " << total_count 
             << " files occupying " << format_bytes(total_size) << "."
             << endl << endl;
    }
    else
    {
        InsertOperation iop = InsertOperation<T>(dedup_table, bytes);
        for (const auto &path : paths_to_deduplicate)
        {
            try
            {
                cout << "Searching " << path << " for duplicates"
                    << (recursive ? " recursively" : "") << "..." << endl;
                traverse_path<T>(path, recursive, iop);
            }
            catch (const std::exception &e)
            {
                cerr << e.what() << "\n\n";
            }
        }

        size_t total_count = iop.get_current_count();
        uintmax_t total_size = iop.get_current_size();
        cout << "\r" << "Done checking " << total_count 
             << " files occupying " << format_bytes(total_size) << "."
             << endl << endl;
    }
        
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
