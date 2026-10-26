#include "harness.h"

#include <stdint.h>
#include <string.h>

#include "support/arena.h"
#include "support/buf.h"
#include "support/diag.h"
#include "support/utf8.h"
#include "support/xid.h"

/* Adversarial probes of the allocator-failure and boundary contracts:
   allocations that must fail, lengths that must not wrap, and value
   sweeps over whole Unicode ranges. Sweep failures report once, not
   per scalar. */

static void test_arena_oversize_guard(void)
{
    ek_arena *a = ek_arena_create();
    char *p;
    int i;

    EK_CHECK(a != NULL);
    EK_CHECK(ek_arena_alloc(a, SIZE_MAX) == NULL);
    EK_CHECK(ek_arena_alloc(a, SIZE_MAX / 2 + 1) == NULL);
    /* the guard boundary itself only fails in the host allocator */
    EK_CHECK(ek_arena_alloc(a, SIZE_MAX / 2) == NULL);

    /* a rejected allocation must leave the arena untouched and usable */
    for (i = 0; i < 64; i++) {
        p = ek_arena_alloc(a, (size_t)(i + 1));
        EK_CHECK(p != NULL);
        memset(p, (unsigned char)i, (size_t)(i + 1));
    }
    ek_arena_destroy(a);
}

static void test_arena_zero_size_slots(void)
{
    enum { SLOTS = 4 };
    ek_arena *a = ek_arena_create();
    void *p[SLOTS];
    int i, j;

    for (i = 0; i < SLOTS; i++) {
        p[i] = ek_arena_alloc(a, 0);
        EK_CHECK(p[i] != NULL);
        EK_CHECK_U64((uintptr_t)p[i] % ek_arena_align(), 0);
        for (j = 0; j < i; j++) {
            EK_CHECK(p[i] != p[j]); /* zero size still hands out slots */
        }
    }
    ek_arena_destroy(a);
}

static void test_arena_interleaved_instances(void)
{
    ek_arena *a = ek_arena_create();
    ek_arena *b = ek_arena_create();
    unsigned char *pa, *pb;
    size_t i;

    EK_CHECK(a != NULL);
    EK_CHECK(b != NULL);
    pa = ek_arena_alloc(a, 4096);
    pb = ek_arena_alloc(b, 4096);
    EK_CHECK(pa != NULL && pb != NULL);
    EK_CHECK(pa != pb);
    memset(pa, 0xAA, 4096);
    memset(pb, 0xBB, 4096);
    ek_arena_destroy(a);
    for (i = 0; i < 4096; i++) {
        /* b's storage is untouched by a's teardown */
        if (pb[i] != 0xBB) {
            EK_CHECK_U64(pb[i], 0xBB);
            ek_arena_destroy(b);
            return;
        }
    }
    EK_CHECK(1);
    EK_CHECK(ek_arena_alloc(b, 16) != NULL);
    ek_arena_destroy(b);
}

static void test_arena_alignment_across_blocks(void)
{
    enum { ROUNDS = 2000, BLOCK_HINT = 4096 }; /* mirrors arena.c's
                                                  default block size */
    ek_arena *a = ek_arena_create();
    unsigned char *cells[ROUNDS];
    size_t align = ek_arena_align();
    size_t total = 0;
    int i;

    for (i = 0; i < ROUNDS; i++) {
        size_t sz = (size_t)(i % 17) + 1;
        cells[i] = ek_arena_alloc(a, sz);
        EK_CHECK(cells[i] != NULL);
        EK_CHECK_U64((uintptr_t)cells[i] % align, 0);
        memset(cells[i], (unsigned char)i, sz);
        total += sz;
    }
    EK_CHECK(total > 2 * BLOCK_HINT); /* several block rollovers happened */
    for (i = 0; i < ROUNDS; i++) {
        size_t sz = (size_t)(i % 17) + 1;
        if (cells[i][sz / 2] != (unsigned char)i) {
            EK_CHECK_U64(cells[i][sz / 2], (unsigned char)i);
            ek_arena_destroy(a);
            return;
        }
    }
    EK_CHECK(1);
    ek_arena_destroy(a);
}

