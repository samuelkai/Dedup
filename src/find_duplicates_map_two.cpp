#include "find_duplicates.h"
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
 * The key of the inner map is the hash of the beginning N bytes of a file 
 * (short hash), where N is a program argument.
 * The key type T is one of {uint8_t, uint16_t, uint32_t, uint64_t}.
 * The value of the inner map contains the paths of all files that produce the 
 * same short hash.
 * When adding a File to ShortTable:
 *   if the vector is empty:
 *     add the new File to the vector
 *   if the vector already contains one File:
 *     copy the occupant to LongTable
 *     add the new File to LongTable
 *     add the new File to the vector
 *   if the vector already contains two Files:
 *     add the new File to LongTable
 */
template <typename T>
using ShortTable = std::unordered_map<
    uintmax_t,
    std::unordered_map<
        T,
        vector<File>
    >
>;

/**
 * If at least two Files have the same short hash, this table is used to store 
 * their long hashes. The long hash is calculated from the whole file. 
 * The value of the map contains the paths of all files that produce the 
 * same long hash. Because hashes can collide, the outer vector
 * contains inner vectors that contain Files whose whole content is the same.
 */
template <typename T>
using LongTable = std::unordered_map<
    T,
    vector<DuplicateVector>
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
                // By catching here we can compare to the other elements of 
                // dup_vec
                cerr << e.what() << '\n';
            }                           
        }
        return false;
    }
}

/**
 * Inserts the given File into the deduplication tables.
 */
template <typename T>
void insert_into_dedup_table(const File &file, uintmax_t size,
                             ShortTable <T> &short_table, uintmax_t bytes, 
                             LongTable<T> &long_table)
{   
    // Calculate the hash and truncate it to the specified length
    const auto hash = static_cast<T>(hash_file(file.path, bytes));

    // If this vector doesn't already exist, [] creates it
    vector<File> &vec = short_table[size][hash];

    if (vec.empty()) // First file that produces this hash
    {
        // Add the file only to short table 
        vec.push_back(file);
    }
    else
    {
        if (vec.size() == 1) // Short table slot already occupied, 
                             // copy the occupant to long table
        {
            const auto long_hash_existing = 
                static_cast<T>(hash_file(vec[0].path, 0));
            vector<DuplicateVector> &vec_vec_ex = 
                long_table[long_hash_existing];

            if (vec_vec_ex.empty())
            {
                vec_vec_ex.push_back(
                    vector<File>{vec[0]});
            }
            else if (!find_duplicate_file(
                vec[0], vec_vec_ex))
            {
                // File differs from others with the same hash
                vec_vec_ex.push_back(
                    vector<File>{vec[0]});
            }
        }

        // Add the file to long table 
        const auto long_hash = static_cast<T>(hash_file(file.path, 0));
        vector<DuplicateVector> &vec_vec = long_table[long_hash];

        if (vec_vec.empty())
        {
            vec_vec.push_back(
                vector<File>{file});
        }
        else if (!find_duplicate_file(
            file, vec_vec))
        {
            // File differs from others with the same hash
            vec_vec.push_back(
                vector<File>{file});
        }

        // Add the file to short table to signal that the long table must be
        // used
        if (vec.size() < 2)
        {
            vec.push_back(file);
        }
    }
}

/**
 * Manages the deduplication. Stores progress information and inserts Files to
 * the dedup tables.
 */
template <typename T>
class DedupManager {
        ShortTable <T> &short_table;
        const uintmax_t bytes;
        size_t current_count;
        const size_t total_count;
        const size_t step_size;
        LongTable<T> &long_table;
    
    public:
        DedupManager(ShortTable <T> &d, uintmax_t b, size_t t_c, size_t s_s, 
                     LongTable<T> &l)
            : short_table(d), bytes(b), current_count(0), total_count(t_c), 
              step_size( s_s == 0 ? 1 : s_s ),
              long_table(l) {}; // Prevent zero step size

