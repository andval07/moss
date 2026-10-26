#ifndef MOSS_ABI_H
#define MOSS_ABI_H

// System call numbers, in a7. Arguments in a0-a2, result in a0.
#define SYS_EXIT   1
#define SYS_GETPID 2
#define SYS_YIELD  3
#define SYS_SLEEP  4
#define SYS_OPEN   5
#define SYS_READ   6
#define SYS_WRITE  7
#define SYS_CLOSE  8

// open() flags
#define O_RDONLY 0
#define O_WRITE  1
#define O_CREAT  2
#define O_TRUNC  4
#define O_APPEND 8

// errors, returned negated
#define ENOENT 2
#define EBADF  9
#define ENOMEM 12
#define EFAULT 14
#define EINVAL 22
#define EMFILE 24
#define ENOSPC 28
#define ENOSYS 38

#endif
