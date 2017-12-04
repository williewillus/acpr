#include <boost/filesystem.hpp>
#include <getopt.h>
#include <fcntl.h>
#include <iostream>

#include "aiocp.h"
#include "boost_backport.h"

using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using std::cerr;
using std::cout;
using std::endl;

bool verbose = false;

static void print_usage() {
  cout << "acpr [OPTIONS] <from> <to>" << endl;
  cout << "  -b: How many KB to copy per AIO operation" << endl;
  cout << "  -c: How many iocbs each file copy uses" << endl;
  cout << "  -m: max_events for the AIO system" << endl;
  cout << "  -n: How many ns to wait when polling for events" << endl;
  cout << "  -t: Threshold to use AIO over normal copy, in bytes" << endl;
  cout << "  -f: Call fallocate before copy" << endl;
  cout << "  -r: Use readahead before copy" << endl;
  cout << "  -h: Display this message" << endl;
  cout << "  -v: Enable verbose output" << endl;
}

int main(int argc, char **argv) {
  int aio_threshold = 1024; // todo reset this default higher after we're done tuning
  int aio_blocksize = 64 * 1024;
  int aio_max_events = 32;
  int aio_iocb_count = 5;
  long aio_timeout_ns = 5000000;
  bool aio_fallocate = false;
  bool aio_readahead = false;

  int c = '0';
  while ((c = getopt(argc, argv, "b:c:m:n:t:hfrv")) != -1) {
      switch (c) {
      case 'b': aio_blocksize = std::stoi(optarg) * 1024; break;
      case 'c': aio_iocb_count = std::stoi(optarg); break;
      case 'm': aio_max_events = std::stoi(optarg); break;
      case 'n': aio_timeout_ns = std::stol(optarg); break;
      case 't': aio_threshold = std::stoi(optarg); break;
      case 'h': print_usage(); return 0;
      case 'f': aio_fallocate = true; break;
      case 'r': aio_readahead = true; break;
      case 'v': verbose = true; break;
      default: throw std::runtime_error("Illegal option");
      }
  }

  assert(aio_threshold > 0);
  assert(aio_blocksize > 0);
  assert(aio_max_events > 0);
  assert(aio_iocb_count > 0);

  if (verbose) {
    cout << "threshold " << aio_threshold << endl;
    cout << "blksize " << aio_blocksize << endl;
    cout << "maxevt " << aio_max_events << endl;
    cout << "cbcount " << aio_iocb_count << endl;
    cout << "timeout " << aio_timeout_ns << endl;
    cout << "fallocate " << aio_fallocate << endl;
    cout << "readahead " << aio_readahead << endl;
  }

  if (argc - optind < 2) {
      cout << "Missing src or dest file" << endl;
      print_usage();
      return 1;
  }

  const path src { argv[optind] };
  const path dest { argv[optind + 1] };
  aio::init(aio_blocksize, aio_max_events, aio_iocb_count, aio_timeout_ns);

  const recursive_directory_iterator end = {};
  for (auto iter = recursive_directory_iterator{src}; iter != end; iter++) {
    const path& p_src = iter->path();
    const path relative = boost_backport::relative(p_src, src);
    const path p_dest = dest / relative;

    if (verbose)
        cout << "-> should copy " << p_src << " to " << p_dest << endl;

    struct stat s;

    if (::stat(dest.string().c_str(), &s) < 0) {
        /* create dest directory if it doesn't already exist
         * to match cp -r default behavior
         */
        if(::stat(src.string().c_str(), &s) < 0) {
            perror("src dir does not exist");
            exit(EXIT_FAILURE);
        }
        if (!mkdir(dest.string().c_str(), s.st_mode))
            if (verbose)
                cout << "+ made dir " << dest << endl;
    }

    if (::stat(p_src.string().c_str(), &s) == 0) {
        if (s.st_mode & S_IFDIR) {
            /* create dir and copy all metadata
             * we can do this here becuase recursive_directory_iterator is breadth-first (dirs before contents)
             * and we want to ensure dirs exist before copying
             */
            if (!mkdir(p_dest.string().c_str(), s.st_mode)) {
                if (verbose)
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
            if (s.st_size <= aio_threshold) {
                if (verbose)
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
                    if (aio_fallocate && fallocate(destfd, 0, 0, s.st_size))
                        perror("failed to preallocate");
                    if (aio_readahead && readahead(srcfd, 0, s.st_size))
                        perror("failed to readahead");
                    if (verbose)
                        cout << "--> Copying using AIO" << endl;
                    aio::copy(srcfd, destfd, s);
                }
            }
        }
    }

    // each iteration, handle finished events
    aio::handle_events(false);
  }

  aio::cleanup();
  return 0;
}
