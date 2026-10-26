#ifndef MOSS_USER_H
#define MOSS_USER_H

#include <stdarg.h>
#include <stdint.h>

#include "abi.h"

void exit(int code) __attribute__((noreturn));
int getpid(void);
int yield(void);
int sleep(uint32_t ticks);
int open(const char *path, int flags);
int read(int fd, void *buf, uint32_t len);
int write(int fd, const void *buf, uint32_t len);
int close(int fd);

// lib.c, shared with the kernel
int vformat(char *buf, int cap, const char *fmt, va_list ap);
uint32_t strlen(const char *s);
int strcmp(const char *a, const char *b);

int printf(const char *fmt, ...);

#define STDOUT 1

#endif
