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

/**
 * Stores Files, their sizes and hashes of file contents.
 * The key of the outer map is file size.
 * The key of the inner map is the hash of the beginning N bytes of a file,
 * where N is a program argument.
 * The key type T is one of {uint8_t, uint16_t, uint32_t, uint64_t}.
 * The value of the inner map contains the paths of all files that produce the 
 * same hash. Because files can differ after the first N bytes, the outer vector
 * contains inner vectors that contain Files whose whole content is the same.
 */
template <typename T>
using DedupTable = std::unordered_map<
    uintmax_t,
    std::unordered_map<
        T,
        vector<DuplicateVector>
    >
>;

/**
 * Stores Files grouped by their size. Used during the file scanning phase.
 */
using FileSizeTable = std::unordered_map<uintmax_t, vector<File>>;

/**
 * Checks the given vector of duplicate file vectors for a file that has the
 * same content as the file in the given path. If found, inserts the path
 * to the duplicate file vector and returns true.
 */ 
bool find_duplicate_file(const File &file,
                         vector<DuplicateVector> &vec_vec)
{
    // vec_vec contains Files that have the same hash
    // dup_vec contains Files whose whole content is the same
    for (auto &dup_vec: vec_vec)
    {
        try
        {
            if (compare_files(file.path, dup_vec[0].path))
            {
                // Identical to the files in dup_vec
                dup_vec.push_back(file);
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
 * Inserts the given File into the deduplication table.
 */
template <typename T>
void insert_into_dedup_table(const File &file, uintmax_t size,
                             DedupTable<T> &dedup_table, uintmax_t bytes)
{   
    // Calculate the hash and truncate it to the specified length
    const auto hash = static_cast<T>(hash_file(file.path, bytes));

    if (dedup_table[size][hash].empty()) // First file that produces this hash
    {
        dedup_table[size][hash].push_back(
            vector<File>{file});
    }
    else
    {
        if (!find_duplicate_file(
            file, dedup_table[size][hash]))
        {
            // File differs from others with the same hash
            dedup_table[size][hash].push_back(
                vector<File>{file});
        }
    }
}

/**
 * Manages the deduplication. Stores progress information and inserts Files to
 * dedup_table.
 */
template <typename T>
class DedupManager {
        DedupTable<T> &dedup_table;
        const uintmax_t bytes;
        size_t current_count;
        const size_t total_count;
        const size_t step_size;
    
    public:
        DedupManager(DedupTable<T> &d, uintmax_t b, size_t t_c, size_t s_s)
            : dedup_table(d), bytes(b), current_count(0), total_count(t_c), 
              step_size( s_s == 0 ? 1 : s_s ) {}; // Prevent zero step size

        void insert(const File &file, uintmax_t size)
        {
            try
            {
                insert_into_dedup_table(file, size, dedup_table, bytes);
            }
            catch(const fs::filesystem_error &e)
            {
                std::cerr << e.what() << '\n';
            }
            catch(const std::runtime_error &e)
            {
                std::cerr << e.what() << " ["
                    << file.path << "]\n";
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            ++current_count;
            print_progress(*this);
        }

        size_t get_current_count() const {return current_count;}
        size_t get_total_count() const {return total_count;}
        size_t get_step_size() const {return step_size;}
};


/**
 * Manages the scanning that is done before deduplication. Files are counted and
 * their paths and last modification times are collected.
 */
class ScanManager {
        // size_t can store the maximum size of a theoretically possible object 
        // of any type. Because we store Files in objects, we use size_t.
        size_t count;
        // uintmax_t is the maximum width unsigned integer type. We use it in 
        // order to not limit file size by type choice.
        uintmax_t size;
        FileSizeTable &file_size_table;
    public:
        ScanManager(FileSizeTable &f)
            : count(0), size(0), file_size_table(f) {};

        void insert(const fs::directory_entry &entry)
        {
            try
            {
                // Symlinks are not followed
                if (fs::is_regular_file(fs::symlink_status(entry.path()))
                    && !fs::is_empty(entry.path()))
                {
                    ++count;
                    size += entry.file_size();
                    file_size_table[entry.file_size()]
                        .push_back(File(entry.path().string(), 
                                        entry.last_write_time()));
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
 * Prints the progress on finding duplicates.
 */
template <typename T>
void print_progress(DedupManager<T> &op) {
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
 * Traverses the given path and collects information about files.
 * Directories are traversed recursively if wanted.
 */
template <typename T>
void scan_path(const fs::path &path, bool recurse, ScanManager &sm)
{
    if (fs::is_directory(path))
    {
        // Directories that cannot be accessed are skipped
        if (recurse)
        {
            for (const auto &p : fs::recursive_directory_iterator(path, 
                fs::directory_options::skip_permission_denied))
            {
                sm.insert(p);
            }
        }
        else
        {
            for (const auto &p : fs::directory_iterator(path, 
                fs::directory_options::skip_permission_denied))
            {
                sm.insert(p);
            }
        }
    }
    else
    {
        sm.insert(fs::directory_entry(path));
    }
}

/**
 * Finds duplicate files from the given paths.
 * 
 * Path can be a file or a directory.
 * Directories can be searched recursively, according to the given parameter.
 * 
 * Returns a vector whose elements are vectors of duplicate files.
 */
template <typename T>
vector<DuplicateVector> find_duplicates(const cxxopts::ParseResult &cl_args, 
    const std::set<fs::path> &paths_to_deduplicate)
{    
    // Start by scanning the paths for files
    FileSizeTable file_size_table;
    cout << "Counting number and size of files in given paths..." << endl;
    ScanManager cop = ScanManager(file_size_table);
    
    const bool recurse = cl_args["recurse"].as<bool>();
    for (const auto &path : paths_to_deduplicate)
    {
        try
        {
            scan_path<T>(path, recurse, cop);
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

    { // Files with unique size can't have duplicates
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

        cout << "Eliminated " << no_unique_file_sizes << " files with unique "
        "size from deduplication.\n";
    }

    DedupTable<T> dedup_table;

    // Both are grouped by file size
    dedup_table.reserve(file_size_table.size());
    const uintmax_t bytes = cl_args["bytes"].as<uintmax_t>();

    { // The deduplication
        DedupManager<T> iop = DedupManager<T>(dedup_table, 
        bytes, total_count, total_count / 20);

        FileSizeTable::iterator iter = file_size_table.begin();
        FileSizeTable::iterator end_iter = file_size_table.end();

        for (; iter != end_iter;)
        {
            for (const auto &file : iter->second)
            {
                iop.insert(file, iter->first);
            }
            iter = file_size_table.erase(iter);
        }
    }

    cout << "\r" << "Done checking.                             " << endl;

    // Includes vectors of files whose whole content is the same
    vector<DuplicateVector> duplicates;

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

template vector<DuplicateVector> find_duplicates<uint8_t>(
    const cxxopts::ParseResult &cl_args, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<DuplicateVector> find_duplicates<uint16_t>(
    const cxxopts::ParseResult &cl_args, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<DuplicateVector> find_duplicates<uint32_t>(
    const cxxopts::ParseResult &cl_args, 
    const std::set<fs::path> &paths_to_deduplicate);
template vector<DuplicateVector> find_duplicates<uint64_t>(
    const cxxopts::ParseResult &cl_args, 
    const std::set<fs::path> &paths_to_deduplicate);
