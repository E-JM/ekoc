#ifndef EK_TESTS_LOWER_COMMON_H
#define EK_TESTS_LOWER_COMMON_H

/* Shared fixtures for the lower unit suites: the full pipeline (lex,
   layout, parse, resolve, check) followed by ek_lower over in-memory
   sources. The resolve, check, and lower diagnostics come back drained
   separately, so a suite can tell the stages apart. */

#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check/check.h"
#include "layout/layout.h"
#include "lex/lex.h"
#include "lower/lower.h"
#include "parse/parse.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/diag.h"

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    ek_lex_result lex;
    ek_layout_result lay;
    ek_parse_result ast;
    ek_resolve_result res;
    ek_check_result chk;
    ek_lower_result low;
    ek_diagnostic *ds;        /* malloc'd snapshot of the resolve drain */
    size_t dn;
    ek_diagnostic *cds;       /* malloc'd snapshot of the check drain */
    size_t cdn;
    const ek_diagnostic *lds; /* lower drain */
    size_t ldn;
} lfx;

static inline void lfx_run_mode(lfx *f, const char *src, const char *name,
                                ek_lower_mode mode)
{
    const ek_diagnostic *drain;
    size_t n;

    memset(f, 0, sizeof *f);
    f->arena = ek_arena_create();
    f->diag = ek_diag_create();
    f->lex = ek_lex(src, strlen(src), name, f->arena, f->diag);
    f->lay = ek_layout(f->lex.tokens, f->lex.count, name, f->arena, f->diag);
    f->ast = ek_parse(f->lay.tokens, f->lay.count, name, f->arena, f->diag);
    f->res = ek_resolve(f->ast.module, name, NULL, 0, f->arena, f->diag);
    /* The drain hands out the live buffer; each later stage drains the
       same one, so the resolve slice is snapshotted first. */
    drain = ek_diag_drain(f->diag, &n);
    if (n) {
        f->ds = malloc(n * sizeof *f->ds);
        if (f->ds) {
            memcpy(f->ds, drain, n * sizeof *f->ds);
            f->dn = n;
        }
    }
    f->chk = ek_check(f->res, f->arena, f->diag);
    /* the check slice is snapshotted the way the resolve slice is: the
       lower stage drains and reuses the same buffer */
    drain = ek_diag_drain(f->diag, &f->cdn);
    if (f->cdn) {
        f->cds = malloc(f->cdn * sizeof *f->cds);
        if (f->cds) {
            memcpy(f->cds, drain, f->cdn * sizeof *f->cds);
        }
    }
    f->low = ek_lower_with(f->res, f->chk, mode, f->arena, f->diag);
    f->lds = ek_diag_drain(f->diag, &f->ldn);
}

static inline void lfx_run(lfx *f, const char *src, const char *name)
{
    lfx_run_mode(f, src, name, EK_LOWER_PROGRAM);
}

static inline void lfx_begin(lfx *f, const char *src)
{
    lfx_run(f, src, "test.eko");
}

/* The module lowered in TESTS mode: the test bindings are the roots. */
static inline void lfx_begin_tests(lfx *f, const char *src)
{
    lfx_run_mode(f, src, "test.eko", EK_LOWER_TESTS);
}

static inline void lfx_end(lfx *f)
{
    free(f->ds);
    free(f->cds);
    ek_diag_destroy(f->diag);
    ek_arena_destroy(f->arena);
}

/* The check stage found nothing to complain about. */
static inline int lfx_check_clean(const lfx *f)
{
    return f->cdn == 0;
}

/* The lower stage found nothing to complain about. */
static inline int lfx_lower_clean(const lfx *f)
{
    return f->ldn == 0;
}

/* Error diagnostics of the lower drain. */
static inline size_t lfx_errors(const lfx *f)
{
    size_t i, n = 0;

    for (i = 0; i < f->ldn; i++) {
        if (f->lds[i].severity == EK_SEV_ERROR) {
            n++;
        }
    }
    return n;
}

static inline int lfx_has_code(const lfx *f, const char *code)
{
    size_t i;

    for (i = 0; i < f->ldn; i++) {
        if (f->lds[i].code && strcmp(f->lds[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static inline int lfx_code_at_line(const lfx *f, const char *code, int line)
{
    size_t i;

    for (i = 0; i < f->ldn; i++) {
        if (f->lds[i].code && strcmp(f->lds[i].code, code) == 0
            && f->lds[i].line == line) {
            return 1;
        }
    }
    return 0;
}

/* Fragments of the emitted unit. */
static inline int lfx_has(const lfx *f, const char *fragment)
{
    return f->low.text && strstr(f->low.text, fragment) != NULL;
}

static inline int lfx_lacks(const lfx *f, const char *fragment)
{
    return f->low.text && strstr(f->low.text, fragment) == NULL;
}

#endif
