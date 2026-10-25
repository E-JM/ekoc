#ifndef EK_TESTS_LEX_COMMON_H
#define EK_TESTS_LEX_COMMON_H

/* Shared fixtures for the lex unit suites. */

#include "harness.h"

#include <string.h>

#include "lex/lex.h"
#include "support/arena.h"
#include "support/diag.h"

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    ek_lex_result r;
} lex_fixture;

static inline void fx_begin(lex_fixture *f, const char *src)
{
    f->arena = ek_arena_create();
    f->diag = ek_diag_create();
    f->r = ek_lex(src, strlen(src), "test.eko", f->arena, f->diag);
}

static inline void fx_end(lex_fixture *f)
{
    ek_diag_destroy(f->diag);
    ek_arena_destroy(f->arena);
}

static inline size_t fx_errors(const lex_fixture *f)
{
    return ek_diag_count(f->diag, EK_SEV_ERROR);
}

/* Drains the buffer; call after all other checks on the fixture. */
static inline int fx_has_error(const lex_fixture *f, const char *code)
{
    size_t n = 0, i;
    const ek_diagnostic *ds = ek_diag_drain(f->diag, &n);
    int found = 0;

    for (i = 0; i < n; i++) {
        if (ds[i].code && strcmp(ds[i].code, code) == 0) {
            found = 1;
        }
    }
    return found;
}

static inline ek_token fx_tok(const lex_fixture *f, size_t i)
{
    return f->r.tokens[i];
}

static inline size_t fx_count(const lex_fixture *f)
{
    return f->r.count;
}

/* Asserts the whole kind sequence of a source, then that it is clean. */
static inline void fx_kinds(const char *src, const ek_token_kind *want,
                            size_t n)
{
    lex_fixture f;
    size_t i;

    fx_begin(&f, src);
    EK_CHECK_U64(fx_count(&f), n);
    for (i = 0; i < n && i < fx_count(&f); i++) {
        if (fx_tok(&f, i).kind != want[i]) {
            EK_CHECK_U64(fx_tok(&f, i).kind, want[i]);
        }
    }
    if (n > 0) {
        EK_CHECK_U64(fx_tok(&f, n - 1).kind, EK_TOK_EOF);
    }
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

#define FX_KINDS(src, ...)                                                \
    do {                                                                  \
        const ek_token_kind want_[] = {__VA_ARGS__};                      \
        fx_kinds(src, want_, sizeof want_ / sizeof want_[0]);             \
    } while (0)

#endif
