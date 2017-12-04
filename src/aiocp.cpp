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

extern bool verbose;
extern int aio_threshold;
extern off_t aio_blocksize;
extern unsigned int aio_max_events;
extern unsigned int aio_iocb_count;
extern long aio_timeout_ns;
extern bool aio_fallocate;
extern bool aio_readahead;

namespace aio {

io_context_t ctx;

// Represents a single file copy
struct CopyTask {
static std::map<iocb*, CopyTask*> tasks; // This map is so we can recover the task belonging to the cb in the IO callback

off_t offset = 0;      // offset to use for the next read operation
std::vector<iocb*> free_iocbs;
std::vector<iocb*> queued_iocbs; // iocbs initialized and waiting to be submitted
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
    queued_iocbs.reserve(aio_iocb_count);

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
    assert(queued_iocbs.empty());
    for (iocb* cb : free_iocbs) {
        tasks.erase(cb);
        delete[] (char*) (cb->u.c.buf);
        delete cb;
    }
}

void schedule_next_reads() {
    while (!free_iocbs.empty() && offset < src_sz) {
        iocb* cb = free_iocbs.back();
        free_iocbs.pop_back();

        auto sz = std::min(src_sz - offset, aio_blocksize);
        io_prep_pread(cb, srcfd, cb->u.c.buf, sz, offset);
        io_set_callback(cb, CopyTask::read_done);
	queued_iocbs.push_back(cb);
        offset += sz;
    }

    // submit when we have max iocbs queued or we hit end of file
    if (offset >= src_sz || queued_iocbs.size() == aio_iocb_count) {
	int res = io_submit(ctx, queued_iocbs.size(), queued_iocbs.data());
	if (verbose)
		cout << "--> submitted " << res << " reads" << endl;
	queued_iocbs.erase(queued_iocbs.begin(), queued_iocbs.begin() + res);
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
        schedule_next_reads();
    } else if (free_iocbs.size() == aio_iocb_count) {
        // done: there is no more to copy and we are the last finishing iocb
        // no one else is managing this object so we just delete ourselves
        if (verbose)
            cout << "--> finished an AIO copy" << endl;
        delete this;
    }
}

};

std::map<iocb*, CopyTask*> CopyTask::tasks;

void init() {
    io_queue_init(aio_max_events, &ctx);
}

void cleanup() {
    if (verbose)
        cout << "draining queue" << endl;
    while (!CopyTask::tasks.empty()) {
        handle_events(true);
    }
    io_queue_release(ctx);
}

void handle_events(bool drain) {
    // we read the event queue manually using io_getevents
    // because io_queue_run just naively drains the queue 1 event (= 1 system call) at a time
    // TODO we can sometimes get more than aio_max_events here, why is that? hardcode 64 for now
    static io_event evts[64];

    timespec timeout;
    if (drain) {
        // if draining, no timeout
        timeout = { 0, 0 };
    } else {
        timeout = { 0, aio_timeout_ns };
    }

    int evts_handled = io_getevents(ctx, 0, 64, evts, &timeout);

    for (int i = 0; i < evts_handled; i++) {
        io_callback_t callback = (io_callback_t)evts[i].data;
        callback(ctx, evts[i].obj, evts[i].res, evts[i].res2);
    }

    if (verbose)
        cout << "handled " << evts_handled << " events" << endl;
}

void copy(int srcfd, int destfd, const struct stat& src_stat) {
    CopyTask* ct = new CopyTask { srcfd, destfd, src_stat.st_size };
    ct->schedule_next_reads();
}

}
