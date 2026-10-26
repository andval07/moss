// User side helpers on top of the system calls.

#include "user.h"

// One write per call, so a line is never cut by a context switch.
int printf(const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vformat(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 1) n = sizeof buf - 1;
    return write(STDOUT, buf, (uint32_t)n);
}
