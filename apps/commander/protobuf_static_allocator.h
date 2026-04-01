#include <stdint.h>
#include <string.h>

// ── Static memory pool ────────────────────────────────────────────────────────
#define PROTOBUF_POOL_SIZE 1024

typedef struct {
    uint8_t  buf[PROTOBUF_POOL_SIZE];
    size_t   used;
} MemPool;

// Reset before each unpack
void pool_reset(MemPool *p) {
    p->used = 0;
}

// Bump allocator — no free needed
void *pool_alloc(void *allocator_data, size_t size) {
    MemPool *p = (MemPool *)allocator_data;

    // Align to 8 bytes
    size = (size + 7) & ~7;

    if (p->used + size > PROTOBUF_POOL_SIZE) {
        return NULL;   // out of pool memory
    }

    void *ptr = &p->buf[p->used];
    p->used += size;
    return ptr;
}

// No-op free — pool is reset all at once
void pool_free(void *allocator_data, void *ptr) {
    (void)allocator_data;
    (void)ptr;
    // intentionally empty
}