#ifndef EK_TESTS_HARNESS_H
#define EK_TESTS_HARNESS_H

/* Minimal test harness: counters live per translation unit, one suite
   per unit. A suite binary exits nonzero when any check fails. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ek_test_passed;
static int ek_test_failed;

#define EK_CHECK(cond)                                                  \
    do {                                                                \
        if (cond) {                                                     \
            ek_test_passed++;                                           \
        } else {                                                        \
            ek_test_failed++;                                           \
            fprintf(stderr, "%s:%d: check failed: %s\n",                \
                    __FILE__, __LINE__, #cond);                         \
        }                                                               \
    } while (0)

#define EK_CHECK_U64(actual, expected)                                  \
    do {                                                                \
        unsigned long long ek_got_ = (unsigned long long)(actual);      \
        unsigned long long ek_want_ = (unsigned long long)(expected);   \
        if (ek_got_ == ek_want_) {                                      \
            ek_test_passed++;                                           \
        } else {                                                        \
            ek_test_failed++;                                           \
            fprintf(stderr, "%s:%d: check failed: %s == %s\n"           \
                    "    got %llu, want %llu\n",                        \
                    __FILE__, __LINE__, #actual, #expected,             \
                    ek_got_, ek_want_);                                 \
        }                                                               \
    } while (0)

#define EK_CHECK_STR(actual, expected)                                  \
    do {                                                                \
        const char *ek_got_ = (actual);                                 \
        const char *ek_want_ = (expected);                              \
        if (ek_got_ != NULL && ek_want_ != NULL                         \
            && strcmp(ek_got_, ek_want_) == 0) {                        \
            ek_test_passed++;                                           \
        } else {                                                        \
            ek_test_failed++;                                           \
            fprintf(stderr, "%s:%d: check failed: %s == \"%s\"\n"       \
                    "    got \"%s\"\n",                                 \
                    __FILE__, __LINE__, #actual,                        \
                    ek_want_ != NULL ? ek_want_ : "(null)",             \
                    ek_got_ != NULL ? ek_got_ : "(null)");              \
        }                                                               \
    } while (0)

/* Prints the suite summary and returns a process exit status. */
static int ek_test_report(const char *suite)
{
    printf("%s: %d passed, %d failed\n", suite, ek_test_passed, ek_test_failed);
    return ek_test_failed == 0 ? 0 : 1;
}

#endif
