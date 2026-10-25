#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* Smoke test: every conformance module (Annex D sources, currently kept
   under docs/tests/conformance) must lex without a single error
   diagnostic. */

static const char *const modules[] = {
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

static void lex_module(const char *path)
{
    size_t len = 0;
    char *data = read_file(path, &len);
    ek_arena *arena;
    ek_diag *diag;
    ek_lex_result r;

    EK_CHECK(data != NULL);
    if (!data) {
        return;
    }
    arena = ek_arena_create();
    diag = ek_diag_create();
    r = ek_lex(data, len, path, arena, diag);
    EK_CHECK_U64(ek_diag_count(diag, EK_SEV_ERROR), 0);
    EK_CHECK_U64(ek_diag_count(diag, EK_SEV_WARNING), 0);
    EK_CHECK(r.count > 0);
    EK_CHECK_U64(r.tokens[r.count - 1].kind, EK_TOK_EOF);
    printf("%s: %llu tokens\n", path,
           (unsigned long long)(r.count > 0 ? r.count - 1 : 0));
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
    free(data);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof modules / sizeof modules[0]; i++) {
        lex_module(modules[i]);
    }
    return ek_test_report("lex/conformance");
}