static void test_arena_oversized_interleave(void)
{
    enum { ROUNDS = 50, BIG = 8192, SMALL = 24 };
    ek_arena *a = ek_arena_create();
    unsigned char *cells[ROUNDS][2];
    int i, j;

    for (i = 0; i < ROUNDS; i++) {
        cells[i][0] = ek_arena_alloc(a, BIG); /* each exceeds block size */
        cells[i][1] = ek_arena_alloc(a, SMALL);
        EK_CHECK(cells[i][0] != NULL && cells[i][1] != NULL);
        EK_CHECK_U64((uintptr_t)cells[i][0] % ek_arena_align(), 0);
        memset(cells[i][0], (unsigned char)i, BIG);
        memset(cells[i][1], (unsigned char)(i ^ 0xFF), SMALL);
    }
    for (i = 0; i < ROUNDS; i++) {
        for (j = 0; j < BIG; j += 251) {
            if (cells[i][0][j] != (unsigned char)i) {
                EK_CHECK_U64(cells[i][0][j], (unsigned char)i);
                ek_arena_destroy(a);
                return;
            }
        }
        for (j = 0; j < SMALL; j++) {
            if (cells[i][1][j] != (unsigned char)(i ^ 0xFF)) {
                EK_CHECK_U64(cells[i][1][j], (unsigned char)(i ^ 0xFF));
                ek_arena_destroy(a);
                return;
            }
        }
    }
    EK_CHECK(1);
    ek_arena_destroy(a);
}

static void test_arena_null_contract(void)
{
    ek_arena_destroy(NULL); /* documented no-op */
    EK_CHECK(ek_arena_alloc(NULL, 4) == NULL);
}

static void test_buf_absurd_lengths(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_append_bytes(&b, "skip", SIZE_MAX) == 0);
    EK_CHECK(ek_buf_append_bytes(&b, "skip", SIZE_MAX - 1) == 0);
    EK_CHECK(ek_buf_append_bytes(&b, "skip", SIZE_MAX - 4) == 0);
    EK_CHECK(b.data == NULL); /* nothing ever fit, nothing was read */
    EK_CHECK_U64(b.len, 0);

    EK_CHECK(ek_buf_append_char(&b, 'x') == 1);
    EK_CHECK_STR(b.data, "x");
    ek_buf_destroy(&b);
}

static void test_buf_empty_and_percent_formats(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_printf(&b, "") == 1); /* empty rendering succeeds */
    EK_CHECK(b.data == NULL); /* nothing stored keeps data NULL */
    EK_CHECK_U64(b.len, 0);

    EK_CHECK(ek_buf_printf(&b, "100%%%% %s%%%c", "ok", '%') == 1);
    EK_CHECK_STR(b.data, "100%% ok%%");
    EK_CHECK_U64(b.len, 10);
    ek_buf_destroy(&b);
}

static void test_buf_printf_embedded_nul(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_printf(&b, "a%cb", '\0') == 1);
    EK_CHECK_U64(b.len, 3);
    EK_CHECK(b.data != NULL);
    EK_CHECK_U64((unsigned char)b.data[1], 0); /* %c wrote a NUL byte */
    EK_CHECK_U64((unsigned char)b.data[2], 'b');
    EK_CHECK_U64((unsigned char)b.data[3], 0); /* terminator invariant */
    ek_buf_destroy(&b);
}

static void test_buf_reuse_after_destroy(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_append_str(&b, "first") == 1);
    ek_buf_destroy(&b);
    EK_CHECK(b.data == NULL);
    EK_CHECK_U64(b.len, 0);
    EK_CHECK_U64(b.cap, 0);
    /* destroy reinitializes, so the buffer is usable again; the header
       does not promise this, the test pins what the code does */
    EK_CHECK(ek_buf_append_str(&b, "second") == 1);
    EK_CHECK_STR(b.data, "second");
    ek_buf_destroy(&b);
}

static void test_diag_percent_and_empty(void)
{
    ek_diag *d = ek_diag_create();
    const ek_diagnostic *items;
    size_t count = 0;

    EK_CHECK(d != NULL);
    ek_diag_emit(d, EK_SEV_ERROR, "E-pct", NULL, 0, 0, "100%%%% -> %s", "x");
    ek_diag_emit(d, EK_SEV_WARNING, NULL, NULL, 0, 0, "");
    items = ek_diag_drain(d, &count);
    EK_CHECK_U64(count, 2);
    EK_CHECK_STR(items[0].message, "100%% -> x");
    EK_CHECK(items[0].file == NULL);
    EK_CHECK_STR(items[1].message, "");
    EK_CHECK(items[1].code == NULL);
    EK_CHECK(items[1].file == NULL);
    ek_diag_destroy(d);
}

