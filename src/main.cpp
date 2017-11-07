#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <iostream>
#include <libaio.h>

using boost::filesystem::path;
using boost::filesystem::recursive_directory_iterator;
using std::cerr;
using std::cout;
using std::endl;

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

  const recursive_directory_iterator end = {};
  for (auto iter = recursive_directory_iterator{path{argv[1]}}; iter != end; iter++) {
    cout << iter->path() << endl;
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
    cout << "read " << res << "bytes. data:" << endl;
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
