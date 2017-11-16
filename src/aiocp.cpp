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

// TODO: should this be decided per-file? what's a good size?
#define IO_BLKSIZE ((off_t) (64*1024))

namespace aio {

io_context_t ctx;

// Represents a single file copy
// Currently reads/writes are scheduled block by block in order
// TODO: Use multiple cb's and schedule multiple blocks at once?
struct CopyTask {
static std::map<iocb*, CopyTask*> tasks; // This map is so we can recover the task belonging to the cb in the IO callback

off_t offset = 0;      // current offset we are reading to/writing from
char buf[IO_BLKSIZE] = {}; // buffer used for all IO on this file
iocb cb {}; // iocb used for all IO on this file
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

    task->handle_write();
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

    task->handle_read();
}

public:
CopyTask(int srcfd, int destfd, off_t src_sz)
    : srcfd(srcfd),
      destfd(destfd),
      src_sz(src_sz) {
    cb.u.c.buf = buf;
    tasks[&cb] = this;
}

~CopyTask() {
    ::close(srcfd);

    ::fsync(destfd);
    ::close(destfd);
}

void schedule_next_read() {
    // offset is set correctly at this point
    io_prep_pread(&cb, srcfd, cb.u.c.buf, std::min(src_sz - offset, IO_BLKSIZE), offset);
    io_set_callback(&cb, CopyTask::read_done);
    iocb* arr = &cb;
    if (io_submit(ctx, 1, &arr) != 1) {
        cerr << "failed to submit read" << endl;
        exit(1);
    }
}

void handle_read() {
    // convert directly into a write
    io_prep_pwrite(&cb, destfd, cb.u.c.buf, cb.u.c.nbytes, cb.u.c.offset);
    io_set_callback(&cb, CopyTask::write_done);
    iocb* arr = &cb;
    if (io_submit(ctx, 1, &arr) != 1) {
        cerr << "failed to submit write from read" << endl;
        exit(1);
    }
}

void handle_write() {
    offset += cb.u.c.nbytes;

    if (offset < src_sz) {
        schedule_next_read();
    } else {
        // we're done so remove from tasks map
        // no one else is managing this object so we just delete ourselves
        cout << "--> finished an AIO copy" << endl;
        tasks.erase(tasks.find(&cb));
        delete this;
    }
}

};

std::map<iocb*, CopyTask*> CopyTask::tasks;

void init() {
    // receive up to 32 events every time we call handle_events
    io_queue_init(32, &ctx);
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
    ct->schedule_next_read();
}

}
