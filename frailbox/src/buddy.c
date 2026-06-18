/*
 * Buddy Allocator — C implementation for frailbox sandbox.
 *
 * Memory is managed as binary partitioned blocks (powers of 2).
 * Free blocks are tracked in per-order free lists using an embedded
 * free-list structure (the first sizeof(free_header) bytes of each
 * free block store the next/prev pointers).
 *
 * The buddy of a block at address `addr` with size `(1 << order)` is:
 *   buddy_addr = addr ^ (1 << order)
 *
 * This XOR property means split blocks can always find their partner
 * at the same order without additional metadata.
 */

#include "buddy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

/* ── Internal constants ──────────────────────────────────────── */

/* Minimum block size: 2^BUDDY_MIN_ORDER = 16 bytes (just header + pointer) */
/* We use one word of each free block as a linked-list pointer. */

/* ── Block header (stored at the start of every allocated block) ── */
/* For free blocks the memory is reused as free-list pointers. */
typedef struct block_header {
    uint32_t order;      /* Block size order (0 = 2^BUDDY_MIN_ORDER) */
    uint32_t magic;      /* Magic number for corruption detection */
} block_header_t;

#define BUDDY_MAGIC 0xBADB0001U
#define HEADER_SIZE (sizeof(block_header_t))

/* Round up to next power of 2 */
static inline size_t round_up_pow2(size_t x) {
    if (x <= 1) return 1;
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
}

/* Compute order for a block of given size (bytes) */
static inline int size_to_order(size_t size) {
    int order = 0;
    while ((size_t)(1U << (order + BUDDY_MIN_ORDER)) < size) order++;
    return order;
}

/* Convert order to byte size */
static inline size_t order_to_size(int order) {
    return (size_t)1 << (order + BUDDY_MIN_ORDER);
}

/* ── Allocator structure ─────────────────────────────────────── */

struct buddy_allocator {
    void   *base;                  /* Base address of managed memory */
    size_t  total_size;            /* Total managed size in bytes */
    int     max_order;             /* Maximum order index */
    int     min_order;             /* Minimum order index (= 0) */

    /* Free lists per order. free_lists[o] = head of free list for order o */
    void   *free_lists[BUDDY_LEVELS];

    /* Stats */
    buddy_stats_t stats;
};

/* ── Embedded free-list helpers ───────────────────────────────── */

/* In a free block, the first pointer-sized word is the "next" pointer */
static inline void **free_next_ptr(void *block) {
    return (void **)block;
}

/* The second pointer-sized word is the "prev" pointer (for doubly-linked list,
   used primarily for O(1) removal during coalescing) */
static inline void **free_prev_ptr(void *block) {
    return (void **)((char *)block + sizeof(void *));
}

static void free_list_push(buddy_allocator_t *ba, int order, void *block) {
    void **next = free_next_ptr(block);
    void **prev = free_prev_ptr(block);
    *next = ba->free_lists[order];
    *prev = NULL;
    if (ba->free_lists[order]) {
        *free_prev_ptr(ba->free_lists[order]) = block;
    }
    ba->free_lists[order] = block;
    ba->stats.free_memory += order_to_size(order);
    ba->stats.free_blocks[order]++;
}

static void free_list_remove(buddy_allocator_t *ba, int order, void *block) {
    void **next = free_next_ptr(block);
    void **prev = free_prev_ptr(block);

    if (*prev) {
        *free_next_ptr(*prev) = *next;
    } else {
        ba->free_lists[order] = *next;
    }
    if (*next) {
        *free_prev_ptr(*next) = *prev;
    }

    ba->stats.free_memory -= order_to_size(order);
    ba->stats.free_blocks[order]--;
}

/* ── Buddy computation ────────────────────────────────────────── */

/* The buddy of a block at `addr` with size `(1 << order)` in bytes */
static inline void *buddy_of(void *base, void *addr, int order) {
    size_t block_size = order_to_size(order);
    uintptr_t offset = (uintptr_t)((char *)addr - (char *)base);
    uintptr_t buddy_offset = offset ^ block_size;
    return (char *)base + buddy_offset;
}

