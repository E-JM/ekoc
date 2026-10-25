#ifndef EK_TESTS_RESOLVE_COMMON_H
#define EK_TESTS_RESOLVE_COMMON_H

/* Shared fixtures for the resolve unit suites: the full front half
   (lex, layout, parse) followed by ek_resolve over in-memory sources,
   plus reference lookups by source position. File-backed fixtures
   read from disk so imports resolve for real. */

#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    const ek_diagnostic *ds;
    size_t dn;
} rfx;

/* malloc'd file content; the caller frees it. */
static inline char *rfx_read_file(const char *path)
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

static inline void rfx_run(rfx *f, const char *src, const char *const *path,
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
}

static inline void rfx_begin(rfx *f, const char *src)
{
    rfx_run(f, src, NULL, 0, "test.eko");
}

static inline void rfx_end(rfx *f)
{
    ek_diag_destroy(f->diag);
    ek_arena_destroy(f->arena);
}

/* Counts errors in the drained diagnostics; the buffer itself is
   empty after rfx_run drains it. */
static inline size_t rfx_errors(const rfx *f)
{
    size_t i, n = 0;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].severity == EK_SEV_ERROR) {
            n++;
        }
    }
    return n;
}

static inline int rfx_has_code(const rfx *f, const char *code)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].code && strcmp(f->ds[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static inline int rfx_code_at_line(const rfx *f, const char *code, int line)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].code && strcmp(f->ds[i].code, code) == 0
            && f->ds[i].line == line) {
            return 1;
        }
    }
    return 0;
}

/* The first reference recorded at a source line; NULL when none. */
static inline const ek_ref *rfx_ref_at(const rfx *f, int line)
{
    size_t i;

    for (i = 0; i < f->res.ref_count; i++) {
        if (f->res.refs[i].node->line == line) {
            return &f->res.refs[i];
        }
    }
    return NULL;
}

/* The first reference at a line whose symbol carries the name;
   several references can share one line. */
static inline const ek_ref *rfx_ref_named(const rfx *f, int line,
                                          const char *name)
{
    size_t i;

    for (i = 0; i < f->res.ref_count; i++) {
        const ek_ref *r = &f->res.refs[i];

        if (r->node->line == line && r->symbol
            && strcmp(r->symbol->name, name) == 0) {
            return r;
        }
    }
    return NULL;
}

/* The first reference at a line matching name and kind. */
static inline const ek_ref *rfx_ref_named_kind(const rfx *f, int line,
                                               const char *name,
                                               ek_sym_kind kind)
{
    size_t i;

    for (i = 0; i < f->res.ref_count; i++) {
        const ek_ref *r = &f->res.refs[i];

        if (r->node->line == line && r->kind == kind && r->symbol
            && strcmp(r->symbol->name, name) == 0) {
            return r;
        }
    }
    return NULL;
}

/* A receiver-form link carries no symbol, so it is found by station. */
static inline int rfx_has_member_ref(const rfx *f, int line, ek_station st)
{
    size_t i;

    for (i = 0; i < f->res.ref_count; i++) {
        if (f->res.refs[i].node->line == line
            && f->res.refs[i].kind == EK_SYM_MEMBER
            && f->res.refs[i].station == st) {
            return 1;
        }
    }
    return 0;
}

static inline const ek_symbol *rfx_surface_sym(const rfx *f, const char *mod,
                                               const char *name)
{
    size_t i, k;

    for (k = 0; k < f->res.module_count; k++) {
        if (strcmp(f->res.modules[k].module, mod) != 0) {
            continue;
        }
        for (i = 0; i < f->res.modules[k].count; i++) {
            if (strcmp(f->res.modules[k].syms[i].name, name) == 0) {
                return &f->res.modules[k].syms[i];
            }
        }
    }
    return NULL;
}

/* Writes one file under dir, creating dir; returns 0 on failure. */
static inline int rfx_put(const char *dir, const char *name,
                          const char *content)
{
    char path[512];
    FILE *fp;
    int ok;

    snprintf(path, sizeof path, "%s/%s", dir, name);
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    ok = fwrite(content, 1, strlen(content), fp) == strlen(content);
    fclose(fp);
    return ok;
}

#endif
