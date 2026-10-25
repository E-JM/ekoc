#include "harness.h"

#include <string.h>

#include "support/diag.h"

static void test_emit_and_count(void)
{
    ek_diag *d = ek_diag_create();

    EK_CHECK(d != NULL);
    EK_CHECK_U64(ek_diag_count(d, EK_SEV_ERROR), 0);
    EK_CHECK_U64(ek_diag_count(d, EK_SEV_WARNING), 0);

    ek_diag_emit(d, EK_SEV_ERROR, "E-test", "f.eko", 1, 1, "bad thing %d", 42);
    ek_diag_emit(d, EK_SEV_WARNING, NULL, "f.eko", 2, 5, "watch out");
    ek_diag_emit(d, EK_SEV_ERROR, "E-other", "g.eko", 10, 3, "%s x %d", "chars", 7);

    EK_CHECK_U64(ek_diag_count(d, EK_SEV_ERROR), 2);
    EK_CHECK_U64(ek_diag_count(d, EK_SEV_WARNING), 1);
    ek_diag_destroy(d);
}

static void test_drain_order_and_content(void)
{
    ek_diag *d = ek_diag_create();
    const ek_diagnostic *items;
    size_t count = 0;

    ek_diag_emit(d, EK_SEV_ERROR, "E-one", "a.eko", 1, 2, "first %s", "msg");
    ek_diag_emit(d, EK_SEV_WARNING, NULL, "b.eko", 30, 40, "second");
    ek_diag_emit(d, EK_SEV_ERROR, "E-three", "c.eko", 100, 0, "third");

    items = ek_diag_drain(d, &count);
    EK_CHECK(items != NULL);
    EK_CHECK_U64(count, 3);

    EK_CHECK_U64(items[0].severity, EK_SEV_ERROR);
    EK_CHECK_STR(items[0].code, "E-one");
    EK_CHECK_STR(items[0].message, "first msg");
    EK_CHECK_STR(items[0].file, "a.eko");
    EK_CHECK_U64(items[0].line, 1);
    EK_CHECK_U64(items[0].col, 2);

    EK_CHECK_U64(items[1].severity, EK_SEV_WARNING);
    EK_CHECK(items[1].code == NULL);
    EK_CHECK_STR(items[1].message, "second");
    EK_CHECK_STR(items[1].file, "b.eko");
    EK_CHECK_U64(items[1].line, 30);
    EK_CHECK_U64(items[1].col, 40);

    EK_CHECK_U64(items[2].severity, EK_SEV_ERROR);
    EK_CHECK_STR(items[2].code, "E-three");
    EK_CHECK_STR(items[2].message, "third");
    EK_CHECK_STR(items[2].file, "c.eko");
    EK_CHECK_U64(items[2].line, 100);
    EK_CHECK_U64(items[2].col, 0);

    EK_CHECK_U64(ek_diag_count(d, EK_SEV_ERROR), 0);
    EK_CHECK_U64(ek_diag_count(d, EK_SEV_WARNING), 0);
    ek_diag_destroy(d);
}

static void test_drain_allows_reuse(void)
{
    ek_diag *d = ek_diag_create();
    const ek_diagnostic *items;
    size_t count = 0;

    ek_diag_emit(d, EK_SEV_ERROR, "E-a", "x.eko", 1, 1, "one");
    items = ek_diag_drain(d, &count);
    EK_CHECK_U64(count, 1);
    EK_CHECK_STR(items[0].message, "one");

    /* emission after a drain reuses the buffer; drained text stays valid */
    ek_diag_emit(d, EK_SEV_WARNING, NULL, "y.eko", 9, 9, "two");
    items = ek_diag_drain(d, &count);
    EK_CHECK_U64(count, 1);
    EK_CHECK_U64(items[0].severity, EK_SEV_WARNING);
    EK_CHECK_STR(items[0].message, "two");

    /* drain with a NULL count argument is allowed */
    ek_diag_emit(d, EK_SEV_ERROR, NULL, NULL, 0, 0, "three");
    items = ek_diag_drain(d, NULL);
    EK_CHECK_STR(items[0].message, "three");
    EK_CHECK(items[0].file == NULL);
    ek_diag_destroy(d);
}

static void test_long_formatted_message(void)
{
    ek_diag *d = ek_diag_create();
    const ek_diagnostic *items;
    size_t count = 0;

    /* long enough to grow the formatting buffer past its first block */
    ek_diag_emit(d, EK_SEV_ERROR, "E-long", "z.eko", 7, 8,
                 "%s-%s-%s-%s-%s-%s width %d height %d depth %d",
                 "aaaaaaaaaa", "bbbbbbbbbb", "cccccccccc", "dddddddddd",
                 "eeeeeeeeee", "ffffffffff", 11, 22, 33);
    items = ek_diag_drain(d, &count);
    EK_CHECK_U64(count, 1);
    EK_CHECK_STR(items[0].message,
                 "aaaaaaaaaa-bbbbbbbbbb-cccccccccc-dddddddddd-eeeeeeeeee"
                 "-ffffffffff width 11 height 22 depth 33");
    ek_diag_destroy(d);
}

int main(void)
{
    test_emit_and_count();
    test_drain_order_and_content();
    test_drain_allows_reuse();
    test_long_formatted_message();
    return ek_test_report("support/diag");
}
