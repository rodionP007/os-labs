#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

struct aio_operation {
    struct aiocb aio;
    char *buffer;
    int write_operation;      // 0 = read, 1 = write
    void* next_operation;
};

struct op_ctx {
    off_t offset;
    size_t buf_size;
    ssize_t last_read;
    int active;   // 1
    int done;     // 1
    volatile sig_atomic_t completed;
};

static int g_fd_in = -1;
static int g_fd_out = -1;
static off_t g_file_size = 0;
static int g_n_ops = 0;
static size_t g_block_size = 0;
static off_t g_total_written = 0;

static void die(const char *where) {
    perror(where);
    exit(EXIT_FAILURE);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) die("clock_gettime");
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static size_t fs_block_size_for_path(const char *path) {
    struct statvfs v;
    if (statvfs(path, &v) != 0) return 4096;
    if (v.f_frsize > 0) return (size_t)v.f_frsize;
    if (v.f_bsize > 0)  return (size_t)v.f_bsize;
    return 4096;
}

void aio_completion_handler(sigval_t sigval) {
    struct aio_operation *op = (struct aio_operation *)sigval.sival_ptr;
    struct op_ctx *ctx = (struct op_ctx*)op->next_operation;
    ctx->completed = 1;
}

static void prepare_common_aiocb(struct aio_operation *op) {
    memset(&op->aio, 0, sizeof(op->aio));
    op->aio.aio_buf = op->buffer;

    op->aio.aio_sigevent.sigev_notify = SIGEV_THREAD;
    op->aio.aio_sigevent.sigev_notify_function = aio_completion_handler;
    op->aio.aio_sigevent.sigev_notify_attributes = NULL;
    op->aio.aio_sigevent.sigev_value.sival_ptr = op;
}

static void submit_read(struct aio_operation *op) {
    struct op_ctx *ctx = (struct op_ctx*)op->next_operation;

    op->write_operation = 0;
    ctx->completed = 0;

    prepare_common_aiocb(op);
    op->aio.aio_fildes = g_fd_in;
    op->aio.aio_offset = ctx->offset;
    op->aio.aio_nbytes = ctx->buf_size;

    if (aio_read(&op->aio) != 0) die("aio_read");
    ctx->active = 1;
}

static void submit_write(struct aio_operation *op, size_t nbytes) {
    struct op_ctx *ctx = (struct op_ctx*)op->next_operation;

    op->write_operation = 1;
    ctx->completed = 0;

    prepare_common_aiocb(op);
    op->aio.aio_fildes = g_fd_out;
    op->aio.aio_offset = ctx->offset;
    op->aio.aio_nbytes = nbytes;

    if (aio_write(&op->aio) != 0) die("aio_write");
    ctx->active = 1;
}

static void usage(const char *p) {
    fprintf(stderr,
        "Usage: %s <src> <dst> <n_ops> <block_size>\n"
        "Example: %s big.bin copy.bin 8 1048576\n"
        "Note: block_size should be multiple of FS block size (often 4096).\n",
        p, p
    );
}

