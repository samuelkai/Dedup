#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <limits>

#include "xxHash/xxhash.h"
#include "utilities.h"

using std::cin;
using std::cout;
using std::endl;
namespace fs = std::filesystem;

bool compare_files(const fs::path p1, const fs::path p2)
{
    fs::path path1 = fs::canonical(p1);
    fs::path path2 = fs::canonical(p2);
    std::ifstream f1(path1, std::ios::binary|std::ifstream::ate);
    std::ifstream f2(path2, std::ios::binary|std::ifstream::ate);

    if (f1.fail() || f2.fail())
    {
        return false; //file problem
    }

    if (f1.tellg() != f2.tellg())
    {
        return false; //size mismatch
    }

    //seek back to beginning
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);

    size_t const buffer_size = 8192;
    char input_buffer1[buffer_size];
    char input_buffer2[buffer_size];
    
    do
    {
        f1.read((char*)input_buffer1, buffer_size);
        auto count1 = f1.gcount();

        f2.read((char*)input_buffer2, buffer_size);
        auto count2 = f2.gcount();

        if (count1 != count2 ||
            memcmp(input_buffer1, input_buffer2, count1))
        {
            return false; // Files are not equal
        }
    } while (f1 && f2);
    return true;
}

uint64_t hash_file(const fs::path p)
{
    fs::path path = fs::canonical(p);
    std::ifstream istream(path, std::ios::binary);

    // Based on example code from xxHash
    XXH64_state_t* const state = XXH64_createState();
    if (state==NULL) {
        XXH64_freeState(state);
        throw std::runtime_error("xxHash state creation error");
    }

    size_t const buffer_size = 4096;
    char input_buffer[buffer_size];
    if (input_buffer==NULL) {
        XXH64_freeState(state);
        throw std::runtime_error("Memory allocation error");
    }

    /* Initialize state with selected seed */
    unsigned long long const seed = 0;   /* or any other value */
    XXH_errorcode const resetResult = XXH64_reset(state, seed);
    if (resetResult == XXH_ERROR) {
        XXH64_freeState(state);
        throw std::runtime_error("xxHash state initialization error");
    }

    do
    {
        istream.read((char*)input_buffer, buffer_size);
        auto count = istream.gcount();
        XXH_errorcode const updateResult = XXH64_update(state,
                                                        input_buffer, count);
        if (updateResult == XXH_ERROR) {
            XXH64_freeState(state);
            throw std::runtime_error("xxHash result update error");
        }
    } while (istream);

    /* Get the hash */
    XXH64_hash_t const hash = XXH64_digest(state);

    /* State can then be re-used; in this example, it is simply freed  */
    XXH64_freeState(state);
    
    return (uint64_t)hash;
}

uint64_t hash_file(const fs::path p, uint64_t bytes)
{
    fs::path path = fs::canonical(p);
    std::ifstream istream(path, std::ios::binary);

    // Based on example code from xxHash
    XXH64_state_t* const state = XXH64_createState();
    if (state==NULL) {
        XXH64_freeState(state);
        throw std::runtime_error("xxHash state creation error");
    }

    size_t const buffer_size = 4096;
    char input_buffer[buffer_size];
    if (input_buffer==NULL) {
        XXH64_freeState(state);
        throw std::runtime_error("Memory allocation error");
    }

    /* Initialize state with selected seed */
    unsigned long long const seed = 0;   /* or any other value */
    XXH_errorcode const resetResult = XXH64_reset(state, seed);
    if (resetResult == XXH_ERROR) {
        XXH64_freeState(state);
        throw std::runtime_error("xxHash state initialization error");
    }

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
        auto count = istream.gcount();
        bytes_read += count;
        XXH_errorcode const updateResult = XXH64_update(state,
                                                        input_buffer, count);
        if (updateResult == XXH_ERROR) {
            XXH64_freeState(state);
            throw std::runtime_error("xxHash result update error");
        }
    } while (istream && bytes_read < bytes);

    /* Get the hash */
    XXH64_hash_t const hash = XXH64_digest(state);

    /* State can then be re-used; in this example, it is simply freed  */
    XXH64_freeState(state);
    
    return (uint64_t)hash;
}