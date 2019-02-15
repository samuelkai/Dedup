#include <iostream>
#include <hashlib++/hashlibpp.h>
#include <boost/filesystem.hpp>
using std::cout;
using std::endl;
using namespace boost::filesystem;

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        cout << "Usage: tut3 path\n";
        return 1;
    }

    path p(argv[1]);
    hashwrapper *myWrapper = new md5wrapper();
    try
    {
        cout << myWrapper->getHashFromString("Hello world") << endl;
        cout << myWrapper->getHashFromFile("/etc/group") << endl;
        cout << current_path() << endl;
    }
    catch (hlException &e)
    {
        cout << e.error_message() << endl;
    }
    delete myWrapper;
    myWrapper = NULL;

    try
    {
        if (exists(p))
        {
            if (is_regular_file(p))
                cout << p << " size is " << file_size(p) << '\n';

            else if (is_directory(p))
            {
                cout << p << " is a directory containing:\n";

                for (directory_entry &x : directory_iterator(p))
                    cout << "    " << x.path() << '\n';
            }
            else
                cout << p << " exists, but is not a regular file or directory\n";
        }
        else
            cout << p << " does not exist\n";
    }

    catch (const filesystem_error &ex)
    {
        cout << ex.what() << '\n';
    }
}