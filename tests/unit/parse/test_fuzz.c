#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Black-box robustness over the conformance corpus: every byte prefix
   of every conformance module, and seeded single-token mutations of
   each module, must run the full pipeline to termination, return a
   MODULE root, and stay inside a finite diagnostic budget. */

static const char *const modules[] = {
    "tests/conformance/access_drilling.eko",
    "tests/conformance/bindings.eko",
    "tests/conformance/case_patterns.eko",
    "tests/conformance/data_services.eko",
    "tests/conformance/derived_services.eko",
    "tests/conformance/generics.eko",
    "tests/conformance/geolib.eko",
    "tests/conformance/geolib_use.eko",
    "tests/conformance/lambdas_capture.eko",
    "tests/conformance/result_chains.eko",
    "tests/conformance/types_composites.eko",
};

#define MUTATIONS_PER_MODULE 300

static char *read_file(const char *path, size_t *len)
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

/* Deterministic xorshift64; the seed fixes the mutation set. */
static unsigned long long rng;
static unsigned long long rng_next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}

/* Each error suppresses only against the previous position, but every
   emitting loop also consumes tokens, so the count stays linear in the
   stream plus the bytes that interpolation slots hide. */
static int diags_finite(const parse_fixture *f, size_t tokens, size_t bytes)
{
    return ek_diag_count(f->diag, EK_SEV_ERROR) <= 4 * (tokens + bytes) + 64;
}

static void check_root(const parse_fixture *f, size_t tokens, size_t bytes)
{
    EK_CHECK(f->ast.module != NULL);
    if (f->ast.module) {
        EK_CHECK_U64(f->ast.module->kind, EK_N_MODULE);
    }
    EK_CHECK(diags_finite(f, tokens, bytes));
}

static void truncations(const char *path)
{
    size_t len = 0;
    char *data = read_file(path, &len);
    char *scratch;
    size_t i;

    EK_CHECK(data != NULL);
    if (!data) {
        return;
    }
    scratch = malloc(len + 1);
    EK_CHECK(scratch != NULL);
    if (!scratch) {
        free(data);
        return;
    }
    for (i = 0; i <= len; i++) {
        parse_fixture f;

        memcpy(scratch, data, i);
        scratch[i] = '\0';
        pfx_begin(&f, scratch);
        check_root(&f, f.lay.count, i);
        pfx_end(&f);
    }
    free(scratch);
    free(data);
    printf("%s: %llu prefixes\n", path, (unsigned long long)(len + 1));
}

/* One seed per module keeps a failure reproducible from the report. */
static void mutations(const char *path, size_t seed)
{
    size_t len = 0;
    char *data = read_file(path, &len);
    ek_arena *lex_arena;
    ek_diag *lex_diag;
    ek_lex_result lex;
    ek_token *work;
    size_t m, source_len;

    EK_CHECK(data != NULL);
    if (!data) {
        return;
    }
    lex_arena = ek_arena_create();
    lex_diag = ek_diag_create();
    lex = ek_lex(data, len, path, lex_arena, lex_diag);
    work = lex.tokens ? malloc((lex.count + 1) * sizeof(ek_token)) : NULL;
    EK_CHECK(lex.tokens != NULL);
    EK_CHECK(work != NULL);
    if (!work) {
        ek_diag_destroy(lex_diag);
        ek_arena_destroy(lex_arena);
        free(data);
        return;
    }
    rng = 0x9E3779B97F4A7C15ull ^ (seed + 1) * 2685821657736338717ull;
    source_len = lex.count > 0 ? lex.count - 1 : 0;
    for (m = 0; m < MUTATIONS_PER_MODULE && source_len > 0; m++) {
        parse_fixture f;
        ek_layout_result lay;
        size_t kind = (size_t)(rng_next() % 3);
        size_t count;

        memcpy(work, lex.tokens, lex.count * sizeof(ek_token));
        if (kind == 0) {
            size_t i = (size_t)(rng_next() % source_len);

            memmove(&work[i], &work[i + 1],
                    (lex.count - 1 - i) * sizeof(ek_token));
            count = lex.count - 1;
        } else if (kind == 1) {
            size_t i = (size_t)(rng_next() % source_len);
            ek_token copy = work[i];

            work[lex.count] = work[lex.count - 1];
            work[lex.count - 1] = copy;
            count = lex.count + 1;
        } else {
            size_t i = (size_t)(rng_next() % source_len);
            size_t j = (size_t)(rng_next() % source_len);
            ek_token tmp = work[i];

            work[i] = work[j];
            work[j] = tmp;
            count = lex.count;
        }
        memset(&f, 0, sizeof f);
        f.arena = ek_arena_create();
        f.diag = ek_diag_create();
        lay = ek_layout(work, count, path, f.arena, f.diag);
        EK_CHECK(lay.tokens != NULL);
        if (lay.tokens) {
            f.ast = ek_parse(lay.tokens, lay.count, path, f.arena, f.diag);
            check_root(&f, lay.count, len);
        }
        pfx_end(&f);
    }
    free(work);
    ek_diag_destroy(lex_diag);
    ek_arena_destroy(lex_arena);
    free(data);
    printf("%s: %d mutations\n", path, MUTATIONS_PER_MODULE);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof modules / sizeof modules[0]; i++) {
        truncations(modules[i]);
        mutations(modules[i], i);
    }
    return ek_test_report("parse/fuzz");
}
