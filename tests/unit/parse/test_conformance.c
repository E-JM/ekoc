#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* Smoke test: every conformance module under tests/conformance must
   lex, lay out, and parse to completion with zero error diagnostics. */

static const char *const modules[] = {
    "tests/conformance/access_drilling.eko",
    "tests/conformance/bindings.eko",
    "tests/conformance/case_patterns.eko",
    "tests/conformance/ctor_anchor.eko",
    "tests/conformance/ctor_anchor_std.eko",
    "tests/conformance/data_services.eko",
    "tests/conformance/derived_services.eko",
    "tests/conformance/generics.eko",
    "tests/conformance/geolib.eko",
    "tests/conformance/geolib_use.eko",
    "tests/conformance/lambdas_capture.eko",
    "tests/conformance/result_chains.eko",
    "tests/conformance/selective_import.eko",
    "tests/conformance/types_composites.eko",
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

static void parse_module(const char *path)
{
    size_t len = 0;
    char *data = read_file(path, &len);
    parse_fixture f;
    const ek_diagnostic *ds;
    size_t dn = 0, i;

    EK_CHECK(data != NULL);
    if (!data) {
        return;
    }
    pfx_begin(&f, data);
    EK_CHECK_U64(ek_diag_count(f.diag, EK_SEV_ERROR), 0);
    EK_CHECK_U64(ek_diag_count(f.diag, EK_SEV_WARNING), 0);
    EK_CHECK(f.ast.module != NULL);
    EK_CHECK(f.ast.module->u.kids.count > 0);
    ds = ek_diag_drain(f.diag, &dn);
    for (i = 0; i < dn; i++) {
        fprintf(stderr, "%s:%d:%d: %s: %s\n", path, ds[i].line, ds[i].col,
                ds[i].code ? ds[i].code : "-", ds[i].message);
    }
    printf("%s: %llu items\n", path,
           (unsigned long long)f.ast.module->u.kids.count);
    pfx_end(&f);
    free(data);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof modules / sizeof modules[0]; i++) {
        parse_module(modules[i]);
    }
    return ek_test_report("parse/conformance");
}