/* Check if a given address is within our managed region and
   is the start of a block at the given order */
static inline int is_valid_block(const buddy_allocator_t *ba, void *addr, int order) {
    if (addr < ba->base) return 0;
    if ((const char *)addr >= (const char *)ba->base + ba->total_size) return 0;
    size_t block_size = order_to_size(order);
    uintptr_t offset = (uintptr_t)((char *)addr - (char *)ba->base);
    if (offset % block_size != 0) return 0;
    return 1;
}

/* ── Core: split a block ───────────────────────────────────────── */
/* Split a block at order `order` into two blocks at order `order-1`.
   The lower half stays on the free list; the upper half is returned. */
static void *split_block(buddy_allocator_t *ba, int order) {
    if (order <= 0) return NULL;

    /* Take a free block from the current order */
    void *block = ba->free_lists[order];
    if (!block) return NULL;
    free_list_remove(ba, order, block);

    /* Split into two buddies at order-1 */
    int lower_order = order - 1;
    size_t half_size = order_to_size(lower_order);

    /* Upper half */
    void *upper = (char *)block + half_size;

    /* Free both halves */
    free_list_push(ba, lower_order, block);
    free_list_push(ba, lower_order, upper);

    return block; /* Return the lower block (still on free list) */
}

/* ── Core: coalesce buddies ─────────────────────────────────────── */
/* Try to merge a block at `order` with its buddy. If the buddy is
   also free, remove both and add the merged block at order+1. */
static void coalesce(buddy_allocator_t *ba, void *block, int order) {
    int current_order = order;

    while (current_order < ba->max_order) {
        void *buddy = buddy_of(ba->base, block, current_order);
        if (!is_valid_block(ba, buddy, current_order)) break;

        /* Check if buddy is free: walk the free list */
        int found = 0;
        void *curr = ba->free_lists[current_order];
        while (curr) {
            if (curr == buddy) {
                found = 1;
                break;
            }
            curr = *free_next_ptr(curr);
        }
        if (!found) break;

        /* Remove both and merge */
        free_list_remove(ba, current_order, block);
        free_list_remove(ba, current_order, buddy);

        /* The merged block is the lower-address half */
        void *merged = (block < buddy) ? block : buddy;
        current_order++;
        free_list_push(ba, current_order, merged);
        block = merged;
    }
}

/* ── API Implementation ────────────────────────────────────────── */

buddy_allocator_t *buddy_create(size_t total_size) {
    /* Round total_size to the next power of 2, limited to max order */
    size_t real_size = round_up_pow2(total_size);
    if (real_size < (size_t)order_to_size(BUDDY_MIN_ORDER)) {
        real_size = order_to_size(BUDDY_MIN_ORDER);
    }
    if (real_size > order_to_size(BUDDY_MAX_ORDER)) {
        real_size = order_to_size(BUDDY_MAX_ORDER);
    }

    int max_order = size_to_order(real_size);

    /* Allocate the controller */
    buddy_allocator_t *ba = calloc(1, sizeof(buddy_allocator_t));
    if (!ba) return NULL;

    /* Allocate backing memory */
    ba->base = mmap(NULL, real_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ba->base == MAP_FAILED) {
        free(ba);
        return NULL;
    }

    ba->total_size = real_size;
    ba->max_order = max_order;
    ba->min_order = 0;

    /* Initialize: the entire region is one free block at max_order */
    ba->free_lists[max_order] = ba->base;
    void **next = free_next_ptr(ba->base);
    void **prev = free_prev_ptr(ba->base);
    *next = NULL;
    *prev = NULL;

    ba->stats.total_memory = real_size;
    ba->stats.free_memory = real_size;
    ba->stats.free_blocks[max_order] = 1;
    ba->stats.max_usable = real_size;

    return ba;
}

void buddy_destroy(buddy_allocator_t *ba) {
    if (!ba) return;
    if (ba->base) {
        munmap(ba->base, ba->total_size);
    }
    memset(ba, 0, sizeof(buddy_allocator_t));
    free(ba);
}

