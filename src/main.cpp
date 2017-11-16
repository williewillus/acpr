#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <iostream>

#include "aiocp.h"
#include "boost_backport.h"

using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using std::cerr;
using std::cout;
using std::endl;

// 1K (change me after testing)
#define AIO_THRESHOLD (1024)

// libaio documentation (+ example of usage)
// http://manpages.ubuntu.com/manpages/precise/en/man3/io.3.html

int main(int argc, char **argv) {
  if (argc < 3) {
      cout << "Usage: " << argv[0] << " <src> <dest>" << endl;
      exit(1);
  }

  const path src { argv[1] };
  const path dest { argv[2] };
  aio::init();

  const recursive_directory_iterator end = {};
  for (auto iter = recursive_directory_iterator{src}; iter != end; iter++) {
    const path& p_src = iter->path();
    const path relative = boost_backport::relative(p_src, src);
    const path p_dest = dest / relative;

    cout << "-> should copy " << p_src << " to " << p_dest << endl;

    struct stat s;
    if (::stat(p_src.string().c_str(), &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            /* create dir and copy all metadata
             * we can do this here becuase recursive_directory_iterator is breadth-first (dirs before contents)
             * and we want to ensure dirs exist before copying
             */
            if (!mkdir(p_dest.string().c_str(), s.st_mode)) {
                cout << "+ made dir " << relative << endl;
            } else {
                std::string msg { "- couldn't make dest directory " };
                msg.append(p_dest.string());
                perror(msg.c_str());
                return 1;
            }
        } else if (s.st_mode & S_IFREG) {
            /* open src/dest file and copy all metadata
             * if file is small enough, copy directly, otherwise spawn AIO task
             */
            if (s.st_size <= AIO_THRESHOLD) {
                cout << "--> Copying file normally since it's small" << endl;
                // TODO for educational purposes, implement this by hand if we have time
                boost::filesystem::copy_file(p_src, p_dest, boost::filesystem::copy_option::overwrite_if_exists);
            } else {
                int srcfd = ::open(p_src.string().c_str(), O_RDONLY);
                int destfd = ::open(p_dest.string().c_str(), O_WRONLY | O_CREAT, s.st_mode);
                if (srcfd == -1)
                    perror("failed to open in file");
                else if (destfd == -1)
                    perror("failed to open out file");
                else {                    
                    cout << "--> Copying using AIO" << endl;
                    aio::copy(srcfd, destfd, s);
                }
            }
        }
    }

    // each iteration, handle finished events
    aio::handle_events();
  }

  // handle any outstanding events
  while (!aio::handle_events())
      ;

  aio::cleanup();
  return 0;
}
