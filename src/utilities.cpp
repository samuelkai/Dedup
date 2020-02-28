#include "utilities.h"
#include "xxHash/xxhash.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace fs = std::filesystem;

/**
 * Exception that compare_files throws when it can't open a file.
 */
FileException::FileException(fs::path path) :
    std::runtime_error("Error opening file: " + path.string())
{
}

/**
 * Returns true if the contents of the files in the given paths are exactly
 * the same.
 */
bool compare_files(const fs::path &path1, const fs::path &path2)
{
    // Open with position indicator at end
    std::ifstream f1(path1, std::ios::binary|std::ifstream::ate);
    std::ifstream f2(path2, std::ios::binary|std::ifstream::ate);


    if (f1.fail()) //file problem
    {
        throw FileException(path1);
    }

    if (f2.fail()) //file problem
    {
        throw FileException(path2);
    }

    if (f1.tellg() != f2.tellg()) //size mismatch
    {
        return false;
    }

    //seek back to beginning
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);

    constexpr size_t buffer_size = 4096;
    char input_buffer1[buffer_size];
    char input_buffer2[buffer_size];
    
    // Compare files a buffer size at a time
    do
    {
        f1.read((char*)input_buffer1, buffer_size);
        const auto count1 = f1.gcount();

        f2.read((char*)input_buffer2, buffer_size);
        const auto count2 = f2.gcount();

        if (count1 != count2 ||
            memcmp(input_buffer1, input_buffer2, count1))
        {
            return false; // Files are not equal
        }
    } while (f1 && f2);
    return true;
}

/**
 * Return the 64-bit XXHash digest of the beginning of the file in the given 
 * path. Parameter "bytes" specifies the number of bytes that are considered.
 * If bytes == 0, the whole file is hashed.
 */
uint64_t hash_file(const fs::path &path, uint64_t bytes)
{
    std::ifstream istream(path, std::ios::binary);

    // Based on example code from xxHash
    XXH64_state_t* const state = XXH64_createState();
    if (state==nullptr) {
        XXH64_freeState(state);
        throw std::runtime_error("xxHash state creation error");
    }

    constexpr size_t buffer_size = 4096;
    char input_buffer[buffer_size];

    const XXH_errorcode resetResult = XXH64_reset(state, 0);
    if (resetResult == XXH_ERROR) {
        XXH64_freeState(state);
        throw std::runtime_error("xxHash state initialization error");
    }

    if (bytes == 0) // hash the whole file
    {
        do
        {
            istream.read((char*)input_buffer, buffer_size);
            const auto count = istream.gcount();
            const XXH_errorcode updateResult = XXH64_update(state,
                                                            input_buffer, 
                                                            count);
            if (updateResult == XXH_ERROR) {
                XXH64_freeState(state);
                throw std::runtime_error("xxHash result update error");
            }
        } while (istream);
    }
    else // hash the first [bytes] bytes
    {
        uint64_t bytes_read = 0;
        do
        {
            if (bytes < buffer_size)
            {
                istream.read((char*)input_buffer, bytes);    
            }
            else {
                istream.read((char*)input_buffer, buffer_size);
            }
            const auto count = istream.gcount();
            bytes_read += count;
            const XXH_errorcode updateResult = XXH64_update(state,
                                                            input_buffer, 
                                                            count);
            if (updateResult == XXH_ERROR) {
                XXH64_freeState(state);
                throw std::runtime_error("xxHash result update error");
            }
        } while (istream && bytes_read < bytes);
    }
    
    /* Get the hash */
    const XXH64_hash_t hash = XXH64_digest(state);

    XXH64_freeState(state);
    
    return (uint64_t)hash;
}

string format_bytes(uintmax_t bytes)
{
    const vector<string> prefixes = {"", "kibi", "mebi", "gibi", "tebi", "pebi"};
    size_t i = 0;
    double dbl_bytes = static_cast<double>(bytes);
    while (dbl_bytes > 1024 && i < prefixes.size())
    {
        dbl_bytes = dbl_bytes / 1024;
        ++i;
    }

    std::ostringstream out;
    out.precision(2);
    out << std::fixed << dbl_bytes << " " << prefixes[i] << "bytes";
    return out.str();
}
