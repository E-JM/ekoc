#include "common.h"

/* Conformance smoke test: all 16 positive modules resolve as
   compilation roots with zero error diagnostics. */

#define CONF_DIR "tests/conformance"

static const char *modules[] = {
    "access_drilling", "bindings", "case_patterns", "ctor_anchor",
    "ctor_anchor_std", "data_services", "derived_services", "generics",
    "geolib", "geolib_use", "lambdas_capture", "namespaces", "ranges", "result_chains",
    "selective_import", "types_composites"
};

static int resolve_module(const char *stem)
{
    char path[512];
    char name[512];
    char *src;
    const char *mpath[1] = { CONF_DIR };
    rfx f;
    int ok;

    snprintf(path, sizeof path, "%s/%s.eko", CONF_DIR, stem);
    snprintf(name, sizeof name, "%s.eko", stem);
    src = rfx_read_file(path);
    if (!src) {
        fprintf(stderr, "resolve/conformance: cannot read %s\n", path);
        return 0;
    }
    rfx_run(&f, src, mpath, 1, name);
    ok = rfx_errors(&f) == 0;
    if (ok) {
        ek_test_passed++;
    } else {
        size_t i;

        fprintf(stderr, "resolve/conformance: %s.eko has %zu error(s)\n",
                stem, rfx_errors(&f));
        for (i = 0; i < f.dn; i++) {
            fprintf(stderr, "  %s:%d:%d: %s: %s\n",
                    f.ds[i].file ? f.ds[i].file : "?", f.ds[i].line,
                    f.ds[i].col, f.ds[i].code ? f.ds[i].code : "-",
                    f.ds[i].message ? f.ds[i].message : "");
        }
        ek_test_failed++;
    }
    rfx_end(&f);
    free(src);
    return ok;
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof modules / sizeof *modules; i++) {
        if (!resolve_module(modules[i])) {
            fprintf(stderr, "resolve/conformance: %s failed\n", modules[i]);
        }
    }
    return ek_test_report("resolve/conformance");
}
