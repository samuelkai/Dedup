#include "find_duplicates_base.h"
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

namespace {
/**
 * Stores Files and hashes of the beginnings of their data.
 */
template <typename T>
using DedupVector = std::vector<
                        std::pair<
                            T, 
                            File
                        >
                    >;

/**
 * Checks the given vector for files identical to the one in the given path and 
 * returns them.
 */
template <typename T>
DuplicateVector find_duplicate_file(string path, 
                                    vector<std::pair<T, File>> &same_hashes)
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
template <typename T>
void insert_into_dedup_table(const File &file, 
                             DedupVector<T> &dedup_vector, uintmax_t bytes)
{   
    // Calculate the hash and truncate it to the specified length
    const auto hash = static_cast<T>(hash_file(file.path, bytes));
    dedup_vector.push_back(std::make_pair(hash, file));
}

/**
 * Manages the deduplication. Stores progress information and inserts Files to
 * dedup_table.
 */
template <typename T>
class DedupManager {
        DedupVector<T> &dedup_vector;
        const uintmax_t bytes;
        size_t current_count;
        const size_t total_count;
        const size_t step_size;
    
    public:
        DedupManager(DedupVector<T> &d, uintmax_t b, size_t t_c, size_t s_s)
            : dedup_vector(d), bytes(b), current_count(0), total_count(t_c), 
              step_size( s_s == 0 ? 1 : s_s ) {}; // Prevent zero step size

        void insert(const File &file)
        {
            try
            {
                insert_into_dedup_table(file, dedup_vector, bytes);
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
 * Sort by the hashes of files.
 */
template <typename T>
bool sort_only_by_first(const std::pair<T, File> &a, 
                        const std::pair<T, File> &b) 
{ 
    return (a.first < b.first); 
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
vector<DuplicateVector> find_duplicates_vector(const ArgMap &cl_args)
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
    const size_t total_non_unique_sz_count = total_count - no_fls_with_uniq_sz;

    const uintmax_t bytes = std::get<uintmax_t>(cl_args.at("bytes"));

    DedupVector<T> dedup_vector;
    dedup_vector.reserve(total_non_unique_sz_count);

    { // The deduplication
        DedupManager<T> dm = DedupManager<T>(dedup_vector, 
        bytes, total_non_unique_sz_count, total_non_unique_sz_count / 20 + 1);

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
            sort_only_by_first<T>);
    }

    cout << endl << "Done checking." << endl;

    // Includes vectors of files whose whole content is the same
    vector<DuplicateVector> duplicates;
    if (dedup_vector.size() == 0) {
        return duplicates;
    }
    {
        // Group files that have the same hash into vectors
        vector<DedupVector<T>> vec_of_same_hashes;
        for (size_t i = 0; i < dedup_vector.size() - 1; i++)
        {
            DedupVector<T> same_hashes;
            same_hashes.push_back(dedup_vector[i]);
            size_t j = 0;
            for (; i+j+1 < dedup_vector.size() && 
                dedup_vector[i+j].first == dedup_vector[i+j+1].first; j++)
            {
                same_hashes.push_back(dedup_vector[i+j+1]);
            }
            i += j;
            vec_of_same_hashes.push_back(same_hashes);
        }

        // Compare the whole content of files that have the same hash
        for (DedupVector<T> &same_hashes: vec_of_same_hashes)
        {
            while (same_hashes.size() > 1)
            {
                const auto to_be_compared = same_hashes.begin()->second;
                same_hashes.erase(same_hashes.begin());
                auto identicals = find_duplicate_file(
                    to_be_compared.path, same_hashes);
                if (identicals.size() > 0)
                {
                    identicals.push_back(to_be_compared);
                    duplicates.push_back(std::move(identicals));
                }
            }
        }
    }
    return duplicates;
}

template vector<DuplicateVector> find_duplicates_vector<uint8_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_vector<uint16_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_vector<uint32_t>(
    const ArgMap &cl_args);
template vector<DuplicateVector> find_duplicates_vector<uint64_t>(
    const ArgMap &cl_args);
