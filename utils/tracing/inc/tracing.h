#ifndef TRACING_H
#define TRACING_H

#define TRACING_ENABLED 1
#if TRACING_ENABLED
#include <stdio.h>
#define TRACE(fmt, ...) do { \
    fprintf(stdout, "[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
} while (0)
#else
#define TRACE(fmt, ...) do { } while (0)
#endif

#endif /* TRACING_H */