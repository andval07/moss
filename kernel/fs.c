// A flat RAM filesystem: one directory of fixed slots, file data in pages
// from the page allocator. Open files live in a global table so two
// descriptors can share one offset (stdin and stdout share the console).

#include "kernel.h"

#define NFILES     8
#define NAME_MAX   15
#define FILE_PAGES 4
#define NOPEN      16

struct file {
    char name[NAME_MAX + 1];
    uint32_t size;
    uint8_t *page[FILE_PAGES];
};

struct ofile {
    struct file *file;  // 0 for the console
    uint32_t offset;
    int flags;
    int refs;
};

static struct file files[NFILES];
static struct ofile open_files[NOPEN];
static struct ofile console = {0, 0, O_WRITE, 0};

void fs_init(void) {}

static struct file *lookup(const char *path) {
    for (int i = 0; i < NFILES; i++)
        if (files[i].name[0] && strcmp(files[i].name, path) == 0) return &files[i];
    return 0;
}

static struct file *create(const char *path) {
    if (strlen(path) > NAME_MAX) return 0;
    for (int i = 0; i < NFILES; i++) {
        if (!files[i].name[0]) {
            memcpy(files[i].name, path, strlen(path) + 1);
            files[i].size = 0;
            return &files[i];
        }
    }
    return 0;
}

static void truncate(struct file *f) {
    for (int i = 0; i < FILE_PAGES; i++) {
        if (f->page[i]) page_free(f->page[i]);
        f->page[i] = 0;
    }
    f->size = 0;
}

static int alloc_fd(struct proc *p, struct ofile *of) {
    for (int fd = 0; fd < NOFILE; fd++) {
        if (!p->fd[fd]) {
            p->fd[fd] = of;
            of->refs++;
            return fd;
        }
    }
    return -EMFILE;
}

int fs_dup_console(struct proc *p) { return alloc_fd(p, &console); }

int fs_open(struct proc *p, const char *path, int flags) {
    if (!path[0]) return -EINVAL;
    struct file *f = lookup(path);
    if (!f && (flags & O_CREAT)) {
        f = create(path);
        if (!f) return -ENOSPC;
    }
    if (!f) return -ENOENT;

    struct ofile *of = 0;
    for (int i = 0; i < NOPEN; i++) {
        if (!open_files[i].refs) {
            of = &open_files[i];
            break;
        }
    }
    if (!of) return -EMFILE;
    int fd = alloc_fd(p, of);
    if (fd < 0) return fd;
    if (flags & O_TRUNC) truncate(f);
    of->file = f;
    of->offset = 0;
    of->flags = flags;
    return fd;
}

static struct ofile *get(struct proc *p, int fd) {
    if (fd < 0 || fd >= NOFILE) return 0;
    return p->fd[fd];
}

int fs_close(struct proc *p, int fd) {
    struct ofile *of = get(p, fd);
    if (!of) return -EBADF;
    of->refs--;
    p->fd[fd] = 0;
    return 0;
}

int fs_read(struct proc *p, int fd, char *buf, uint32_t len) {
    struct ofile *of = get(p, fd);
    if (!of) return -EBADF;
    struct file *f = of->file;
    if (!f) return 0;  // console input: always end of file
    if (of->offset >= f->size) return 0;
    if (len > f->size - of->offset) len = f->size - of->offset;
    for (uint32_t i = 0; i < len; i++, of->offset++)
        buf[i] = (char)f->page[of->offset / PAGE_SIZE][of->offset % PAGE_SIZE];
    return (int)len;
}

int fs_write(struct proc *p, int fd, const char *buf, uint32_t len) {
    struct ofile *of = get(p, fd);
    if (!of || !(of->flags & O_WRITE)) return -EBADF;
    struct file *f = of->file;
    if (!f) {
        for (uint32_t i = 0; i < len; i++) *PUTCHAR = (uint8_t)buf[i];
        return (int)len;
    }
    if (of->flags & O_APPEND) of->offset = f->size;
    uint32_t done = 0;
    for (; done < len; done++, of->offset++) {
        uint32_t index = of->offset / PAGE_SIZE;
        if (index >= FILE_PAGES) break;
        if (!f->page[index] && !(f->page[index] = page_alloc())) break;
        f->page[index][of->offset % PAGE_SIZE] = (uint8_t)buf[done];
    }
    if (of->offset > f->size) f->size = of->offset;
    if (done == 0 && len > 0) return -ENOSPC;
    return (int)done;
}
