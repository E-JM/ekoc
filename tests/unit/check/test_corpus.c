#include "common.h"
#include "corpus.h"

/* Mutation and truncation corpus over the generics and lambda
   conformance modules: single-word delete, duplicate, and swap
   mutations, systematic plus seeded, and byte-prefix truncations.
   Every run goes through the full pipeline and must terminate, keep
   the row table sorted, fill every world bit, and stay within a
   bounded, well-formed diagnostic count. Termination and memory
   safety are additionally enforced by the test-runner timeout and the
   sanitized build. */

#define CONF_DIR "tests/conformance"

#define CORPUS_SEEDED 250
#define CORPUS_MAX_DIAG 5000

/* One run: the invariants that must hold for every input, clean or
   erroneous. */
static int run_one(const char *src, size_t index)
{
    cfx f;
    size_t i;
    int bad = 0;

    cfx_run(&f, src, NULL, 0, "corpus.eko");
    if (!cfx_rows_sorted(&f)) {
        fprintf(stderr, "check/corpus: mutation %zu rows unsorted\n", index);
        bad = 1;
    }
    if (!cfx_bits_filled(&f)) {
        fprintf(stderr, "check/corpus: mutation %zu has an unset bit\n",
                index);
        bad = 1;
    }
    if (f.cdn > CORPUS_MAX_DIAG) {
        fprintf(stderr, "check/corpus: mutation %zu emitted %zu "
                "diagnostics\n", index, f.cdn);
        bad = 1;
    }
    for (i = 0; i < f.cdn; i++) {
        if (!f.cds[i].message || (f.cds[i].code && !f.cds[i].code[0])) {
            fprintf(stderr, "check/corpus: mutation %zu diagnostic %zu is "
                    "malformed\n", index, i);
            bad = 1;
            break;
        }
    }
    cfx_end(&f);
    return bad;
}

/* The whole corpus over one conformance module: systematic delete,
   duplicate, and swap mutations plus prefix truncations, then the
   seeded sweep. */
static size_t corpus_over(const char *path, const char *label, int *ok)
{
    char *src = cfx_read_file(path);
    words_t w;
    size_t mutations = 0, failures = 0;

    *ok = 0;
    if (!src) {
        fprintf(stderr, "check/corpus: cannot read %s\n", path);
        return 0;
    }
    if (!words_split(src, &w)) {
        fprintf(stderr, "check/corpus: %s does not split\n", label);
        free(src);
        return 0;
    }
    printf("check/corpus: %s: %zu words\n", label, w.n);
    corpus_systematic(&w, "check/corpus", label, run_one, 1, &mutations,
                      &failures);
    corpus_seeded(&w, "check/corpus", label, run_one, CORPUS_SEEDED,
                  &mutations, &failures);
    printf("check/corpus: %s: %zu mutations, %zu failures\n", label,
           mutations, failures);
    words_free(&w);
    free(src);
    *ok = 1;
    return failures;
}

int main(void)
{
    int ok1, ok2;
    size_t bad1 = corpus_over(CONF_DIR "/generics.eko", "generics", &ok1);
    size_t bad2 = corpus_over(CONF_DIR "/lambdas_capture.eko",
                              "lambdas_capture", &ok2);

    EK_CHECK_U64(ok1 && !bad1 ? 0 : 1, 0);
    EK_CHECK_U64(ok2 && !bad2 ? 0 : 1, 0);
    return ek_test_report("check/corpus");
}
