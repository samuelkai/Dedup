#include "find_duplicates_base.h"
#include "utilities.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <variant>
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
 * Checks the given vector of duplicate file vectors for a file that has the
 * same content as the file in the given path. If found, inserts the path
 * to the duplicate file vector and returns true.
 */ 
namespace
{
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
                cerr << e.what() << '\n';
            }                           
        }
        return false;
    }
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

    // If this vector doesn't already exist, [] creates it
    vector<DuplicateVector> &vec_vec = dedup_table[size][hash];

    if (vec_vec.empty()) // First file that produces this hash
    {
        vec_vec.push_back(
            vector<File>{file});
    }
    else
    {
        if (!find_duplicate_file(
            file, vec_vec))
        {
            // File differs from others with the same hash
            vec_vec.push_back(
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
                cerr << e.what() << '\n';
            }
            catch(const std::runtime_error &e)
            {
                cerr << e.what() << " [" << file.path << "]\n";
            }
            catch(const std::exception& e)
            {
                cerr << e.what() << '\n';
            }
            ++current_count;
            print_progress(current_count, total_count, step_size);
        }
};

/**
 * Finds duplicate files from the given paths.
 * 
 * Path can be a file or a directory.
 * Directories can be searched recursively, according to the given parameter.
 * 
 * Returns a vector whose elements are vectors of duplicate files.
 */
template <typename T>
vector<DuplicateVector> find_duplicates_map(const ArgMap &cl_args)
{    
    // Start by scanning the paths for files
    FileSizeTable file_size_table;
    cout << "Counting number and size of files in given paths..." << endl;
    ScanManager sm = ScanManager(file_size_table);
    
    const bool recurse = std::get<bool>(cl_args.at("recurse"));
    size_t number_of_path = 0; // Used in deciding which file to keep when 
                               // deleting or linking without prompting
    for (const auto &path : std::get<vector<fs::path>>(cl_args.at("paths")))
    {
        try
        {
            scan_path(path, recurse, sm, number_of_path);
        }
        catch(const std::exception &e)
        {
            cerr << e.what() << '\n';
        }
        ++number_of_path;
    }
    
    size_t total_count = sm.get_count();
    const uintmax_t total_size = sm.get_size();
    cout << "Counted " << total_count << " files occupying "
            << format_bytes(total_size) << "." << endl;

    { // Files with unique size can't have duplicates
        auto same_size_iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

        size_t no_fls_with_uniq_sz = 0;

        for(; same_size_iter != end_iter; )
        {
            if (same_size_iter->second.size() == 1) // Unique size
            {
                same_size_iter = file_size_table.erase(same_size_iter);
                ++no_fls_with_uniq_sz;
            }
            else
            {
                ++same_size_iter;
            }
        }

        cout << "Discarded " << no_fls_with_uniq_sz << " files with unique "
        "size from deduplication.\n";
        total_count -= no_fls_with_uniq_sz;
    }

    DedupTable<T> dedup_table;

    // Both are grouped by file size
    dedup_table.reserve(file_size_table.size());
    const uintmax_t bytes = std::get<uintmax_t>(cl_args.at("bytes"));

    { // The deduplication
        DedupManager<T> dm = DedupManager<T>(dedup_table, 
        bytes, total_count, total_count / 20 + 1);

        auto iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

        for (; iter != end_iter;)
        {
            for (const auto &file : iter->second)
            {
                dm.insert(file, iter->first);
            }
            iter = file_size_table.erase(iter);
        }
    }
    
    cout << endl << "Done checking." << endl;

    // Includes vectors of files whose whole content is the same
    vector<DuplicateVector> duplicates;
    {
        auto same_size_iter = dedup_table.begin();
        auto end_iter = dedup_table.end();

        for (; same_size_iter != end_iter;)
        {
            for (const auto &same_hash : same_size_iter->second)
            {
                for (const auto &identicals : same_hash.second)
                {
                    if (identicals.size() > 1)
                    {
                        duplicates.push_back(std::move(identicals));
                    }
                }
            }
            same_size_iter = dedup_table.erase(same_size_iter);
        }
    }
    return duplicates;
}

template vector<DuplicateVector> find_duplicates_map<uint8_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_map<uint16_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_map<uint32_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_map<uint64_t>(
    const ArgMap &cl_args);
