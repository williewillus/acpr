#include <iostream>
#include <libaio.h>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "aiocp.h"

using std::cerr;
using std::endl;

// Max IO events per CopyTask
static const int MAX_EVENTS = 32;

#define IO_BLKSIZE ((off_t) (64*1024))

static constexpr int iops_needed(off_t filesz) {
    if (filesz % IO_BLKSIZE == 0) {
        return filesz / IO_BLKSIZE;
    } else {
        return (filesz / IO_BLKSIZE) + 1;
    }
}

// Represents a single file copy
struct CopyTask {
static std::map<io_context_t, CopyTask*> tasks; // Each file copy has its own io_context_t. This map is so we can recover the task in the IO callback

io_context_t ctx;
// io submits in progress
int busy;
// io submits remaining
int tocopy;
int srcfd;
int destfd;

static void write_done(io_context_t ctx, iocb* cb, long bytes_written, long ec) {
    if (ec) {
        cerr << "error in write " << ec << endl;
        exit(1);
    }

    if (bytes_written != cb->u.c.nbytes) {
        cerr << "expected to write " << cb->u.c.nbytes << " bytes but wrote " << bytes_written << endl;
        exit(1);
    }

    CopyTask* task = tasks[ctx];
    task->tocopy--;
    task->busy--;
    delete[] ((char*) cb->u.c.buf);
    delete cb;
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

    CopyTask* task = tasks[ctx];
    io_prep_pwrite(cb, task->destfd, cb->u.c.buf, cb->u.c.nbytes, cb->u.c.offset);
    io_set_callback(cb, write_done);
    if (io_submit(ctx, 1, &cb) != 1) {
        cerr << "failed to submit write from read" << endl;
        exit(1);
    }
}

public:
CopyTask(int srcfd, int destfd)
    : srcfd(srcfd),
      destfd(destfd) {
    io_queue_init(MAX_EVENTS, &ctx);
    tasks[ctx] = this;
}

~CopyTask() {
    tasks.erase(tasks.find(ctx));
    io_queue_release(ctx);
    ctx = io_context_t{};
    ::close(srcfd);
    ::close(destfd);
}

};

std::map<io_context_t, CopyTask*> CopyTask::tasks;

// copies srcfd to destfd synchronously using kernel AIO, up to MAX_EVENTS at a time. closes fds after completion.
void copy(int srcfd, int destfd, const struct stat& src_stat) {
    CopyTask ct = CopyTask { srcfd, destfd };
    ct.tocopy = iops_needed(src_stat.st_size);

    int offset = 0;
    // This loops until copy is done, eventually we want to move some of this logic into the callback handler so reads/writes are scheduled on-the-fly instead of all up front here
    while (ct.tocopy > 0) {
        int n = std::min(std::min(MAX_EVENTS - ct.busy, MAX_EVENTS / 2), iops_needed(src_stat.st_size - offset));
        if (n > 0) {
            iocb* ioq[n];
            for (int i = 0; i < n; i++) {
                iocb* io = new iocb{};
                int iosize = std::min(src_stat.st_size - offset, IO_BLKSIZE);
                char* buf = new char[IO_BLKSIZE]();
                io_prep_pread(io, srcfd, buf, iosize, offset);
                io_set_callback(io, CopyTask::read_done);
                ioq[i] = io;
                offset += iosize;
            }

            if (io_submit(ct.ctx, n, ioq) != n) {
                cerr << "error submitting" << endl;
                exit(1);
            }
            ct.busy += n;
        }

        if (io_queue_run(ct.ctx) < 0) {
            cerr << "error running" << endl;
            exit(1);
        }
    }
}
