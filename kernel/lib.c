// String helpers and a small printf, used by both the kernel and user code.
// The core has no M extension, so the compiler calls __mulsi3 and friends
// for * / %: they are here as shift-and-add loops.

#include "kernel.h"

void *memset(void *dst, int c, uint32_t n) {
    uint8_t *d = dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) a++, b++;
    return (uint8_t)*a - (uint8_t)*b;
}

uint32_t strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

uint32_t __mulsi3(uint32_t a, uint32_t b) {
    uint32_t r = 0;
    for (; b; b >>= 1, a <<= 1)
        if (b & 1) r += a;
    return r;
}

uint32_t __udivsi3(uint32_t n, uint32_t d) {
    uint32_t q = 0, r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= 1u << i;
        }
    }
    return q;
}

uint32_t __umodsi3(uint32_t n, uint32_t d) {
    return n - __mulsi3(__udivsi3(n, d), d);
}

// --- formatting --------------------------------------------------------------

struct out {
    char *buf;
    int cap, len;
};

static void put(struct out *o, char c) {
    if (o->len < o->cap - 1) o->buf[o->len] = c;
    o->len++;
}

static void put_u32(struct out *o, uint32_t v, uint32_t base) {
    char digits[10];
    int n = 0;
    do {
        uint32_t d = __umodsi3(v, base);
        digits[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v = __udivsi3(v, base);
    } while (v);
    while (n) put(o, digits[--n]);
}

// %d %u %x %s %c, nothing else. Returns the full length, like snprintf.
int vformat(char *buf, int cap, const char *fmt, va_list ap) {
    struct out o = {buf, cap, 0};
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            put(&o, *fmt);
            continue;
        }
        switch (*++fmt) {
        case 'd': {
            int v = va_arg(ap, int);
            if (v < 0) put(&o, '-');
            put_u32(&o, v < 0 ? -(uint32_t)v : (uint32_t)v, 10);
            break;
        }
        case 'u': put_u32(&o, va_arg(ap, uint32_t), 10); break;
        case 'x': put_u32(&o, va_arg(ap, uint32_t), 16); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            while (*s) put(&o, *s++);
            break;
        }
        case 'c': put(&o, (char)va_arg(ap, int)); break;
        case '\0': fmt--; break;
        default: put(&o, *fmt); break;
        }
    }
    if (cap > 0) buf[o.len < cap ? o.len : cap - 1] = '\0';
    return o.len;
}

// --- kernel console ----------------------------------------------------------

void kputs(const char *s) {
    while (*s) *PUTCHAR = (uint8_t)*s++;
}

void kprintf(const char *fmt, ...) {
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vformat(buf, sizeof buf, fmt, ap);
    va_end(ap);
    kputs(buf);
}

void halt(uint32_t code) {
    *TOHOST = code;
    for (;;) {
    }
}
