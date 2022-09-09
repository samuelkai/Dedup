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
 * Inserts the given File into the deduplication vector.
 */
template <typename T>
void insert_into_dedup_vector(const File &file, 
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
                insert_into_dedup_vector(file, dedup_vector, bytes);
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
    FileSizeTable file_size_table;

    // Start by scanning the paths for files
    const size_t total_count = scan_all_paths(file_size_table, cl_args);

    // Files with unique size can't have duplicates
    const size_t no_fls_with_uniq_sz = 
        skip_files_with_unique_size(file_size_table);

    const size_t total_non_unique_sz_count = total_count - no_fls_with_uniq_sz;

    const uintmax_t bytes = std::get<uintmax_t>(cl_args.at("bytes"));

    DedupVector<T> dedup_vector;
    dedup_vector.reserve(total_non_unique_sz_count);

    { // Collect all files in the deduplication vector and sort them according 
      // to the hash of the beginning of their data.
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
                same_hashes.back().first == dedup_vector[i+j+1].first; j++)
            {
                same_hashes.push_back(std::move(dedup_vector[i+j+1]));
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
