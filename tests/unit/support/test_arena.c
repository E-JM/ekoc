#include "harness.h"

#include <stdint.h>
#include <string.h>

#include "support/arena.h"

static void test_basic_allocation(void)
{
    ek_arena *a = ek_arena_create();
    char *p;
    size_t i;

    EK_CHECK(a != NULL);

    p = ek_arena_alloc(a, 10);
    EK_CHECK(p != NULL);
    memset(p, 'x', 10);
    for (i = 0; i < 10; i++) {
        EK_CHECK_U64((unsigned char)p[i], 'x');
    }

    /* size 0 hands out one aligned slot and must not disturb the block */
    EK_CHECK(ek_arena_alloc(a, 0) != NULL);
    for (i = 0; i < 10; i++) {
        EK_CHECK_U64((unsigned char)p[i], 'x');
    }
    ek_arena_destroy(a);
}

static void test_alignment(void)
{
    ek_arena *a = ek_arena_create();
    size_t align = ek_arena_align();
    size_t sizes[] = { 1, 2, 3, 5, 8, 13, 21, 34, 16, 17 };
    size_t i;

    EK_CHECK(align != 0 && (align & (align - 1)) == 0);
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        void *p = ek_arena_alloc(a, sizes[i]);
        EK_CHECK(p != NULL);
        EK_CHECK_U64((uintptr_t)p % align, 0);
    }
    ek_arena_destroy(a);
}

static void test_growth_across_blocks(void)
{
    enum { COUNT = 300, SIZE = 1000 }; /* 300 KB: several default blocks */
    ek_arena *a = ek_arena_create();
    unsigned char *cells[COUNT];
    size_t i, j;

    for (i = 0; i < COUNT; i++) {
        cells[i] = ek_arena_alloc(a, SIZE);
        EK_CHECK(cells[i] != NULL);
        memset(cells[i], (unsigned char)(i % 251), SIZE);
    }
    for (i = 0; i < COUNT; i++) {
        for (j = 0; j < SIZE; j++) {
            if (cells[i][j] != (unsigned char)(i % 251)) {
                EK_CHECK_U64(cells[i][j], (unsigned char)(i % 251));
                ek_arena_destroy(a);
                return;
            }
        }
    }
    EK_CHECK(1);
    ek_arena_destroy(a);
}

static void test_oversized_allocation(void)
{
    enum { BIG = 64 * 1024 }; /* larger than any default block */
    ek_arena *a = ek_arena_create();
    unsigned char *big, *small;
    size_t i;

    big = ek_arena_alloc(a, BIG);
    EK_CHECK(big != NULL);
    EK_CHECK_U64((uintptr_t)big % ek_arena_align(), 0);
    memset(big, 0xAB, BIG);

    small = ek_arena_alloc(a, 8);
    EK_CHECK(small != NULL);
    memset(small, 0xCD, 8);
    for (i = 0; i < BIG; i += 997) {
        EK_CHECK_U64(big[i], 0xAB);
    }
    ek_arena_destroy(a);
}

int main(void)
{
    test_basic_allocation();
    test_alignment();
    test_growth_across_blocks();
    test_oversized_allocation();
    return ek_test_report("support/arena");
}
