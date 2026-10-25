#include "common.h"

/* Conformance smoke test (Annex D.2): all 11 positive modules resolve
   as compilation roots with zero error diagnostics — with exactly one
   known exception.

   KNOWN SUITE BUG, not worked around: result_chains.eko, test
   `wildcard_links`, references `rows`, a name bound only inside test
   `parse_table`. Clause 9.14.6 forbids shared state between tests
   ("Tests share no state"), and test bindings do not enter the module
   surface (Clauses 7.3.2, 9.14.6), so the reference is unbound in
   `wildcard_links` and the resolver reports it. The suite bug is
   reported to the language owner per Annex D ("a test that conflicts
   with a normative clause is an error in the suite"); the test asserts
   the diagnostic precisely (file and line of the offending name).
*/

#define CONF_DIR "docs/tests/conformance"

static const char *modules[] = {
    "access_drilling", "bindings", "case_patterns", "data_services",
    "derived_services", "generics", "geolib", "geolib_use",
    "lambdas_capture", "result_chains", "types_composites"
};

static int resolve_module(const char *stem)
{
    char path[512];
    char name[512];
    char *src;
    const char *mpath[1] = { CONF_DIR };
    rfx f;
    int ok = 1;

    snprintf(path, sizeof path, "%s/%s.eko", CONF_DIR, stem);
    snprintf(name, sizeof name, "%s.eko", stem);
    src = rfx_read_file(path);
    if (!src) {
        fprintf(stderr, "resolve/conformance: cannot read %s\n", path);
        return 0;
    }
    rfx_run(&f, src, mpath, 1, name);
    if (strcmp(stem, "result_chains") == 0) {
        /* The one known diagnostic: the unbound `rows` of the suite
           bug described above (Clause 9.14.6). */
        const ek_diagnostic *d = NULL;
        size_t i;

        EK_CHECK_U64(rfx_errors(&f), 1);
        for (i = 0; i < f.dn; i++) {
            if (f.ds[i].severity == EK_SEV_ERROR) {
                d = &f.ds[i];
            }
        }
        EK_CHECK(d != NULL);
        if (d) {
            EK_CHECK_STR(d->code, "E-unbound-name");
            EK_CHECK_U64((unsigned long long)d->line, 61);
            EK_CHECK_U64((unsigned long long)d->col, 5);
            EK_CHECK(strstr(d->file ? d->file : "", "result_chains.eko")
                     != NULL);
        }
        ok = ek_test_failed == 0;
    } else {
        size_t i;

        if (rfx_errors(&f) != 0) {
            fprintf(stderr, "resolve/conformance: %s.eko has %zu error(s)\n",
                    stem, rfx_errors(&f));
            for (i = 0; i < f.dn; i++) {
                fprintf(stderr, "  %s:%d:%d: %s: %s\n",
                        f.ds[i].file ? f.ds[i].file : "?", f.ds[i].line,
                        f.ds[i].col, f.ds[i].code ? f.ds[i].code : "-",
                        f.ds[i].message ? f.ds[i].message : "");
            }
            ek_test_failed++;
            ok = 0;
        } else {
            ek_test_passed++;
        }
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
