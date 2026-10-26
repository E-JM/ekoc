#include "harness.h"

#include <stdint.h>
#include <string.h>

#include "support/buf.h"

static void test_init_empty(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(b.data == NULL);
    EK_CHECK_U64(b.len, 0);
    EK_CHECK_U64(b.cap, 0);
    ek_buf_destroy(&b);
    EK_CHECK(b.data == NULL);
}

static void test_append_char_and_str(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_append_str(&b, "he") == 1);
    EK_CHECK(ek_buf_append_char(&b, 'y') == 1);
    EK_CHECK(ek_buf_append_str(&b, "") == 1);
    EK_CHECK(ek_buf_append_char(&b, '!') == 1);
    EK_CHECK_U64(b.len, 4);
    EK_CHECK_STR(b.data, "hey!");
    EK_CHECK_U64((unsigned char)b.data[b.len], 0); /* terminator invariant */
    ek_buf_destroy(&b);
}

static void test_append_bytes_with_embedded_nul(void)
{
    ek_buf b;
    const char raw[] = { 'a', '\0', 'b', '\0' };

    ek_buf_init(&b);
    EK_CHECK(ek_buf_append_bytes(&b, raw, sizeof raw) == 1);
    EK_CHECK_U64(b.len, 4);
    EK_CHECK(memcmp(b.data, raw, sizeof raw) == 0);
    EK_CHECK_U64((unsigned char)b.data[4], 0);

    /* a terminator-looking byte must not reset the buffer */
    EK_CHECK(ek_buf_append_str(&b, "tail") == 1);
    EK_CHECK_U64(b.len, 8);
    EK_CHECK(memcmp(b.data, raw, sizeof raw) == 0);
    EK_CHECK_STR(b.data + 4, "tail");
    ek_buf_destroy(&b);
}

static void test_growth_keeps_content(void)
{
    ek_buf b;
    int ok = 1;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < 1000; i++) {
        ok = ok && ek_buf_append_str(&b, "0123456789") != 0;
        ok = ok && ek_buf_append_char(&b, (char)('A' + i % 26)) != 0;
    }
    EK_CHECK(ok);
    EK_CHECK_U64(b.len, 11000);
    EK_CHECK_U64(b.cap >= b.len + 1, 1);
    for (i = 0; i < 1000; i++) {
        if (memcmp(b.data + i * 11, "0123456789", 10) != 0
            || b.data[i * 11 + 10] != (char)('A' + i % 26)) {
            EK_CHECK(0);
            ek_buf_destroy(&b);
            return;
        }
    }
    EK_CHECK(1);
    EK_CHECK_U64((unsigned char)b.data[b.len], 0);
    ek_buf_destroy(&b);
}

static void test_printf_append(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_printf(&b, "%s=%d", "n", 42) == 1);
    EK_CHECK(ek_buf_printf(&b, " %c%.2f%%", 'o', 99.5) == 1);
    EK_CHECK_STR(b.data, "n=42 o99.50%");
    EK_CHECK_U64(b.len, strlen("n=42 o99.50%"));

    /* long output forces reallocation inside vprintf */
    EK_CHECK(ek_buf_printf(&b, " %0500d", 7) == 1);
    EK_CHECK_U64(b.len, strlen("n=42 o99.50%") + 1 + 500);
    EK_CHECK_U64((unsigned char)b.data[13], '0');
    EK_CHECK_STR(b.data + b.len - 2, "07");
    ek_buf_destroy(&b);
}

/* The failure contract: a 0 return stores nothing and leaves the
   buffer with its previous content, still usable. Host allocation
   failure cannot be forced from outside the module; the size-overflow
   guard drives the same failure returns, so this pins the contract
   shape, not the allocator. */
static void test_append_failure_keeps_content(void)
{
    ek_buf b;

    ek_buf_init(&b);
    EK_CHECK(ek_buf_append_str(&b, "keep") == 1);
    EK_CHECK(ek_buf_append_bytes(&b, "x", SIZE_MAX) == 0);
    EK_CHECK(ek_buf_append_bytes(&b, "x", SIZE_MAX - 1) == 0);
    EK_CHECK_U64(b.len, 4);
    EK_CHECK_STR(b.data, "keep");

    /* a failed append on a fresh buffer allocates nothing */
    ek_buf_destroy(&b);
    ek_buf_init(&b);
    EK_CHECK(ek_buf_append_bytes(&b, "x", SIZE_MAX) == 0);
    EK_CHECK(b.data == NULL);
    EK_CHECK_U64(b.len, 0);
    EK_CHECK(ek_buf_append_char(&b, 'x') == 1);
    EK_CHECK_STR(b.data, "x");
    ek_buf_destroy(&b);
}

int main(void)
{
    test_init_empty();
    test_append_char_and_str();
    test_append_bytes_with_embedded_nul();
    test_growth_keeps_content();
    test_printf_append();
    test_append_failure_keeps_content();
    return ek_test_report("support/buf");
}
