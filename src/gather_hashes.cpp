#include <sys/stat.h>
#include <hashlib++/hashlibpp.h>
#include <iostream>
#include <cstring>
#include "MurmurHash3.h"
#include "gather_hashes.h"
using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;

void gather_hashes(const fs::path path, unordered_map<uint64_t, vector<string>> &dedup_table)
{
    md5wrapper myWrapper;
    try
    {
        if (fs::exists(path))
        {
            if (fs::is_regular_file(path))
            {
                try
                {
                    //KESKEN
                    uint32_t seed = 1;
                    uint64_t hash_otpt[2];  // allocate 128 bits
                    FILE *key;
                    if((key = fopen(path.c_str(), "rb")) == NULL)
                    {
                        throw hlException(HL_FILE_READ_ERROR,
                                "Cannot read file \"" + 
                                path.string() + 
                                "\".");
                    }
                    struct stat st;
                    stat(path.c_str(), &st);
                    int size = st.st_size;
                    MurmurHash3_x64_128(key, size, seed, hash_otpt);
                    fclose(key);
                    dedup_table[hash_otpt[0]].push_back(fs::canonical(path));
                }
                catch (hlException &e)
                {
                    cout << e.error_message() << endl;
                }
            }
            else if (fs::is_directory(path))
            {
                for (const fs::directory_entry &dir_entry : fs::directory_iterator(path))
                {
                    if (fs::is_regular_file(dir_entry))
                    {
                        try
                        {
                            uint32_t seed = 1;
                            uint64_t hash_otpt[2];  // allocate 128 bits
                            FILE *pFile;
                            long lSize;
                            size_t result;
                            if((pFile = fopen(dir_entry.path().c_str(), "rb")) == NULL)
                            {
                                cout << "Virhe" << endl;
                                break;
                            }
                            
                            // obtain file size:
                            fseek (pFile , 0 , SEEK_END);
                            lSize = ftell (pFile);
                            rewind (pFile);

                            // allocate memory to contain the whole file:
                            char *buffer = new char[lSize];
                            if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

                            // copy the file into the buffer:
                            result = fread (buffer,1,lSize,pFile);
                            if (result != (size_t)lSize) {fputs ("Reading error",stderr); exit (3);}

                            fclose (pFile);
                            MurmurHash3_x64_128(buffer, lSize, seed, hash_otpt);
                            delete[] buffer;
                            dedup_table[hash_otpt[0]].push_back(fs::canonical(dir_entry));
                        }
                        catch (hlException &e)
                        {
                            cout << e.error_message() << endl;
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
        cout << ex.what() << '\n';
    }
}