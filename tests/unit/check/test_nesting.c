#include "common.h"

/* The nesting-depth guarantee: a conforming implementation MUST accept
   at least 64 levels of type nesting, and a construct past the ceiling
   is rejected with a diagnostic rather than a crash. The guard's own
   ceiling is checked at its boundary. */

static char *nested_type(size_t levels)
{
    char *t = malloc(levels * 2 + 4);
    size_t i;

    if (!t) {
        return NULL;
    }
    for (i = 0; i < levels; i++) {
        t[i * 2] = '[';
        t[i * 2 + 1] = ']';
    }
    strcpy(t + levels * 2, "Int");
    return t;
}

static void accepts_the_guaranteed_depth(void)
{
    cfx f;
    char *type = nested_type(64);
    char src[512];

    if (!type) {
        return;
    }
    snprintf(src, sizeof src, "deep: (x: %s) -> Int =\n    0\n", type);
    cfx_begin(&f, src);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
    free(type);
}

/* A construct past the parser's ceiling is the named rejection, not a
   crash: the guard diagnoses once and the frames unwind. */
static void rejects_past_the_ceiling(void)
{
    cfx f;
    char *type = nested_type(1001);
    char *src;

    if (!type) {
        return;
    }
    src = malloc(strlen(type) + 64);
    if (!src) {
        free(type);
        return;
    }
    sprintf(src, "deep: (x: %s) -> Int =\n    0\n", type);
    cfx_begin(&f, src);
    EK_CHECK(cfx_slice_has(f.ds, f.dn, "E-nesting-depth"));
    cfx_end(&f);
    free(src);
    free(type);
}

int main(void)
{
    accepts_the_guaranteed_depth();
    rejects_past_the_ceiling();
    return ek_test_report("check/nesting");
}
