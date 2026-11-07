#include <string.h>

#include "lex/internal.h"

void gbuf_init(gbuf *b, ek_arena *arena)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->arena = arena;
}

static int gbuf_reserve(gbuf *b, size_t need)
{
    size_t cap;
    char *p;

    if (b->len + need <= b->cap) {
        return 1;
    }
    cap = b->cap ? b->cap : 64;
    while (cap < b->len + need) {
        cap *= 2;
    }
    p = ek_arena_alloc(b->arena, cap);
    if (!p) {
        return 0;
    }
    if (b->len > 0) {
        memcpy(p, b->data, b->len);
    }
    b->data = p;
    b->cap = cap;
    return 1;
}

int gbuf_put(gbuf *b, const void *bytes, size_t n)
{
    if (!gbuf_reserve(b, n)) {
        return 0;
    }
    memcpy(b->data + b->len, bytes, n);
    b->len += n;
    return 1;
}

int gbuf_char(gbuf *b, char c)
{
    return gbuf_put(b, &c, 1);
}
