// Demo programs. Each runs as a user-mode process; build.py picks which ones
// the kernel starts at boot.

#include "user.h"

static void spin(int n) {
    for (volatile int i = 0; i < n; i++) {
    }
}

int hello_main(void) {
    printf("hello from pid %d\n", getpid());
    return 0;
}

// ping and pong never yield: they only alternate because the timer
// preempts them.
static int count(const char *name) {
    for (int i = 0; i < 4; i++) {
        spin(400);
        printf("%s %d\n", name, i);
    }
    return 0;
}

int ping_main(void) { return count("ping"); }
int pong_main(void) { return count("pong"); }

int sleeper_main(void) {
    printf("sleeper: sleep 6 ticks\n");
    sleep(6);
    printf("sleeper: awake\n");
    return 7;
}

int files_main(void) {
    char buf[64];
    int fd = open("data", O_WRITE | O_CREAT | O_TRUNC);
    uint32_t sum = 0;
    // 1500 bytes: the file needs two pages
    for (int i = 0; i < 1500; i += 50) {
        for (int j = 0; j < 50; j++) buf[j] = (char)(i + j);
        write(fd, buf, 50);
    }
    close(fd);

    fd = open("data", O_RDONLY);
    int n, total = 0;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        for (int j = 0; j < n; j++) sum += (uint8_t)buf[j];
        total += n;
    }
    close(fd);
    printf("files: read %d bytes, sum %u\n", total, sum);

    fd = open("data", O_WRITE | O_APPEND);
    write(fd, "tail", 4);
    close(fd);
    fd = open("data", O_RDONLY);
    total = 0;
    while ((n = read(fd, buf, sizeof buf)) > 0) total += n;
    close(fd);
    printf("files: after append %d bytes\n", total);

    printf("files: open missing = %d\n", open("nope", O_RDONLY));
    fd = open("data", O_RDONLY);
    printf("files: write to read-only fd = %d\n", write(fd, "x", 1));
    close(fd);
    printf("files: read bad fd = %d\n", read(3, buf, 1));
    printf("files: bad pointer = %d\n", write(STDOUT, (void *)0xfff0, 0x100));
    return 0;
}

// writer and reader share a file through the filesystem.
int writer_main(void) {
    int fd = open("msg", O_WRITE | O_CREAT | O_TRUNC);
    write(fd, "note from the writer", 20);
    close(fd);
    printf("writer: done\n");
    return 0;
}

int reader_main(void) {
    char buf[32];
    sleep(2);
    int fd = open("msg", O_RDONLY);
    int n = read(fd, buf, sizeof buf - 1);
    buf[n > 0 ? n : 0] = '\0';
    close(fd);
    printf("reader: '%s'\n", buf);
    return 0;
}

// A CSR access from user mode is an illegal instruction: the kernel kills
// the process and the others keep running.
int bad_main(void) {
    printf("bad: reading mstatus\n");
    uint32_t v;
    __asm__ volatile("csrr %0, mstatus" : "=r"(v));
    printf("bad: still alive?\n");
    return (int)v;
}

// --- shell -------------------------------------------------------------------

// A tiny shell. There is no keyboard, so it runs a fixed script.
static const char *script[] = {
    "pid",
    "echo hello > greet",
    "echo world >> greet",
    "cat greet",
    "cat missing",
    "sleep 1",
    "frobnicate",
    0,
};

static int split(char *line, char **argv, int max) {
    int argc = 0;
    while (*line && argc < max) {
        while (*line == ' ') *line++ = '\0';
        if (!*line) break;
        argv[argc++] = line;
        while (*line && *line != ' ') line++;
    }
    return argc;
}

static void cat(const char *path) {
    char buf[32];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("cat: %s: error %d\n", path, -fd);
        return;
    }
    int n;
    while ((n = read(fd, buf, sizeof buf)) > 0) write(STDOUT, buf, (uint32_t)n);
    close(fd);
}

static void echo(char **argv, int argc) {
    int flags = O_WRITE | O_CREAT;
    int fd = STDOUT, words = argc;
    if (argc >= 3 && (strcmp(argv[argc - 2], ">") == 0 ||
                      strcmp(argv[argc - 2], ">>") == 0)) {
        flags |= argv[argc - 2][1] ? O_APPEND : O_TRUNC;
        fd = open(argv[argc - 1], flags);
        words = argc - 2;
    }
    for (int i = 1; i < words; i++) {
        write(fd, argv[i], strlen(argv[i]));
        write(fd, i + 1 < words ? " " : "\n", 1);
    }
    if (fd != STDOUT) close(fd);
}

int sh_main(void) {
    char line[48];
    char *argv[6];
    for (int i = 0; script[i]; i++) {
        printf("$ %s\n", script[i]);
        uint32_t len = strlen(script[i]);
        for (uint32_t j = 0; j <= len; j++) line[j] = script[i][j];
        int argc = split(line, argv, 6);
        if (strcmp(argv[0], "pid") == 0) {
            printf("%d\n", getpid());
        } else if (strcmp(argv[0], "echo") == 0) {
            echo(argv, argc);
        } else if (strcmp(argv[0], "cat") == 0 && argc == 2) {
            cat(argv[1]);
        } else if (strcmp(argv[0], "sleep") == 0 && argc == 2) {
            sleep((uint32_t)(argv[1][0] - '0'));
        } else {
            printf("sh: %s: unknown command\n", argv[0]);
        }
    }
    return 0;
}

// --- benchmarks --------------------------------------------------------------

#ifndef BENCH_N
#define BENCH_N 50
#endif

int yielder_main(void) {
    for (int i = 0; i < BENCH_N; i++) yield();
    return 0;
}

int getpid_loop_main(void) {
    for (int i = 0; i < BENCH_N; i++) getpid();
    return 0;
}
