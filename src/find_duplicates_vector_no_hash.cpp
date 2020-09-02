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

std::size_t index_of_file(DedupVector &vec, string path)
{
    for (size_t i = 0; i < vec.size(); i++)
    {
        if (vec[i].second.path == path)
        {
            return i;
        }
    }
    return std::numeric_limits<std::size_t>::max(); 
}

DuplicateVector find_duplicate_file(string path, DedupVector &same_hashes)
{
    DuplicateVector dup_vec;
    for (auto curr = same_hashes.begin(); curr != same_hashes.end();)
    {
        if (compare_files(path, curr->second.path))
        {
            dup_vec.push_back(curr->second);
            curr = same_hashes.erase(curr);
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
    // Calculate the hash and truncate it to the specified length
    const ContentArray content = read_file_beginning(file.path);
    dedup_vector.push_back(std::make_pair(content, file));
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
    for (const auto &path : std::get<vector<fs::path>>(cl_args.at("paths")))
    {
        try
        {
            scan_path(path, recurse, sm);
        }
        catch(const std::exception &e)
        {
            cerr << e.what() << '\n';
        }
        
    }
    
    const size_t total_count = sm.get_count();
    const uintmax_t total_size = sm.get_size();
    cout << "Counted " << total_count << " files occupying "
            << format_bytes(total_size) << "." << endl;

    size_t no_unique_file_sizes = 0;

    { // Files with unique size can't have duplicates
        auto same_size_iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

        for(; same_size_iter != end_iter; )
        {
            if (same_size_iter->second.size() == 1) // Unique size
            {
                same_size_iter = file_size_table.erase(same_size_iter);
                ++no_unique_file_sizes;
            }
            else
            {
                ++same_size_iter;
            }
        }

        cout << "Discarded " << no_unique_file_sizes << " files with unique "
        "size from deduplication.\n";
    }

    // DedupTable<T> dedup_table;

    // Both are grouped by file size
    // dedup_table.reserve(file_size_table.size());
    const uintmax_t bytes = std::get<uintmax_t>(cl_args.at("bytes"));

    DedupVector dedup_vector;
    dedup_vector.reserve(total_count - no_unique_file_sizes);

    { // The deduplication
        DedupManager iop = DedupManager(dedup_vector, 
        bytes, total_count, total_count / 20);

        auto iter = file_size_table.begin();
        auto end_iter = file_size_table.end();

        for (; iter != end_iter;)
        {
            for (const auto &file : iter->second)
            {
                // if (file.path == "/home/samuel/Koodi/linux-5.5.10/tools/perf/pmu-events/arch/x86/westmereep-dp/pipeline.json" || file.path == "/home/samuel/Koodi/linux-5.5.10/tools/perf/pmu-events/arch/x86/westmereep-sp/pipeline.json") 
                // {
                //     std::raise(SIGINT);
                // }
                iop.insert(file);
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
        auto iter = dedup_vector.begin();
        const auto end_iter = dedup_vector.end();

        for (; iter != end_iter;)
        {
            // auto indeksi __attribute__((unused)) = index_of_file<T>(dedup_vector, "/home/samuel/Koodi/linux-5.5.10/tools/perf/pmu-events/arch/x86/westmereep-sp/pipeline.json");
            std::pair<typename DedupVector::iterator, typename DedupVector::iterator> bounds = std::equal_range(iter, end_iter, *iter, sort_only_by_first);
            DedupVector same_hashes = vector(bounds.first, bounds.second);
            while (same_hashes.size() > 1)
            {
                auto identicals = find_duplicate_file(same_hashes[0].second.path, same_hashes);
                if (identicals.size() > 1)
                {
                    duplicates.push_back(std::move(identicals));
                }
            }
            iter = bounds.second;
        }
    }
    return duplicates;
}
