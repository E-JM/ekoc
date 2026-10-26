#ifndef EK_TESTS_LAYOUT_COMMON_H
#define EK_TESTS_LAYOUT_COMMON_H

/* Shared fixtures for the layout unit suites: ek_lex then ek_layout
   over in-memory sources. */

#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex/lex.h"
#include "layout/layout.h"
#include "support/arena.h"
#include "support/diag.h"

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    ek_lex_result lex;
    ek_layout_result r;
} layout_fixture;

static inline void fx_begin(layout_fixture *f, const char *src)
{
    f->arena = ek_arena_create();
    f->diag = ek_diag_create();
    f->lex = ek_lex(src, strlen(src), "test.eko", f->arena, f->diag);
    f->r = ek_layout(f->lex.tokens, f->lex.count, "test.eko", f->arena,
                     f->diag);
}

static inline void fx_end(layout_fixture *f)
{
    ek_diag_destroy(f->diag);
    ek_arena_destroy(f->arena);
}

static inline size_t fx_errors(const layout_fixture *f)
{
    return ek_diag_count(f->diag, EK_SEV_ERROR);
}

/* Counts emitted diagnostics carrying code. Drains the buffer: call
   after every fx_errors check on the fixture. */
static inline size_t fx_count_code(layout_fixture *f, const char *code)
{
    size_t n = 0, i, hits = 0;
    const ek_diagnostic *ds = ek_diag_drain(f->diag, &n);

    for (i = 0; i < n; i++) {
        if (ds[i].code && strcmp(ds[i].code, code) == 0) {
            hits++;
        }
    }
    return hits;
}

static inline size_t fx_count(const layout_fixture *f)
{
    return f->r.count;
}

static inline ek_layout_token fx_tok(const layout_fixture *f, size_t i)
{
    return f->r.tokens[i];
}

static inline size_t fx_count_kind(const layout_fixture *f, ek_layout_kind kind)
{
    size_t i, hits = 0;

    for (i = 0; i < f->r.count; i++) {
        if (f->r.tokens[i].kind == kind) {
            hits++;
        }
    }
    return hits;
}

/* Renders the kind skeleton: S source, N newline, I indent, D dedent,
   E eof, space-separated. The caller frees; NULL when out of memory. */
static inline char *fx_kinds(const ek_layout_result *r)
{
    size_t i, pos = 0;
    char *s = malloc(r->count * 3 + 1);

    if (!s) {
        return NULL;
    }
    for (i = 0; i < r->count; i++) {
        const char *k =
            r->tokens[i].kind == EK_LTOK_SOURCE ? "S" :
            r->tokens[i].kind == EK_LTOK_NEWLINE ? "N" :
            r->tokens[i].kind == EK_LTOK_INDENT ? "I" :
            r->tokens[i].kind == EK_LTOK_DEDENT ? "D" : "E";
        if (i > 0) {
            s[pos++] = ' ';
        }
        s[pos++] = k[0];
    }
    s[pos] = '\0';
    return s;
}

static inline char *fx_skeleton(const layout_fixture *f)
{
    return fx_kinds(&f->r);
}

/* Runs one source through lex and layout and asserts its skeleton. */
static inline void fx_skeleton_check(const char *src, const char *want)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, src);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, want);
    free(got);
    fx_end(&f);
}

/* The conformance modules (Annex D sources), relative to the repository
   root, where make test runs the suite binaries. */
static inline const char *const *fx_modules(size_t *count)
{
    static const char *const mods[] = {
        "docs/tests/conformance/access_drilling.eko",
        "docs/tests/conformance/bindings.eko",
        "docs/tests/conformance/case_patterns.eko",
        "docs/tests/conformance/data_services.eko",
        "docs/tests/conformance/derived_services.eko",
        "docs/tests/conformance/generics.eko",
        "docs/tests/conformance/geolib.eko",
        "docs/tests/conformance/geolib_use.eko",
        "docs/tests/conformance/lambdas_capture.eko",
        "docs/tests/conformance/result_chains.eko",
        "docs/tests/conformance/types_composites.eko",
    };

    *count = sizeof mods / sizeof mods[0];
    return mods;
}

/* Reads one file whole, NUL-terminated for the lexer. The caller
   frees; NULL when the file cannot be read or sized. */
static inline char *fx_read_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    char *data;
    long size;

    if (!fp) {
        fprintf(stderr, "%s: cannot open\n", path);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0
        || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        fprintf(stderr, "%s: cannot size\n", path);
        return NULL;
    }
    data = malloc((size_t)size + 1);
    if (!data) {
        fclose(fp);
        return NULL;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        fprintf(stderr, "%s: cannot read\n", path);
        return NULL;
    }
    fclose(fp);
    data[size] = '\0';
    *len = (size_t)size;
    return data;
}

#endif
