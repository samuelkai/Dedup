#include <iostream>
#include <hashlib++/hashlibpp.h>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
using std::cin;
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::vector;
namespace ch = std::chrono;
namespace fs = std::filesystem;

/*
* Calculates hash values of files and stores them
* in the deduplication table.
* 
* Path can be a file or a directory.
* If it is a file, its hash is calculated.
* If it is a directory, the hash of each file in the
* directory (non-recursively) will be calculated.
*/
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

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Usage: dedup PATH\n";
        return 1;
    }
    
    std::clock_t start = std::clock();

    fs::path path(argv[1]);
    unordered_map<string, vector<string>> dedup_table;

    gather_hashes(path, dedup_table);
    while (true)
    {
        cout << "Input another path to be included in "
        "the deduplication or finish with N" << endl;

        string input;
        getline(cin, input);
        if (input == "N")
        {
            break;
        }

        gather_hashes(fs::path(input), dedup_table);
    }

    cout << endl
         << "Contents of the deduplication table:\n"
         << endl;

    for (const auto &pair : dedup_table)
    {
        cout << "Key: " << pair.first << "\nValues:\n";
        for (const auto &path : pair.second)
        {
            cout << path << endl;
        }
        cout << endl;
    }

    double time = (std::clock() - start) / (double)CLOCKS_PER_SEC;
    std::cout << "Execution took " << time << " seconds." << std::endl;

}