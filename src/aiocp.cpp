#include <cassert>
#include <iostream>
#include <libaio.h>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "aiocp.h"

using std::cout;
using std::cerr;
using std::endl;

static off_t aio_blocksize;
static unsigned int aio_iocb_count;
static bool aio_verbose;

namespace aio {

io_context_t ctx;

// Represents a single file copy
struct CopyTask {
static std::map<iocb*, CopyTask*> tasks; // This map is so we can recover the task belonging to the cb in the IO callback

off_t offset = 0;      // offset to use for the next read operation
std::vector<iocb*> free_iocbs;
const int srcfd;
const int destfd;
const off_t src_sz;

static void write_done(io_context_t ctx, iocb* cb, long bytes_written, long ec) {
    if (ec) {
        cerr << "error in write " << ec << endl;
        exit(1);
    }

    if (bytes_written != cb->u.c.nbytes) {
        cerr << "expected to write " << cb->u.c.nbytes << " bytes but wrote " << bytes_written << endl;
        exit(1);
    }        

    CopyTask* task = tasks.at(cb);
    if (task == nullptr) {
        cerr << "couldn't find task for cb!" << endl;
        exit(1);
    }

    task->handle_write(cb);
}

static void read_done(io_context_t ctx, iocb* cb, long bytes_read, long ec) {
    if (ec) {
        cerr << "error in read " << ec << endl;
        exit(1);
    }

    if (bytes_read != cb->u.c.nbytes) {
        cerr << "expected to read " << cb->u.c.nbytes << " bytes but read " << bytes_read << endl;
        exit(1);
    }

    CopyTask* task = tasks.at(cb);
    if (task == nullptr) {
        cerr << "couldn't find task for cb!" << endl;
        exit(1);
    }

    task->handle_read(cb);
}

public:
CopyTask(int srcfd, int destfd, off_t src_sz)
    : srcfd(srcfd),
      destfd(destfd),
      src_sz(src_sz) {
    free_iocbs.reserve(aio_iocb_count);

    for (unsigned int i = 0; i < aio_iocb_count; i++) {
        iocb* cb = new iocb{};
        cb->u.c.buf = new char[aio_blocksize];
        free_iocbs.push_back(cb);
        tasks[cb] = this;
    }
}

~CopyTask() {
    ::close(srcfd);

    ::fsync(destfd);
    ::close(destfd);

    assert(free_iocbs.size() == aio_iocb_count);
    for (unsigned int i = 0; i < aio_iocb_count; i++) {
        iocb* cb = free_iocbs[i];

        tasks.erase(cb);
        delete[] (char*) (cb->u.c.buf);
        delete cb;
    }
}

// spawn up to aio_iocb_count more reads
void schedule_next_reads() {
    iocb* submit[aio_iocb_count];
    int num_submit = 0;

    while (!free_iocbs.empty() && offset < src_sz) {
        iocb* cb = free_iocbs.back();
        free_iocbs.pop_back();

        auto sz = std::min(src_sz - offset, aio_blocksize);
        io_prep_pread(cb, srcfd, cb->u.c.buf, sz, offset);
        io_set_callback(cb, CopyTask::read_done);
        submit[num_submit++] = cb;
        offset += sz;
    }

    if (io_submit(ctx, num_submit, submit) != num_submit) {
        cerr << "failed to submit read" << endl;
        exit(1);
    } else if (num_submit > 1) {
        if (aio_verbose)
            cout << "--> submitted " << num_submit << " reads" << endl;
    }
}

void handle_read(iocb* cb) {    
    io_prep_pwrite(cb, destfd, cb->u.c.buf, cb->u.c.nbytes, cb->u.c.offset);
    io_set_callback(cb, CopyTask::write_done);
    if (io_submit(ctx, 1, &cb) != 1) {
        cerr << "failed to submit write from read" << endl;
        exit(1);
    }
}

void handle_write(iocb* cb) {
    free_iocbs.push_back(cb);

    if (offset < src_sz) {
        // TODO: there is still the case where a big file gets N CB's scheduled up front, then schedules additional ones 1-by-1.
        // even so, there will always be N CB's in flight, so is this a real problem?
        schedule_next_reads();
    } else if (free_iocbs.size() == aio_iocb_count) {
        // done: there is no more to copy and we are the last finishing iocb
        // no one else is managing this object so we just delete ourselves
        if (aio_verbose)
            cout << "--> finished an AIO copy" << endl;
        delete this;
    }
}

};

std::map<iocb*, CopyTask*> CopyTask::tasks;

void init(int blocksize, int max_events, int iocb_count, bool verbose) {
    aio_blocksize = blocksize;
    io_queue_init(max_events, &ctx);
    assert(iocb_count > 0);
    aio_iocb_count = iocb_count;
    aio_verbose = verbose;
}

void cleanup() {
    io_queue_release(ctx);
}

bool handle_events() {
    if (CopyTask::tasks.empty()) {
        return true;
    } else {
        // drive the event queue, callbacks will be invoked from here
        io_queue_run(ctx);
        return false;
    }
}

void copy(int srcfd, int destfd, const struct stat& src_stat) {
    CopyTask* ct = new CopyTask { srcfd, destfd, src_stat.st_size };
    ct->schedule_next_reads();
}

}
