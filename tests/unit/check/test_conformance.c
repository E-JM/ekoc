#include "common.h"

/* Conformance smoke test (Annex D.2) through check: all 11 positive
   modules run the full pipeline (lex, layout, parse, resolve, check)
   as compilation roots.

   PASS CONDITION (the M6a ruling): no error diagnostics from check in
   any module, and no diagnostics at all except the one known suite
   bug. resolve.h documents the placeholder std surface; by the bridge
   ruling external opacity cannot force a diagnostic, so an internal
   fact rejecting a conformance module is a finding to report, never
   an assertion to weaken.

   KNOWN SUITE BUG (see tests/unit/resolve/test_conformance.c):
   result_chains.eko references `rows` outside the test that binds it;
   resolve reports exactly one E-unbound-name at 61:5. */

#define CONF_DIR "docs/tests/conformance"

static const char *modules[] = {
    "access_drilling", "bindings", "case_patterns", "data_services",
    "derived_services", "generics", "geolib", "geolib_use",
    "lambdas_capture", "result_chains", "types_composites"
};

static int check_module(const char *stem)
{
    char path[512];
    char name[512];
    char *src;
    const char *mpath[1] = {CONF_DIR};
    cfx f;
    int ok = 1;

    snprintf(path, sizeof path, "%s/%s.eko", CONF_DIR, stem);
    snprintf(name, sizeof name, "%s.eko", stem);
    src = cfx_read_file(path);
    if (!src) {
        fprintf(stderr, "check/conformance: cannot read %s\n", path);
        return 0;
    }
    cfx_run(&f, src, mpath, 1, name);

    /* The result table is sorted by node address; every row's bit
       slot stays empty until M6b fills it. */
    EK_CHECK(cfx_rows_sorted(&f));
    {
        size_t i;

        for (i = 0; i < f.chk.count; i++) {
            if (f.chk.rows[i].bit != EK_BIT_UNSET) {
                EK_CHECK(0);
                ok = 0;
            }
        }
    }

    if (strcmp(stem, "result_chains") == 0) {
        size_t i;
        size_t res_errors = cfx_stage_errors(f.ds, f.dn);

        /* The one suite bug, diagnosed by resolve alone. */
        EK_CHECK_U64(res_errors, 1);
        EK_CHECK_U64(cfx_errors(&f), 0);
        for (i = 0; i < f.dn; i++) {
            if (f.ds[i].severity == EK_SEV_ERROR) {
                EK_CHECK_STR(f.ds[i].code, "E-unbound-name");
                EK_CHECK_U64((unsigned long long)f.ds[i].line, 61);
            }
        }
    } else {
        if (cfx_stage_errors(f.ds, f.dn) != 0 || cfx_errors(&f) != 0) {
            fprintf(stderr, "check/conformance: %s.eko rejected\n", stem);
            {
                size_t i;

                for (i = 0; i < f.dn; i++) {
                    fprintf(stderr, "  resolve %s:%d:%d: %s: %s\n",
                            f.ds[i].file ? f.ds[i].file : "?", f.ds[i].line,
                            f.ds[i].col, f.ds[i].code ? f.ds[i].code : "-",
                            f.ds[i].message ? f.ds[i].message : "");
                }
                for (i = 0; i < f.cdn; i++) {
                    fprintf(stderr, "  check %s:%d:%d: %s: %s\n",
                            f.cds[i].file ? f.cds[i].file : "?",
                            f.cds[i].line, f.cds[i].col,
                            f.cds[i].code ? f.cds[i].code : "-",
                            f.cds[i].message ? f.cds[i].message : "");
                }
            }
            ok = 0;
        }
        if (ok) {
            ek_test_passed++;
        } else {
            ek_test_failed++;
        }
    }
    cfx_end(&f);
    free(src);
    return ok;
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof modules / sizeof *modules; i++) {
        if (!check_module(modules[i])) {
            fprintf(stderr, "check/conformance: %s failed\n", modules[i]);
        }
    }
    return ek_test_report("check/conformance");
}