void *buddy_alloc(buddy_allocator_t *ba, size_t size) {
    if (!ba || size == 0) return NULL;

    /* Account for header */
    size_t needed = size + HEADER_SIZE;
    if (needed < HEADER_SIZE) return NULL; /* overflow */

    /* Round up to minimum block size */
    if (needed < order_to_size(0)) {
        needed = order_to_size(0);
    }

    int target_order = size_to_order(needed);
    if (target_order < 0) target_order = 0;
    if (target_order > ba->max_order) return NULL;

    /* Find a free block at the target order or larger */
    int order = target_order;
    while (order <= ba->max_order && !ba->free_lists[order]) {
        order++;
    }
    if (order > ba->max_order) return NULL;

    /* Split until we reach the target order */
    while (order > target_order) {
        if (!split_block(ba, order)) return NULL;
        order--;
    }

    /* Take the block from the free list */
    void *block = ba->free_lists[target_order];
    if (!block) return NULL;
    free_list_remove(ba, target_order, block);

    /* Write header */
    block_header_t *hdr = (block_header_t *)block;
    hdr->order = (uint32_t)target_order;
    hdr->magic = BUDDY_MAGIC;

    /* Update stats */
    size_t block_size = order_to_size(target_order);
    ba->stats.allocated_memory += block_size;
    ba->stats.allocation_count++;
    if (ba->stats.allocated_memory > ba->stats.max_usable) {
        /* Recompute max_usable: largest free block */
        for (int i = ba->max_order; i >= 0; i--) {
            if (ba->free_lists[i]) {
                ba->stats.max_usable = order_to_size(i) - HEADER_SIZE;
                break;
            }
        }
    }

    /* Return the usable memory after the header */
    return (char *)block + HEADER_SIZE;
}

void buddy_free(buddy_allocator_t *ba, void *ptr) {
    if (!ba || !ptr) return;

    void *block = (char *)ptr - HEADER_SIZE;
    block_header_t *hdr = (block_header_t *)block;

    /* Validate */
    if (hdr->magic != BUDDY_MAGIC) {
        fprintf(stderr, "buddy_free: corrupted block header (magic=0x%x, ptr=%p)\n",
                hdr->magic, ptr);
        return;
    }

    int order = (int)hdr->order;
    if (order < 0 || order > ba->max_order) {
        fprintf(stderr, "buddy_free: invalid order %d\n", order);
        return;
    }

    /* Clear magic to detect double-free */
    hdr->magic = 0;

    /* Update stats */
    size_t block_size = order_to_size(order);
    ba->stats.allocated_memory -= block_size;
    ba->stats.free_count++;

    /* Return block to free list and try to coalesce */
    free_list_push(ba, order, block);
    coalesce(ba, block, order);

    /* Update max_usable */
    for (int i = ba->max_order; i >= 0; i--) {
        if (ba->free_lists[i]) {
            ba->stats.max_usable = order_to_size(i) - HEADER_SIZE;
            break;
        }
    }
}

buddy_stats_t buddy_stats(const buddy_allocator_t *ba) {
    return ba->stats;
}

void buddy_reset(buddy_allocator_t *ba) {
    if (!ba) return;

    /* Clear all free lists */
    for (int i = 0; i <= ba->max_order; i++) {
        ba->free_lists[i] = NULL;
    }

    /* Reinitialize: one free block at max_order */
    ba->free_lists[ba->max_order] = ba->base;
    void **next = free_next_ptr(ba->base);
    void **prev = free_prev_ptr(ba->base);
    *next = NULL;
    *prev = NULL;

    ba->stats.allocated_memory = 0;
    ba->stats.free_memory = ba->total_size;
    ba->stats.allocation_count = 0;
    ba->stats.free_count = 0;
    ba->stats.max_usable = ba->total_size;
    memset(ba->stats.free_blocks, 0, sizeof(ba->stats.free_blocks));
    ba->stats.free_blocks[ba->max_order] = 1;
}
