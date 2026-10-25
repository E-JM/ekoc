#include "harness.h"

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
    ek_buf_append_str(&b, "he");
    ek_buf_append_char(&b, 'y');
    ek_buf_append_str(&b, "");
    ek_buf_append_char(&b, '!');
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
    ek_buf_append_bytes(&b, raw, sizeof raw);
    EK_CHECK_U64(b.len, 4);
    EK_CHECK(memcmp(b.data, raw, sizeof raw) == 0);
    EK_CHECK_U64((unsigned char)b.data[4], 0);

    /* a terminator-looking byte must not reset the buffer */
    ek_buf_append_str(&b, "tail");
    EK_CHECK_U64(b.len, 8);
    EK_CHECK(memcmp(b.data, raw, sizeof raw) == 0);
    EK_CHECK_STR(b.data + 4, "tail");
    ek_buf_destroy(&b);
}

static void test_growth_keeps_content(void)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < 1000; i++) {
        ek_buf_append_str(&b, "0123456789");
        ek_buf_append_char(&b, (char)('A' + i % 26));
    }
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
    ek_buf_printf(&b, "%s=%d", "n", 42);
    ek_buf_printf(&b, " %c%.2f%%", 'o', 99.5);
    EK_CHECK_STR(b.data, "n=42 o99.50%");
    EK_CHECK_U64(b.len, strlen("n=42 o99.50%"));

    /* long output forces reallocation inside vprintf */
    ek_buf_printf(&b, " %0500d", 7);
    EK_CHECK_U64(b.len, strlen("n=42 o99.50%") + 1 + 500);
    EK_CHECK_U64((unsigned char)b.data[13], '0');
    EK_CHECK_STR(b.data + b.len - 2, "07");
    ek_buf_destroy(&b);
}

int main(void)
{
    test_init_empty();
    test_append_char_and_str();
    test_append_bytes_with_embedded_nul();
    test_growth_keeps_content();
    test_printf_append();
    return ek_test_report("support/buf");
}
