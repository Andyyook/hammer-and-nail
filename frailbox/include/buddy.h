#ifndef FRAILBOX_BUDDY_H
#define FRAILBOX_BUDDY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Buddy Allocator — replaces bump-pointer arena with free-capable allocator.
 *
 * The allocator manages memory in powers of 2. Each allocation is rounded up
 * to the next power of 2, and the buddy system tracks free blocks by size.
 * When a block is freed, its buddy is checked — if both are free, they merge
 * back into the next larger size class.
 *
 * This is a C implementation for the frailbox sandbox. It uses mmap for
 * the backing memory and supports compile-time coexistence with the arena.
 */

#define BUDDY_MIN_ORDER   4     /* 16 bytes — minimum block size */
#define BUDDY_MAX_ORDER   22    /* 4 MB — maximum block size */
#define BUDDY_LEVELS      (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)

/* Opaque buddy allocator handle */
typedef struct buddy_allocator buddy_allocator_t;

/* Fragmentation statistics */
typedef struct buddy_stats {
    uint64_t total_memory;       /* Total managed memory (bytes) */
    uint64_t free_memory;        /* Currently free memory (bytes) */
    uint64_t allocated_memory;   /* Currently allocated (bytes) */
    uint64_t allocation_count;   /* Number of successful allocations */
    uint64_t free_count;         /* Number of successful frees */
    uint64_t max_usable;         /* Largest allocatable block (bytes) */
    int      free_blocks[BUDDY_LEVELS]; /* Free blocks per order */
} buddy_stats_t;

/* ── Core API ────────────────────────────────────────────────── */

/* Create a buddy allocator managing total_size bytes (rounded to 2^max_order) */
buddy_allocator_t *buddy_create(size_t total_size);

/* Destroy the allocator and release all memory */
void buddy_destroy(buddy_allocator_t *ba);

/* Allocate size bytes. Returns NULL on failure. */
void *buddy_alloc(buddy_allocator_t *ba, size_t size);

/* Free a previously allocated pointer. */
void buddy_free(buddy_allocator_t *ba, void *ptr);

/* Return fragmentation/usage statistics */
buddy_stats_t buddy_stats(const buddy_allocator_t *ba);

/* Reset the allocator (free all blocks) */
void buddy_reset(buddy_allocator_t *ba);

#ifdef __cplusplus
}
#endif

#endif /* FRAILBOX_BUDDY_H */