int main(int argc, char **argv) {
    if (argc != 5) { usage(argv[0]); return 2; }

    const char *src = argv[1];
    const char *dst = argv[2];
    g_n_ops = atoi(argv[3]);
    g_block_size = (size_t)strtoull(argv[4], NULL, 10);

    if (g_n_ops <= 0 || g_n_ops > 1024) {
        fprintf(stderr, "n_ops must be 1..1024\n");
        return 2;
    }
    if (g_block_size == 0) {
        fprintf(stderr, "block_size must be > 0\n");
        return 2;
    }
    g_fd_in = open(src, O_RDONLY | O_NONBLOCK, 0666);
    if (g_fd_in < 0) die("open(read_filename)");

    g_fd_out = open(dst, O_CREAT | O_WRONLY | O_TRUNC | O_NONBLOCK, 0666);
    if (g_fd_out < 0) die("open(write_filename)");

    struct stat st;
    if (fstat(g_fd_in, &st) != 0) die("fstat");
    g_file_size = st.st_size;

    size_t fs_block = fs_block_size_for_path(dst);
    if (g_block_size % fs_block != 0) {
        fprintf(stderr,
            "Warning: block_size (%zu) is not multiple of FS block size (%zu).\n",
            g_block_size, fs_block);
    }

    fprintf(stderr, "File size: %" PRId64 " bytes\n", (int64_t)g_file_size);
    fprintf(stderr, "n_ops=%d block_size=%zu fs_block=%zu\n", g_n_ops, g_block_size, fs_block);

    struct aio_operation *ops = (aio_operation*)calloc((size_t)g_n_ops, sizeof(*ops));
    struct op_ctx *ctxs = (op_ctx*)calloc((size_t)g_n_ops, sizeof(*ctxs));
    if (!ops || !ctxs) die("calloc");

    size_t align = fs_block;
    if (align < 4096) align = 4096;

    for (int i = 0; i < g_n_ops; i++) {
        void *buf = NULL;
        int rc = posix_memalign(&buf, align, g_block_size);
        if (rc != 0) {
            fprintf(stderr, "posix_memalign: %s\n", strerror(rc));
            return 1;
        }
        memset(buf, 0, g_block_size);

        ops[i].buffer = (char*)buf;
        ops[i].write_operation = 0;

        ctxs[i].offset = (off_t)i * (off_t)g_block_size;
        ctxs[i].buf_size = g_block_size;
        ctxs[i].last_read = 0;
        ctxs[i].active = 0;
        ctxs[i].done = 0;
        ctxs[i].completed = 0;

        ops[i].next_operation = &ctxs[i];
    }

    uint64_t t0 = now_ns();

    int done_slots = 0;
    for (int i = 0; i < g_n_ops; i++) {
        if (ctxs[i].offset >= g_file_size) {
            ctxs[i].done = 1;
            done_slots++;
            continue;
        }
        submit_read(&ops[i]);
    }
    while (done_slots < g_n_ops) {
        const struct aiocb *list[1024];
        int cnt = 0;

        for (int i = 0; i < g_n_ops; i++) {
            if (ctxs[i].active) list[cnt++] = &ops[i].aio;
        }
        if (cnt == 0) break;

        if (aio_suspend(list, cnt, NULL) != 0) {
            if (errno == EINTR) continue;
            die("aio_suspend");
        }

        for (int i = 0; i < g_n_ops; i++) {
            if (!ctxs[i].active) continue;

            int err = aio_error(&ops[i].aio);
            if (err == EINPROGRESS) continue;
            if (err != 0) { errno = err; die("aio_error"); }

            ssize_t ret = aio_return(&ops[i].aio);
            if (ret < 0) die("aio_return");

            ctxs[i].active = 0;

            if (ops[i].write_operation) {

                g_total_written += ret;

                ctxs[i].offset += (off_t)g_n_ops * (off_t)g_block_size;
                if (ctxs[i].offset >= g_file_size) {
                    ctxs[i].done = 1;
                    done_slots++;
                    continue;
                }
                submit_read(&ops[i]);
            } else {
                if (ret == 0) {
                    ctxs[i].done = 1;
                    done_slots++;
                    continue;
                }
                ctxs[i].last_read = ret;
                submit_write(&ops[i], (size_t)ret);
            }
        }
    }

    if (ftruncate(g_fd_out, g_file_size) != 0) perror("ftruncate (warning)");

    uint64_t t1 = now_ns();
    double sec = (double)(t1 - t0) / 1e9;
    double mib = (double)g_file_size / (1024.0 * 1024.0);
    double speed = (sec > 0.0) ? (mib / sec) : 0.0;

    printf("Copied %" PRId64 " bytes in %.6f s => %.2f MiB/s\n",
           (int64_t)g_file_size, sec, speed);

    for (int i = 0; i < g_n_ops; i++) free(ops[i].buffer);
    free(ops);
    free(ctxs);

    close(g_fd_in);
    close(g_fd_out);
    return 0;
}
