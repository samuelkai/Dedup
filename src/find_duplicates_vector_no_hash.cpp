#include "find_duplicates.h"
#include "utilities.h"

#include <algorithm>
#include <csignal>
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

using DedupVector = std::vector<
                        std::pair<
                            ContentArray,
                            File
                        >
                    >;

/**
 * Stores Files grouped by their size. Used during the file scanning phase.
 */
using FileSizeTable = std::unordered_map<uintmax_t, vector<File>>;

DuplicateVector find_duplicate_file(string path, DedupVector &same_beginning)
{
    DuplicateVector dup_vec;
    for (auto curr = same_beginning.begin(); curr != same_beginning.end();)
    {
        if (compare_files(path, curr->second.path))
        {
            dup_vec.push_back(curr->second);
            curr = same_beginning.erase(curr);
        }
        else
        {
            ++curr;
        }
        
    }
    return dup_vec;

}

/**
 * Inserts the given File into the deduplication table.
 */
void insert_into_dedup_table(const File &file, 
                             DedupVector &dedup_vector)
{   
    const ContentArray beginning = read_file_beginning(file.path);
    dedup_vector.push_back(std::make_pair(beginning, file));
}

class DedupManager;

void print_progress(DedupManager &op);

/**
 * Manages the deduplication. Stores progress information and inserts Files to
 * dedup_table.
 */
class DedupManager {
        DedupVector &dedup_vector;
        const uintmax_t bytes;
        size_t current_count;
        const size_t total_count;
        const size_t step_size;
    
    public:
        DedupManager(DedupVector &d, uintmax_t b, size_t t_c, size_t s_s)
            : dedup_vector(d), bytes(b), current_count(0), total_count(t_c), 
              step_size( s_s == 0 ? 1 : s_s ) {}; // Prevent zero step size

        void insert(const File &file)
        {
            try
            {
                insert_into_dedup_table(file, dedup_vector);
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
 * Prints the progress on finding duplicates.
 */
void print_progress(DedupManager &op) {
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

bool sort_only_by_first(const std::pair<ContentArray, File> &a, 
                        const std::pair<ContentArray, File> &b) 
{ 
    return (a.first < b.first);
} 

/**
 * Finds duplicate files from the given paths.
 * 
 * Path can be a file or a directory.
 * Directories can be searched recursively, according to the given parameter.
 * 
 * Returns a vector whose elements are vectors of duplicate files.
 */
vector<DuplicateVector> find_duplicates_vector_no_hash(const ArgMap &cl_args)
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
    
    const size_t total_count = sm.get_count();
    const uintmax_t total_size = sm.get_size();
    cout << "Counted " << total_count << " files occupying "
            << format_bytes(total_size) << "." << endl;

    size_t no_fls_with_uniq_sz = 0;

    { // Files with unique size can't have duplicates
        auto same_size_iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

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
    }

    // DedupTable<T> dedup_table;

    // Both are grouped by file size
    // dedup_table.reserve(file_size_table.size());
    const uintmax_t bytes = std::get<uintmax_t>(cl_args.at("bytes"));

    DedupVector dedup_vector;
    dedup_vector.reserve(total_count - no_fls_with_uniq_sz);

    { // The deduplication
        DedupManager dm = DedupManager(dedup_vector, 
        bytes, total_count, total_count / 20);

        auto iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

        for (; iter != end_iter;)
        {
            for (const auto &file : iter->second)
            {
                dm.insert(file);
            }
            iter = file_size_table.erase(iter);
        }

        std::sort(dedup_vector.begin(), dedup_vector.end(), 
            sort_only_by_first);
    }

    cout << "\r" << "Done checking.                             " << endl;

    // Includes vectors of files whose whole content is the same
    vector<DuplicateVector> duplicates;
    {
        std::sort(dedup_vector.begin(), dedup_vector.end(), sort_only_by_first);

        vector<DedupVector> vec_of_same_beginnings;
        for (size_t i = 0; i < dedup_vector.size() - 1; i++)
        {
            DedupVector same_beginnings;
            same_beginnings.push_back(dedup_vector[i]);
            size_t j = 0;
            for (; i+j+1 < dedup_vector.size() && 
                dedup_vector[i+j].first == dedup_vector[i+j+1].first; j++)
            {
                same_beginnings.push_back(dedup_vector[i+j+1]);
            }
            i += j;
            vec_of_same_beginnings.push_back(same_beginnings);
        }

        for (DedupVector &same_beginnings: vec_of_same_beginnings)
        {
            while (same_beginnings.size() > 1)
            {
                auto identicals = find_duplicate_file(
                    same_beginnings[0].second.path, same_beginnings);
                if (identicals.size() > 1)
                {
                    duplicates.push_back(std::move(identicals));
                }
            }
        }
    }
    return duplicates;
}