static void test_diag_very_long_message(void)
{
    ek_diag *d = ek_diag_create();
    const ek_diagnostic *items;
    char big[2001];
    size_t count = 0;
    size_t i;

    for (i = 0; i < 2000; i++) {
        big[i] = (char)('0' + (int)(i % 10));
    }
    big[2000] = '\0';
    ek_diag_emit(d, EK_SEV_ERROR, "E-big", "f.eko", 1, 1, "%s|end", big);
    items = ek_diag_drain(d, &count);
    EK_CHECK_U64(count, 1);
    EK_CHECK_U64(strlen(items[0].message), 2004);
    EK_CHECK(memcmp(items[0].message, big, 2000) == 0);
    EK_CHECK_STR(items[0].message + 2000, "|end");
    ek_diag_destroy(d);
}

static void test_diag_bulk_cycles(void)
{
    enum { N = 500, CYCLES = 4 };
    ek_diag *d = ek_diag_create();
    const ek_diagnostic *items;
    size_t count = 0;
    int c, i;

    for (c = 0; c < CYCLES; c++) {
        for (i = 0; i < N; i++) {
            ek_diag_emit(d, i % 2 ? EK_SEV_WARNING : EK_SEV_ERROR, NULL,
                         "f.eko", i, c, "cycle %d item %d", c, i);
        }
        EK_CHECK_U64(ek_diag_count(d, EK_SEV_ERROR), N / 2);
        EK_CHECK_U64(ek_diag_count(d, EK_SEV_WARNING), N / 2);
        items = ek_diag_drain(d, &count);
        EK_CHECK_U64(count, N);
        for (i = 0; i < N; i++) {
            /* emission order survives storage, growth, and drain */
            if (items[i].line != i || items[i].col != c
                || strlen(items[i].message) == 0) {
                EK_CHECK_U64((size_t)items[i].line, (size_t)i);
                EK_CHECK_U64((size_t)items[i].col, (size_t)c);
                ek_diag_destroy(d);
                return;
            }
        }
        EK_CHECK(1);
    }
    ek_diag_destroy(d);
}

static void test_diag_null_destroy(void)
{
    ek_diag_destroy(NULL); /* documented no-op */
}

static void test_utf8_exhaustive_roundtrip(void)
{
    uint32_t cp;
    int ok = 1;

    for (cp = 0; cp <= 0x10FFFF && ok; cp++) {
        char out[4] = { 0, 0, 0, 0 };
        size_t n;
        ek_utf8_decoded r;

        if (cp >= 0xD800 && cp <= 0xDFFF) {
            ok = ek_utf8_encode(cp, out) == 0;
            continue;
        }
        n = ek_utf8_encode(cp, out);
        ok = n >= 1 && n <= 4;
        if (!ok) {
            break;
        }
        r = ek_utf8_decode(out, n);
        ok = r.status == EK_UTF8_OK && r.cp == cp && r.len == n
             && ek_utf8_lead_len((unsigned char)out[0]) == n
             && ek_utf8_advance(out, n) == n;
        if (ok && n > 1) {
            /* every proper prefix is truncated, never invalid */
            size_t k;
            for (k = 1; k < n && ok; k++) {
                r = ek_utf8_decode(out, k);
                ok = r.status == EK_UTF8_TRUNCATED && r.len == k;
            }
        }
    }
    EK_CHECK(ok);
}

static void test_xid_start_subset_of_continue(void)
{
    uint32_t cp;
    int ok = 1;

    for (cp = 0; cp <= 0x10FFFF && ok; cp++) {
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            continue;
        }
        if (ek_xid_start(cp) && !ek_xid_continue(cp)) {
            ok = 0; /* a start character must also be able to continue */
        }
    }
    EK_CHECK(ok);
}

int main(void)
{
    test_arena_oversize_guard();
    test_arena_zero_size_slots();
    test_arena_interleaved_instances();
    test_arena_alignment_across_blocks();
    test_arena_oversized_interleave();
    test_arena_null_contract();
    test_buf_absurd_lengths();
    test_buf_empty_and_percent_formats();
    test_buf_printf_embedded_nul();
    test_buf_reuse_after_destroy();
    test_diag_percent_and_empty();
    test_diag_very_long_message();
    test_diag_bulk_cycles();
    test_diag_null_destroy();
    test_utf8_exhaustive_roundtrip();
    test_xid_start_subset_of_continue();
    return ek_test_report("support/stress");
}
