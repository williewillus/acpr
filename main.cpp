#include <iostream>
#include <boost/filesystem.hpp>

using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;

int main(int argc, char** argv)
{
    path p { argv[1] };

    const recursive_directory_iterator end = {};

    for (auto iter = recursive_directory_iterator { p };
         iter != end;
         iter++) {
        std::cout << iter->path() << std::endl;
    }

    return 0;
}
