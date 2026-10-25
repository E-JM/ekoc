#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* Deeply nested blocks: six header levels, each opening exactly one
   block, all closed by end of file (Clause 5.1.3, A.4 rule 10). */

static char *build_source(void)
{
    static const char *const headers[] = {
        "a: Int =", "b: Int =", "c: Int =",
        "d: Int =", "e: Int =", "f: Int =",
    };
    size_t i, pos = 0, cap = 512;
    char *s = malloc(cap);

    if (!s) {
        return NULL;
    }
    for (i = 0; i < 6; i++) {
        int space;
        int indent = 4 * (int)i;

        for (space = 0; space < indent; space++) {
            s[pos++] = ' ';
        }
        pos += (size_t)sprintf(s + pos, "%s\n", headers[i]);
    }
    for (i = 0; i < 24; i++) {
        s[pos++] = ' ';
    }
    pos += (size_t)sprintf(s + pos, "0\n");
    s[pos] = '\0';
    return s;
}

static void test_six_levels(void)
{
    char *src = build_source();
    layout_fixture f;
    char *got;
    char want[512];
    int i;
    size_t pos = 0;

    EK_CHECK(src != NULL);
    if (!src) {
        return;
    }
    fx_begin(&f, src);
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 6);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_DEDENT), 6);
    for (i = 0; i < 6; i++) {
        pos += (size_t)sprintf(want + pos, "S S S S N I ");
    }
    pos += (size_t)sprintf(want + pos, "S N ");
    for (i = 0; i < 6; i++) {
        pos += (size_t)sprintf(want + pos, "D ");
    }
    sprintf(want + pos, "E");
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, want);
    free(got);
    fx_end(&f);
    free(src);
}

int main(void)
{
    test_six_levels();
    return ek_test_report("layout/deep");
}
