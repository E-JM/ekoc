#include "support/arena.h"

#include <stdint.h>
#include <stdlib.h>

/* Blocks chain forward from head; allocation bumps only the head block,
   so a failed fit chains a fresh block and older blocks keep their
   remaining space. */

struct block {
    struct block *next;
    size_t used;
    size_t cap;
};

struct ek_arena {
    struct block *head;
    size_t block_size;
};

/* C99 has no max_align_t; a union of fundamental types has a size that
   is a multiple of the host's maximum fundamental alignment, which is
   all align_up needs since malloc already returns maximally aligned
   memory. */
union align_unit {
    long double ld;
    void *ptr;
    long long ll;
};

#define ARENA_BLOCK_SIZE ((size_t)4096)

static size_t align_up(size_t n)
{
    const size_t a = sizeof(union align_unit);
    return (n + a - 1) & ~(a - 1);
}

static struct block *block_new(struct ek_arena *arena, size_t need)
{
    const size_t header = align_up(sizeof(struct block));
    size_t payload = need > arena->block_size ? need : arena->block_size;
    struct block *block = malloc(header + payload);
    if (block == NULL) {
        return NULL;
    }
    block->next = arena->head;
    block->used = 0;
    block->cap = payload;
    arena->head = block;
    return block;
}

ek_arena *ek_arena_create(void)
{
    struct ek_arena *arena = malloc(sizeof *arena);
    if (arena == NULL) {
        return NULL;
    }
    arena->head = NULL;
    arena->block_size = ARENA_BLOCK_SIZE;
    return arena;
}

void *ek_arena_alloc(ek_arena *arena, size_t size)
{
    struct block *block;
    size_t need;

    if (arena == NULL || size > SIZE_MAX / 2) {
        return NULL;
    }
    need = align_up(size == 0 ? 1 : size);
    if (arena->head == NULL || arena->head->cap - arena->head->used < need) {
        block = block_new(arena, need);
    } else {
        block = arena->head;
    }
    if (block == NULL) {
        return NULL;
    } else {
        char *p = (char *)block + align_up(sizeof(struct block)) + block->used;
        block->used += need;
        return p;
    }
}

size_t ek_arena_align(void)
{
    return sizeof(union align_unit);
}

void ek_arena_destroy(ek_arena *arena)
{
    struct block *block = arena == NULL ? NULL : arena->head;
    while (block != NULL) {
        struct block *next = block->next;
        free(block);
        block = next;
    }
    free(arena);
}
