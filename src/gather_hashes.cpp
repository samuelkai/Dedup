#include "XXH/xxhash.h"
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
using std::cin;
using std::cout;
using std::endl;
using std::cerr;
using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;

uint64_t hash_file(const fs::path p) {
    fs::path path = fs::canonical(p);
    std::ifstream istream(path, std::ios::binary);

    // Based on example code from xxHash
    XXH64_state_t* const state = XXH64_createState();
    if (state==NULL) {
        throw std::runtime_error("xxHash state creation error");
    }

    size_t const buffer_size = 8192;
    void* const input_buffer = malloc(buffer_size);
    if (input_buffer==NULL) {
        throw std::runtime_error("Memory allocation error");
    }

    /* Initialize state with selected seed */
    unsigned long long const seed = 0;   /* or any other value */
    XXH_errorcode const resetResult = XXH64_reset(state, seed);
    if (resetResult == XXH_ERROR) {
        throw std::runtime_error("xxHash state initialization error");
    }

    while ( istream.read((char*)input_buffer, buffer_size) ) {
        auto count = istream.gcount();   
        XXH_errorcode const updateResult = XXH64_update(state, input_buffer, count);
        if (updateResult == XXH_ERROR) {
            throw std::runtime_error("xxHash result update error");
        }
    }
    auto count = istream.gcount();
    if (count) {
        XXH_errorcode const updateResult = XXH64_update(state, input_buffer, count);
        if (updateResult == XXH_ERROR) {
            throw std::runtime_error("xxHash result update error");
        }
    }

    /* Get the hash */
    XXH64_hash_t const hash = XXH64_digest(state);

    /* State can then be re-used; in this example, it is simply freed  */
    free(input_buffer);
    XXH64_freeState(state);
    
    return (uint64_t)hash;
}

void gather_hashes(const fs::path path, unordered_map<uint64_t, vector<string>> &dedup_table)
{
    try
    {
        if (fs::exists(path))
        {
            if (fs::is_regular_file(path))
            {
                try {
                    dedup_table[hash_file(path)].push_back(path);
                } catch (const std::exception &ex) {
                    cerr << ex.what() << ": file " << path << '\n';
                }
            }
            else if (fs::is_directory(path))
            {
                for (const fs::directory_entry &dir_entry : fs::directory_iterator(path))
                {
                    if (fs::is_regular_file(dir_entry))
                    {
                        try {
                            fs::path p = dir_entry.path();
                            dedup_table[hash_file(p)].push_back(p);
                        } catch (const std::exception &ex) {
                           cerr << ex.what() << ": file " << dir_entry << '\n';
                        }
                    }
                    else
                    {
                        cout << endl;
                    }
                }
            }
            else
            {
                cout << path << " exists, but is not a regular file or directory\n";
            }
        }
        else
        {
            cout << path << " does not exist\n";
        }
    }
    catch (const fs::filesystem_error &ex)
    {
        cerr << ex.what() << '\n';
    }
}