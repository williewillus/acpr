#include <libaio.h>
#include <sys/stat.h>

namespace aio {

void init(int blocksize, int max_events, int iocb_count, long aio_timeout_ns);

void cleanup();

void handle_events(bool drain);

void copy(int srcfd, int destfd, const struct stat& src_stat);

}
