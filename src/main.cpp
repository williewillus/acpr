#include <iostream>
#include <boost/filesystem.hpp>
#include <libaio.h>

using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using std::cout;
using std::cerr;
using std::endl;

#define MAX_TASKS 32

// libaio documentation (+ example of usage)
// http://manpages.ubuntu.com/manpages/precise/en/man3/io.3.html

int main(int argc, char** argv)
{
    if (argc >= 2) {
        path p { argv[1] };

        const recursive_directory_iterator end = {};

        for (auto iter = recursive_directory_iterator { p };
             iter != end;
             iter++) {
            cout << iter->path() << endl;
        }
    }

    return 0;
}
