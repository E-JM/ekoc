#include "common.h"
#include "corpus.h"
#include "refcheck.h"

/* Mutation corpus over the geolib conformance pair: single-word
   delete, duplicate, and swap mutations, systematic and seeded. Every
   run must terminate, keep the refs table sorted and exactly covering
   the reference sites, keep every diagnostic well-formed, and stay
   within a bounded diagnostic count. Termination and memory safety
   are additionally enforced by the test-runner timeout and the
   sanitized build. */

#define CONF_DIR "tests/conformance"

/* Number of seeded random mutations added to the systematic sweep. */
#define CORPUS_SEEDED 200
#define CORPUS_MAX_DIAG 10000

/* One run: resolve the mutated source and check the invariants that
   must hold for every input, clean or erroneous. */
static int run_one(const char *src, size_t index)
{
    rfx f;
    const char *mpath[1] = { CONF_DIR };
    size_t i;
    int bad;

    rfx_run(&f, src, mpath, 1, "corpus.eko");
    bad = rfx_check_refs(&f);
    if (f.dn > CORPUS_MAX_DIAG) {
        fprintf(stderr, "resolve/corpus: mutation %zu emitted %zu "
                "diagnostics\n", index, f.dn);
        bad = 1;
    }
    for (i = 0; i < f.dn; i++) {
        if (!f.ds[i].message || (f.ds[i].code && !f.ds[i].code[0])) {
            fprintf(stderr, "resolve/corpus: mutation %zu diagnostic %zu "
                    "is malformed\n", index, i);
            bad = 1;
            break;
        }
    }
    for (i = 0; i < f.res.module_count; i++) {
        size_t k;

        if (!f.res.modules[i].module || !f.res.modules[i].file) {
            fprintf(stderr, "resolve/corpus: mutation %zu module %zu has "
                    "no name\n", index, i);
            bad = 1;
        }
        for (k = 0; k < f.res.modules[i].count; k++) {
            if (!f.res.modules[i].syms[k].name) {
                fprintf(stderr, "resolve/corpus: mutation %zu symbol %zu "
                        "of module %zu has no name\n", index, k, i);
                bad = 1;
                break;
            }
        }
    }
    rfx_end(&f);
    return bad;
}

/* The whole corpus over one conformance module: systematic delete,
   duplicate, and swap mutations, then the seeded sweep. */
static size_t corpus_over(const char *path, const char *label, int *ok)
{
    char *src = rfx_read_file(path);
    words_t w;
    size_t mutations = 0, failures = 0;

    *ok = 0;
    if (!src) {
        fprintf(stderr, "resolve/corpus: cannot read %s\n", path);
        return 0;
    }
    if (!words_split(src, &w)) {
        fprintf(stderr, "resolve/corpus: %s does not split\n", label);
        free(src);
        return 0;
    }
    printf("resolve/corpus: %s: %zu words\n", label, w.n);
    corpus_systematic(&w, "resolve/corpus", label, run_one, 0, &mutations,
                      &failures);
    corpus_seeded(&w, "resolve/corpus", label, run_one, CORPUS_SEEDED,
                  &mutations, &failures);
    printf("resolve/corpus: %s: %zu mutations, %zu failures\n", label,
           mutations, failures);
    words_free(&w);
    free(src);
    *ok = 1;
    return failures;
}

int main(void)
{
    int ok1, ok2;
    size_t bad1 = corpus_over(CONF_DIR "/geolib_use.eko", "geolib_use",
                              &ok1);
    size_t bad2 = corpus_over(CONF_DIR "/geolib.eko", "geolib", &ok2);

    EK_CHECK_U64(ok1 && !bad1 ? 0 : 1, 0);
    EK_CHECK_U64(ok2 && !bad2 ? 0 : 1, 0);
    return ek_test_report("resolve/corpus");
}
