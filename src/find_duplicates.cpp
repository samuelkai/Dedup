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

constexpr int progress_bar_width = 70;

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

void draw_progress_bar(size_t total_count, size_t current_count) {
    float progress = (float)current_count / (float)total_count;
    cout << "[";
    int pos = progress_bar_width * progress;
    for (int i = 0; i < progress_bar_width; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << "] " << int(progress * 100.0) << " %\r";
    cout.flush();
}

class DirectoryOperation {
    public:
        virtual void increment() {};
        virtual size_t get_total_count() {return 0;};
        virtual size_t get_current_count() {return 0;};
        virtual size_t get_step_size() {return 0;};
        virtual void insert(const fs::path&) {};
};

template <typename T>
class InsertOperation : public DirectoryOperation {
        DedupTable<T> &dedup_table;
        uint64_t bytes;
        size_t current_count;
    public:
        InsertOperation(DedupTable<T> &d, uint64_t b) : dedup_table(d), 
            bytes(b), current_count(0) {}
        void insert(const fs::path &path) override {insert_into_dedup_table(path, dedup_table,
            bytes); current_count++;}
        virtual size_t get_total_count() {return 0;};
        virtual size_t get_current_count() {return current_count;};
        virtual size_t get_step_size() {return 0;};
};

template <typename T>
class ProgressInsertOperation : public InsertOperation<T> {
        size_t total_count;
        size_t step_size;
    public:
        ProgressInsertOperation(DedupTable<T> &d, uint64_t b, size_t t_c, size_t s) : InsertOperation<T>(d,b), total_count(t_c)
        {
            if (s == 0) {
                step_size = 1;
            }
            else {
                step_size = s;
            }
        }
        size_t get_total_count() override {return total_count;}
        size_t get_step_size() override {return step_size;}
};

class CountOperation : public DirectoryOperation {
        size_t count;
    public:
        CountOperation() : count(0) {};
        void increment() override {count++;}
        size_t get_count() {return count;}
};

inline void handle_file_path(const fs::path &path, bool just_count, 
    DirectoryOperation &op)
{
    if (just_count)
    {
        op.increment();
    }
    else
    {
        op.insert(path);
        if (op.get_step_size() > 0 && op.get_current_count() % op.get_step_size() == 0)
        {
            draw_progress_bar(op.get_total_count(), op.get_current_count());
        }
    }
}

void traverse_directory_recursively(const fs::path &directory, bool just_count, DirectoryOperation &op)
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
            handle_file_path(iter_path, just_count, op);
        }
    }
}

/**
 * Traverses the given path for files, which are then checked for duplicates.
 * Only regular files are checked.
 * Directories are traversed recursively if wanted.
 */
void traverse_path(const fs::path &path, bool recursive, bool just_count, DirectoryOperation &op)
{            
    if (fs::is_directory(path))
    {
        if (recursive)
        {
            traverse_directory_recursively(path, just_count, op);
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
                    handle_file_path(iter_path, just_count, op);
                }
            }
        }
    }
    else if (fs::is_regular_file(fs::symlink_status(path)))
    {
        handle_file_path(path, just_count, op);
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
        CountOperation cop = CountOperation();
        auto start_time_count = ch::steady_clock::now();
        for (const auto &path : paths_to_deduplicate)
        {
            traverse_path(path, recursive, true, cop);
        }
        ch::duration<double, std::milli> elapsed_time_count =
            ch::steady_clock::now() - start_time_count;
        size_t total_count = cop.get_count();
        cout << "Counting of " << total_count << " files took " << elapsed_time_count.count()
                << " milliseconds." << endl;

        ProgressInsertOperation piop = ProgressInsertOperation(dedup_table, 
        bytes, total_count, total_count / 20);
        auto start_time = ch::steady_clock::now();
        for (const auto &path : paths_to_deduplicate)
        {
            try
            {
                cout << "Searching " << path << " for duplicates"
                    << (recursive ? " recursively" : "") << endl;
                traverse_path(path, recursive, false, piop);
            }
            catch (const std::exception &e)
            {
                cerr << e.what() << "\n\n";
            }
        }
        ch::duration<double, std::milli> elapsed_time =
            ch::steady_clock::now() - start_time;

        for (int i = 0; i < progress_bar_width + 10; ++i) {
            cout << " ";
        }

        cout << "\r" << "Done checking " << piop.get_current_count() << " files."
             << endl << "Gathering of hashes took " << elapsed_time.count()
             << " milliseconds." << endl;
    }
    else
    {
        InsertOperation iop = InsertOperation(dedup_table, bytes);
        auto start_time = ch::steady_clock::now();
        for (const auto &path : paths_to_deduplicate)
        {
            try
            {
                cout << "Searching " << path << " for duplicates"
                    << (recursive ? " recursively" : "") << endl;
                traverse_path(path, recursive, false, iop);
            }
            catch (const std::exception &e)
            {
                cerr << e.what() << "\n\n";
            }
        }
        ch::duration<double, std::milli> elapsed_time =
            ch::steady_clock::now() - start_time;

        cout << "\r" << "Done checking " << iop.get_current_count() << " files."
             << endl << "Gathering of hashes took " << elapsed_time.count()
             << " milliseconds." << endl;
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
