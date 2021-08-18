#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION

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
 * Holds file-related information.
 */
File::File(std::string _path, std::filesystem::file_time_type _m_time,
           std::size_t _number_of_path) : 
        path(std::move(_path)), m_time(_m_time), number_of_path(_number_of_path)
{
}

/**
 * Exception that is thrown when file stream is not valid.
 */
FileException::FileException(std::error_code ec) :
    std::system_error(ec)
{
}

/**
 * Returns true if the contents of the files in the given paths are exactly
 * the same.
 */
bool compare_files(const string &path1, const string &path2)
{
    std::ifstream f1;
    std::ifstream f2;
    try
    {
        f1.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        f1.open(path1, std::ios::binary|std::ifstream::in);
        
        f2.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        f2.open(path2, std::ios::binary|std::ifstream::in);
    }
    catch(const std::ios_base::failure &e)
    {
        throw FileException(e.code());
    }

    constexpr size_t buffer_size = 4096;
    char input_buffer1[buffer_size];
    char input_buffer2[buffer_size];
    
    // Compare files a buffer size at a time
    do
    {
        try
        {
            f1.read((char*)input_buffer1, buffer_size);
        }
        catch(const std::ios_base::failure &e)
        {
            if (!f1.eof())
            {
                throw FileException(e.code());
            }
        }
        const auto count1 = f1.gcount();

        try
        {
            f2.read((char*)input_buffer2, buffer_size);
        }
        catch(const std::ios_base::failure &e)
        {
            if (!f2.eof())
            {
                throw FileException(e.code());
            }
        }
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
uint64_t hash_file(const string &path, uintmax_t bytes)
{
    std::ifstream istream;
    istream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        istream.open(path, std::ios::binary|std::ifstream::in);
    }
    catch(const std::ios_base::failure &e)
    {
        if (!istream.eof())
        {
            throw std::runtime_error(strerror(errno));
        }
    }

    // Based on example code from xxHash
    XXH3_state_t* const state = XXH3_createState();
    if (state==nullptr) {
        XXH3_freeState(state);
        throw std::runtime_error("xxHash state creation error");
    }

    constexpr size_t buffer_size = 4096;
    char input_buffer[buffer_size];

    const XXH_errorcode resetResult = XXH3_64bits_reset(state);
    if (resetResult == XXH_ERROR) {
        XXH3_freeState(state);
        throw std::runtime_error("xxHash state initialization error");
    }

    if (bytes == 0) // hash the whole file
    {
        do
        {
            try
            {
                istream.read((char*)input_buffer, buffer_size);
            }
            catch(const std::ios_base::failure &e)
            {
                if (!istream.eof())
                {
                    throw std::runtime_error(strerror(errno));
                }
            }
            const auto count = istream.gcount();
            const XXH_errorcode updateResult = XXH3_64bits_update(state,
                                                            input_buffer, 
                                                            count);
            if (updateResult == XXH_ERROR) {
                XXH3_freeState(state);
                throw std::runtime_error("xxHash result update error");
            }
        } while (istream);
    }
    else // hash the first [bytes] bytes
    {
        uint64_t bytes_read = 0;
        do
        {
            try
            {
                if ((bytes - bytes_read) < buffer_size)
                {
                    istream.read((char*)input_buffer, (bytes - bytes_read));
                }
                else {
                    istream.read((char*)input_buffer, buffer_size);
                }
            }
            catch(const std::ios_base::failure &e)
            {
                if (!istream.eof())
                {
                    throw std::runtime_error(strerror(errno));
                }
            }
            const auto count = istream.gcount();
            bytes_read += count;
            const XXH_errorcode updateResult = XXH3_64bits_update(state,
                                                            input_buffer, 
                                                            count);
            if (updateResult == XXH_ERROR) {
                XXH3_freeState(state);
                throw std::runtime_error("xxHash result update error");
            }
        } while (istream && bytes_read < bytes);
    }
    
    /* Get the hash */
    const XXH64_hash_t hash = XXH3_64bits_digest(state);

    XXH3_freeState(state);
    
    return (uint64_t)hash;
}

/**
 * Formats the given bytes as a string with a binary prefix.
 */
string format_bytes(uintmax_t bytes)
{
    const vector<string> prefixes = 
        {"", "kibi", "mebi", "gibi", "tebi", "pebi"};
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

/**
 * Returns the given number of bytes from the beginning of the given file.
 */
BeginningData read_file_beginning(const string &path, uintmax_t bytes)
{
    std::fstream f1;
    try
    {
        f1.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        f1.open(path, std::ios::binary|std::ifstream::in);
    }
    catch(const std::ios_base::failure &e)
    {
        throw FileException(e.code());
    }

    BeginningData input_buffer(bytes);
    try
    {
        f1.read(input_buffer.data(), bytes);
    }
    catch(const std::ios_base::failure &e)
    {
        if (!f1.eof())
        {
            throw FileException(e.code());
        }
        else if (f1.gcount() < static_cast<std::streamsize>(bytes))
        {
            // Pad with zeros
            for (size_t i = f1.gcount(); i < bytes; i++)
            {
                input_buffer[i] = 0;
            }
        }
        
    }
    return input_buffer;
}
