#include <libaio.h>
#include <sys/stat.h>

namespace aio {

void init(int blocksize, int max_events, int iocb_count, bool verbose);

void cleanup();

bool handle_events();

void copy(int srcfd, int destfd, const struct stat& src_stat);

}
