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
using DedupTable = std::unordered_map<T, vector<vector<string>>>;

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
void insert_into_dedup_table(const string &path, DedupTable<T> &dedup_table,
                             uint64_t bytes)
{   
        // Truncate the hash to the specified length
        const auto hash = static_cast<T>(hash_file(path, bytes));

        if (dedup_table[hash].empty()) // First file that produces this hash
        {
            dedup_table[hash].push_back(
                vector<string>{path});
        }
        else
        {
            if (!find_duplicate_file(
                path, dedup_table[hash]))
            {
                // File differs from others with the same hash
                dedup_table[hash].push_back(
                    vector<string>{path});
            }
        }
}

class DirectoryOperation {
        const bool b_has_progress;
    protected:
    //  Prevents slicing: https://stackoverflow.com/questions/49961811/must-a-c-interface-obey-the-rule-of-five
        DirectoryOperation(const DirectoryOperation&) = default;
        DirectoryOperation(DirectoryOperation&&) = default;
        DirectoryOperation& operator=(const DirectoryOperation&) = default;
        DirectoryOperation& operator=(DirectoryOperation&&) = default;
    public:
        DirectoryOperation(bool p)
            : b_has_progress(p) {}        

        virtual void insert(const fs::directory_entry&) = 0;
        bool has_progress() const
        {
            return b_has_progress;
        }

        virtual ~DirectoryOperation() = default;
};

template <typename T>
class InsertOperation : public DirectoryOperation {
        DedupTable<T> &dedup_table;
        const uint64_t bytes;
        size_t current_count;
    public:
        InsertOperation(DedupTable<T> &d, uint64_t b, bool p)
            : DirectoryOperation(p), dedup_table(d), bytes(b), 
              current_count(0) {}

        void insert_into_own_dedup_table(const string &path)
        {
            insert_into_dedup_table(path, dedup_table, bytes);
            ++current_count;
        }

        size_t get_current_count() const {return current_count;}
};

template <typename T>
class NoProgressInsertOperation : public InsertOperation<T> {
        uintmax_t current_size;
    public:
        NoProgressInsertOperation(DedupTable<T> &d, uint64_t b)
            : InsertOperation<T>(d, b, false), current_size(0) {}

        void insert(const fs::directory_entry &entry) final override
        {
            InsertOperation<T>::insert_into_own_dedup_table(entry.path().string());
            current_size += entry.file_size();
        }

        size_t get_current_size() const {return current_size;}
};

template <typename T>
class ProgressInsertOperation : public InsertOperation<T> {
        const size_t total_count;
        const size_t step_size;
    public:
        ProgressInsertOperation(DedupTable<T> &d, uint64_t b, size_t t_c, 
            size_t s)
            : InsertOperation<T>(d, b, true), total_count(t_c), 
              step_size( s == 0 ? 1 : s ) {}; // Prevent zero step size

        void insert(const fs::directory_entry &entry) final override
        {
            InsertOperation<T>::insert_into_own_dedup_table(entry.path().string());
        }

        size_t get_total_count() const {return total_count;}
        size_t get_step_size() const {return step_size;}
};

class CountOperation : public DirectoryOperation {
        size_t count;
        uintmax_t size;
    public:
        CountOperation()
            : DirectoryOperation(false), count(0), size(0) {};

        void insert(const fs::directory_entry &entry) final override
        {
            ++count;
            size += entry.file_size();
        }

        size_t get_count() const {return count;}
        uintmax_t get_size() const {return size;}
};

/**
 * Draws a bar on the terminal showing the progress on finding duplicates.
 */
