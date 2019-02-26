#include <iostream>
#include <hashlib++/hashlibpp.h>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::vector;
namespace fs = std::filesystem;

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        cout << "Usage: main path\n";
        return 1;
    }

    fs::path p(argv[1]);
    hashwrapper *myWrapper = new md5wrapper();
    try
    {
        // cout << myWrapper->getHashFromString("Hello world") << endl;
        // cout << myWrapper->getHashFromFile("/etc/group") << endl;
        cout << fs::current_path() << endl;
    }
    catch (hlException &e)
    {
        cout << e.error_message() << endl;
    }

    unordered_map<string, vector<string>> dedup_table;

    try
    {
        if (exists(p))
        {
            if (is_regular_file(p))
            {
                cout << p << " size is " << file_size(p) << '\n';
                try
                {
                    string hash = myWrapper->getHashFromFile(p);
                    // If the hash already exists
                    if (dedup_table.find(hash) != dedup_table.end())
                    {
                        dedup_table[hash].push_back(fs::canonical(p));
                    }
                    else
                    {
                        dedup_table[hash] = vector<string>{fs::canonical(p)};
                    }
                    cout << "   " << hash << endl;
                }
                catch (hlException &e)
                {
                    cout << e.error_message() << endl;
                }
            }
            else if (is_directory(p))
            {
                cout << p << " is a directory containing:\n";

                for (const fs::directory_entry &x : fs::directory_iterator(p))
                {
                    cout << "    " << x.path();
                    if (fs::is_regular_file(x))
                    {
                        try
                        {
                            string hash = myWrapper->getHashFromFile(x.path());
                            // If the hash already exists
                            if (dedup_table.find(hash) != dedup_table.end())
                            {
                                dedup_table[hash].push_back(fs::canonical(x.path()));
                            }
                            else
                            {
                                dedup_table[hash] = vector<string>{fs::canonical(x.path())};
                            }
                            cout << "   " << hash << endl;
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
                cout << p << " exists, but is not a regular file or directory\n";
            }
        }
        else
        {
            cout << p << " does not exist\n";
        }
    }
    catch (const fs::filesystem_error &ex)
    {
        cout << ex.what() << '\n';
    }

    for (const auto &pair : dedup_table)
    {
        cout << "Key:[" << pair.first << "] Values:[ ";
        for (const auto &path : pair.second)
        {
            cout << path << " ";
        }
        cout << "]" << endl;
    }

    delete myWrapper;
    myWrapper = NULL;
}