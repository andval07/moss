// Page allocator: the RAM between the end of .bss and the kernel stack, cut
// into 1 KiB pages and kept on a free list. The link pointer lives in the
// first word of each free page.

#include "kernel.h"

extern char __pages_start[], __pages_end[];

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
static int nfree, ntotal;

void page_init(void) {
    // push in reverse so the first page_alloc() returns the lowest page
    for (char *p = __pages_end - PAGE_SIZE; p >= __pages_start; p -= PAGE_SIZE) {
        page_free(p);
        ntotal++;
    }
}

void *page_alloc(void) {
    struct free_page *page = free_list;
    if (!page) return 0;
    free_list = page->next;
    nfree--;
    memset(page, 0, PAGE_SIZE);
    return page;
}

void page_free(void *page) {
    struct free_page *f = page;
    f->next = free_list;
    free_list = f;
    nfree++;
}

int pages_free(void) { return nfree; }
int pages_total(void) { return ntotal; }
