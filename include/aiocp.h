#include <libaio.h>
#include <sys/stat.h>

namespace aio {

void init();

void cleanup();

void handle_events(bool drain);

void copy(int srcfd, int destfd, const struct stat& src_stat);

}
