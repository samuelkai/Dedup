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
namespace fs = std::filesystem;

hashwrapper *myWrapper;
unordered_map<string, vector<string>> dedup_table;

void gather_hashes(fs::path path)
{
    try
    {
        if (fs::exists(path))
        {
            if (fs::is_regular_file(path))
            {
                try
                {
                    string hash = myWrapper->getHashFromFile(path);
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
                            string hash = myWrapper->getHashFromFile(x.path());
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

    cout << argv[1] << endl;
    fs::path path(argv[1]);
    myWrapper = new md5wrapper();

    gather_hashes(path);

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

        gather_hashes(fs::path(input));
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

    delete myWrapper;
    myWrapper = NULL;
}