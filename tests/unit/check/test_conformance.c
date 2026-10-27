#include "common.h"

/* Conformance smoke test through check: all 13 positive modules run
   the full pipeline (lex, layout, parse, resolve, check) as
   compilation roots.

   PASS CONDITION: no error diagnostics from any stage. resolve.h
   documents the std surface: the core types are real source, and the
   remaining placeholder rows keep external opacity for the services
   the compiler does not type yet, so a rejected conformance module is
   an internal defect. */

#define CONF_DIR "tests/conformance"

static const char *modules[] = {
    "access_drilling", "bindings", "case_patterns", "ctor_anchor",
    "ctor_anchor_std", "data_services", "derived_services", "generics",
    "geolib", "geolib_use", "lambdas_capture", "result_chains",
    "selective_import", "types_composites"
};

static int check_module(const char *stem)
{
    char path[512];
    char name[512];
    char *src;
    const char *mpath[1] = {CONF_DIR};
    cfx f;
    int ok;

    snprintf(path, sizeof path, "%s/%s.eko", CONF_DIR, stem);
    snprintf(name, sizeof name, "%s.eko", stem);
    src = cfx_read_file(path);
    if (!src) {
        fprintf(stderr, "check/conformance: cannot read %s\n", path);
        return 0;
    }
    cfx_run(&f, src, mpath, 1, name);

    /* The result table is sorted by node address, and every row
       carries a defined world bit. */
    EK_CHECK(cfx_rows_sorted(&f));
    EK_CHECK(cfx_bits_filled(&f));

    ok = cfx_stage_errors(f.ds, f.dn) == 0 && cfx_errors(&f) == 0;
    if (ok) {
        ek_test_passed++;
    } else {
        size_t i;

        fprintf(stderr, "check/conformance: %s.eko rejected\n", stem);
        for (i = 0; i < f.dn; i++) {
            fprintf(stderr, "  resolve %s:%d:%d: %s: %s\n",
                    f.ds[i].file ? f.ds[i].file : "?", f.ds[i].line,
                    f.ds[i].col, f.ds[i].code ? f.ds[i].code : "-",
                    f.ds[i].message ? f.ds[i].message : "");
        }
        for (i = 0; i < f.cdn; i++) {
            fprintf(stderr, "  check %s:%d:%d: %s: %s\n",
                    f.cds[i].file ? f.cds[i].file : "?", f.cds[i].line,
                    f.cds[i].col, f.cds[i].code ? f.cds[i].code : "-",
                    f.cds[i].message ? f.cds[i].message : "");
        }
        ek_test_failed++;
    }
    cfx_end(&f);
    free(src);
    return ok;
}

/* The positive bit flows of lambdas_capture.eko: the pure factory's
   own row stays pure while its closure row and the call of the closure
   are world-touching. */
static void check_lambdas_bits(void)
{
    char path[512], name[512], *src;
    static const char *mpath[1] = {CONF_DIR};
    cfx f;

    snprintf(path, sizeof path, "%s/lambdas_capture.eko", CONF_DIR);
    snprintf(name, sizeof name, "%s", "lambdas_capture.eko");
    src = cfx_read_file(path);
    if (!src) {
        fprintf(stderr, "check/conformance: cannot read %s\n", path);
        EK_CHECK(0);
        return;
    }
    cfx_run(&f, src, mpath, 1, name);
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bit_at(&f, 5, EK_BIT_PURE));  /* inc */
    EK_CHECK(cfx_bit_at(&f, 7, EK_BIT_PURE));  /* mk_logger's own arrow */
    EK_CHECK(cfx_bit_at(&f, 8, EK_BIT_WORLD)); /* the `!` closure */
    EK_CHECK(cfx_bit_at(&f, 20, EK_BIT_WORLD)); /* log(3) */
    cfx_end(&f);
    free(src);
}

int main(void)
{
    size_t i;

    for (i = 0; i < sizeof modules / sizeof *modules; i++) {
        if (!check_module(modules[i])) {
            fprintf(stderr, "check/conformance: %s failed\n", modules[i]);
        }
    }
    check_lambdas_bits();
    return ek_test_report("check/conformance");
}
