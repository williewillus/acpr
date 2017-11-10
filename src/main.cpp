#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <iostream>
#include <libaio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using std::cerr;
using std::cout;
using std::endl;

// 1K (change me after testing)
#define AIO_THRESHOLD (1024)
#define MAX_TASKS (32)
// 64K chunk size for now
#define IO_CHUNK_SIZE (64 * 1024)

// libaio documentation (+ example of usage)
// http://manpages.ubuntu.com/manpages/precise/en/man3/io.3.html

int main(int argc, char **argv) {
  if (argc < 3) {
      cout << "Usage: " << argv[0] << " <src> <dest>" << endl;
      exit(1);
  }

  const path src { argv[1] };
  const path dest { argv[2] };

  const recursive_directory_iterator end = {};
  for (auto iter = recursive_directory_iterator{src}; iter != end; iter++) {
    const path& p_src = iter->path();
    const path relative = boost::filesystem::relative(p_src, src);
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
                // TODO for educational purposes, implement this by hand if we have time
                boost::filesystem::copy_file(p_src, p_dest, boost::filesystem::copy_option::overwrite_if_exists);
            } else {
                int srcfd = ::open(p_dest.string().c_str(), O_WRONLY | O_CREAT, s.st_mode);
                if (srcfd == -1)
                    perror("failed to touch output file");
                else {
                    cout << "+ touched file " << relative << endl;

                    // todo spawn AIO task
                    ::close(srcfd);
                }
            }

        }
    }
  }

  // Example reading readme using aio?
  int srcfd = ::open("../README.txt", O_RDONLY);
  if (srcfd < 0)
      perror("failed to open");

  io_context_t ctx {};
  io_queue_init(MAX_TASKS, &ctx);

  // set up the task
  iocb cb {}; // this probably goes on the heap in a real program
  char* buf = new char[IO_CHUNK_SIZE]();
  io_prep_pread(&cb, srcfd, buf, IO_CHUNK_SIZE, 0);

  // set up completion callback
  io_set_callback(&cb, [](io_context_t ctx, iocb* cb, long res, long res2) -> void {
    cout << "read " << res << " bytes. data:" << endl;
    cout << (char*) cb->u.c.buf << endl;
  });

  // submit just one task
  iocb* cbs[] = { &cb };
  if (io_submit(ctx, 1, cbs) < 0)
      cerr << "error submitting" << endl;

  // sychronously await
  io_queue_run(ctx);

  io_queue_release(ctx);
  delete[] buf;
  return 0;
}