template <typename T>
void draw_progress(ProgressInsertOperation<T> &op) {
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

template <typename T>
inline void handle_file_path(const fs::directory_entry &path, 
    DirectoryOperation &op)
{
    op.insert(path);
    if (op.has_progress())
    {
        draw_progress(static_cast<ProgressInsertOperation<T>&>(op));
    }
}

template <typename T>
void traverse_directory_recursively(const fs::path &directory, 
    DirectoryOperation &op)
{
    fs::recursive_directory_iterator iter(directory, 
        fs::directory_options::skip_permission_denied);
    const fs::recursive_directory_iterator end;
    while (iter != end)
    {    
        const fs::path iter_path(iter->path());

        try
        {
            if (fs::is_regular_file(fs::symlink_status(iter_path))
                && !fs::is_empty(iter_path))
            {
                handle_file_path<T>(*iter, op);
            }
        }
        catch(const fs::filesystem_error &e)
        {
            std::cerr << e.what() << '\n';
        }
        catch(const std::runtime_error &e)
        {
            std::cerr << e.what() << " ["
                << iter_path.string() << "]\n";
        }
        ++iter;        
    }
}

/**
 * Traverses the given path for files, which are then checked for duplicates.
 * Only regular files are checked.
 * Directories are traversed recursively if wanted.
 */
template <typename T>
void traverse_path(const fs::path &path, bool recurse, DirectoryOperation &op)
{
    if (fs::is_directory(path))
    {
        if (recurse)
        {
            traverse_directory_recursively<T>(path, op);
        }
        else
        {
            fs::directory_iterator iter(path, 
                fs::directory_options::skip_permission_denied);
            const fs::directory_iterator end;
            for (; iter != end; ++iter)
            {
                const fs::path iter_path(iter->path());

                try
                {
                    if (fs::is_regular_file(fs::symlink_status(iter_path))
                        && !fs::is_empty(iter_path))
                    {
                        handle_file_path<T>(*iter, op);
                    }
                }
                catch(const fs::filesystem_error &e)
                {
                    std::cerr << e.what() << '\n';
                }
                catch(const std::runtime_error &e)
                {
                    std::cerr << e.what() << " ["
                        << iter_path.string() << "]\n";
                }
            }
        }
    }
    else if (fs::is_regular_file(fs::symlink_status(path)))
    {
        handle_file_path<T>(fs::directory_entry(path), op);
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
vector<vector<string>> find_duplicates(const cxxopts::ParseResult &result, 
    const std::set<fs::path> &paths_to_deduplicate)
{
    DedupTable<T> dedup_table;

    const uint64_t bytes = result["bytes"].as<uint64_t>();
    const bool recurse = result["recurse"].as<bool>();
    const bool skip_count = result["skip-count"].as<bool>();

    if (!skip_count)
    {
        cout << "Counting number and size of files in given paths..." << endl;
        CountOperation cop = CountOperation();
        
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

        dedup_table.reserve(total_count);

        ProgressInsertOperation piop = ProgressInsertOperation<T>(dedup_table, 
        bytes, total_count, total_count / 20);
        for (const auto &path : paths_to_deduplicate)
        {
            try
            {
                cout << "Searching " << path << " for duplicates"
                    << (recurse ? " recursively" : "") << "..." << endl;
                traverse_path<T>(path, recurse, piop);
            }
            catch (const std::exception &e)
            {
                cerr << e.what() << '\n';
            }
        }
        cout << "\r" << "Done checking " << total_count 
             << " files occupying " << format_bytes(total_size) << "."
             << endl << endl;
    }
    else
    {
        NoProgressInsertOperation iop = 
            NoProgressInsertOperation<T>(dedup_table, bytes);
        for (const auto &path : paths_to_deduplicate)
        {
            try
            {
                cout << "Searching " << path << " for duplicates"
                    << (recurse ? " recursively" : "") << "..." << endl;
                traverse_path<T>(path, recurse, iop);
            }
            catch (const std::exception &e)
            {
                cerr << e.what() << '\n';
            }
        }

        const size_t total_count = iop.get_current_count();
        const uintmax_t total_size = iop.get_current_size();
        cout << "\r" << "Done checking " << total_count 
             << " files occupying " << format_bytes(total_size) << "."
             << endl << endl;
    }
        
    // Includes vectors of files whose whole content is the same
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
