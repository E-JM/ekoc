#ifndef EK_SUPPORT_ARENA_H
#define EK_SUPPORT_ARENA_H

#include <stddef.h>

typedef struct ek_arena ek_arena;

/* Returns NULL when the arena itself cannot be allocated. */
ek_arena *ek_arena_create(void);

/* Returns NULL on exhaustion of the host allocator; previously returned
   blocks stay usable. size 0 allocates one aligned slot. */
void *ek_arena_alloc(ek_arena *arena, size_t size);

/* Alignment of every allocation, a multiple of the host's maximum
   fundamental alignment. */
size_t ek_arena_align(void);

void ek_arena_destroy(ek_arena *arena);

#endif
