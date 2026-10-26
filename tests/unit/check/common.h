#ifndef EK_TESTS_CHECK_COMMON_H
#define EK_TESTS_CHECK_COMMON_H

/* Shared fixtures for the check unit suites: the full pipeline (lex,
   layout, parse, resolve) followed by ek_check over in-memory
   sources. The resolve and check diagnostics come back drained
   separately, so a suite can tell the stages apart. File-backed
   fixtures read the conformance directory for imports. */

#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check/check.h"
#include "layout/layout.h"
#include "lex/lex.h"
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
    const ek_diagnostic *ds; /* resolve drain */
    size_t dn;
    const ek_diagnostic *cds; /* check drain */
    size_t cdn;
} cfx;

/* malloc'd file content; the caller frees it. */
static inline char *cfx_read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long sz;
    char *buf;

    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 0
        || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[sz] = 0;
    fclose(fp);
    return buf;
}

static inline void cfx_run(cfx *f, const char *src, const char *const *path,
                           size_t npath, const char *name)
{
    memset(f, 0, sizeof *f);
    f->arena = ek_arena_create();
    f->diag = ek_diag_create();
    f->lex = ek_lex(src, strlen(src), name, f->arena, f->diag);
    f->lay = ek_layout(f->lex.tokens, f->lex.count, name, f->arena, f->diag);
    f->ast = ek_parse(f->lay.tokens, f->lay.count, name, f->arena, f->diag);
    f->res = ek_resolve(f->ast.module, name, path, npath, f->arena, f->diag);
    f->ds = ek_diag_drain(f->diag, &f->dn);
    f->chk = ek_check(f->res, f->arena, f->diag);
    f->cds = ek_diag_drain(f->diag, &f->cdn);
}

static inline void cfx_begin(cfx *f, const char *src)
{
    cfx_run(f, src, NULL, 0, "test.eko");
}

static inline void cfx_end(cfx *f)
{
    ek_diag_destroy(f->diag);
    ek_arena_destroy(f->arena);
}

/* Error diagnostics of one stage in the drained slices. */
static inline size_t cfx_stage_errors(const ek_diagnostic *ds, size_t dn)
{
    size_t i, n = 0;

    for (i = 0; i < dn; i++) {
        if (ds[i].severity == EK_SEV_ERROR) {
            n++;
        }
    }
    return n;
}

static inline size_t cfx_errors(const cfx *f)
{
    return cfx_stage_errors(f->cds, f->cdn);
}

static inline int cfx_has_code(const cfx *f, const char *code)
{
    size_t i;

    for (i = 0; i < f->cdn; i++) {
        if (f->cds[i].code && strcmp(f->cds[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static inline int cfx_code_at_line(const cfx *f, const char *code, int line)
{
    size_t i;

    for (i = 0; i < f->cdn; i++) {
        if (f->cds[i].code && strcmp(f->cds[i].code, code) == 0
            && f->cds[i].line == line) {
            return 1;
        }
    }
    return 0;
}

/* The first check row whose node sits on a source line. */
static inline const ek_ck_row *cfx_row_at_line(const cfx *f, int line)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line) {
            return &f->chk.rows[i];
        }
    }
    return NULL;
}

/* Any row on the line carrying one reading. */
static inline int cfx_reading_at(const cfx *f, int line, ek_reading r)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line
            && f->chk.rows[i].reading == r) {
            return 1;
        }
    }
    return 0;
}

/* Any row on the line carrying one world bit (Clause 7.9). */
static inline int cfx_bit_at(const cfx *f, int line, ek_bit b)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line
            && f->chk.rows[i].bit == b) {
            return 1;
        }
    }
    return 0;
}

/* Every row carries a defined bit: EK_BIT_UNSET survives only on a
   failed run. */
static inline int cfx_bits_filled(const cfx *f)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        if (f->chk.rows[i].bit == EK_BIT_UNSET) {
            return 0;
        }
    }
    return 1;
}

/* The rows are sorted by node address. */
static inline int cfx_rows_sorted(const cfx *f)
{
    size_t i;

    for (i = 1; i < f->chk.count; i++) {
        if ((size_t)f->chk.rows[i - 1].node > (size_t)f->chk.rows[i].node) {
            return 0;
        }
    }
    return 1;
}

#endif
