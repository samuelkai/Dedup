#include <hashlib++/hashlibpp.h>
#include <iostream>
#include "gather_hashes.h"
using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;

void gather_hashes(const fs::path path, unordered_map<string, vector<string>> &dedup_table)
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
                    string hash = myWrapper.getHashFromFile(path);
                    dedup_table[hash].push_back(fs::canonical(path));
                }
                catch (hlException &e)
                {
                    cout << e.error_message() << endl;
                }
            }
            else if (fs::is_directory(path))
            {
                for (const fs::directory_entry &x : fs::directory_iterator(path))
                {
                    if (fs::is_regular_file(x))
                    {
                        try
                        {
                            string hash = myWrapper.getHashFromFile(x.path());
                            dedup_table[hash].push_back(fs::canonical(x.path()));
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