        void insert(const File &file, uintmax_t size)
        {
            try
            {
                insert_into_dedup_table(file, size, short_table, bytes, 
                                        long_table);
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

        void insert(const fs::directory_entry &entry, size_t number_of_path)
        {
            try
            {
                fs::path path = entry.path();
                // Symlinks and empty files are skipped
                if (fs::is_regular_file(fs::symlink_status(path))
                    && !fs::is_empty(path))
                {
                    auto file_size = entry.file_size();
                    // If a file's hard link count is 1, it doesn't have extra
                    // hard links
                    if (fs::hard_link_count(path) > 1)
                    {
                        for (auto &file : file_size_table[file_size])
                        {
                            if (fs::equivalent(path, fs::path(file.path)))
                            {
                                // The file to be inserted is an extra hard link
                                // to an already inserted file, so skip it
                                return;
                            }
                        }
                    }
                    
                    file_size_table[file_size]
                        .push_back(File(path.string(), 
                                        entry.last_write_time(),
                                        number_of_path));
                    ++count;
                    size += file_size;
                }
            }
            catch(const fs::filesystem_error &e)
            {
                cerr << e.what() << '\n';
            }
            catch(const std::runtime_error &e)
            {
                cerr << e.what() << " [" << entry.path().string() << "]\n";
            }
            catch(const std::exception& e)
            {
                cerr << e.what() << '\n';
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
namespace
{
    void scan_path(const fs::path &path, bool recurse, ScanManager &sm, 
                   size_t number_of_path)
    {
        if (fs::is_directory(path))
        {
            // Directories that cannot be accessed are skipped
            if (recurse)
            {
                for (const auto &p : fs::recursive_directory_iterator(path, 
                    fs::directory_options::skip_permission_denied))
                {
                    sm.insert(p, number_of_path);
                }
            }
            else
            {
                for (const auto &p : fs::directory_iterator(path, 
                    fs::directory_options::skip_permission_denied))
                {
                    sm.insert(p, number_of_path);
                }
            }
        }
        else
        {
            sm.insert(fs::directory_entry(path), number_of_path);
        }
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
vector<DuplicateVector> find_duplicates_map_two(const ArgMap &cl_args)
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
        auto same_hash_iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

        size_t no_unique_file_sizes = 0;

        for(; same_hash_iter != end_iter; )
        {
            if (same_hash_iter->second.size() == 1) // Unique size
            {
                same_hash_iter = file_size_table.erase(same_hash_iter);
                ++no_unique_file_sizes;
            }
            else
            {
                ++same_hash_iter;
            }
        }

        cout << "Discarded " << no_unique_file_sizes << " files with unique "
        "size from deduplication.\n";
        total_count -= no_unique_file_sizes;
    }

    ShortTable<T> short_table;
    LongTable<T> long_table;

    // Both are grouped by file size
    short_table.reserve(file_size_table.size());
    const uintmax_t bytes = std::get<uintmax_t>(cl_args.at("bytes"));

    { // The deduplication
        DedupManager<T> dm = DedupManager<T>(short_table, 
        bytes, total_count, total_count / 20, long_table);

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
    
    cout << "\r" << "Done checking.                             " << endl;

    // Includes vectors of files whose whole content is the same
    vector<DuplicateVector> duplicates;
    {
        auto same_hash_iter = long_table.begin();
        auto end_iter = long_table.end();

        for (; same_hash_iter != end_iter;)
        {
            for (const auto &identicals : same_hash_iter->second)
            {
                if (identicals.size() > 1)
                {
                    duplicates.push_back(std::move(identicals));
                }
            }
            same_hash_iter = long_table.erase(same_hash_iter);
        }
    }
    return duplicates;
}

template vector<DuplicateVector> find_duplicates_map_two<uint8_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_map_two<uint16_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_map_two<uint32_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_map_two<uint64_t>(
    const ArgMap &cl_args);
