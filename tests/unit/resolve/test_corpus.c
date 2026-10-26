#include "common.h"
#include "refcheck.h"

/* Mutation corpus over the geolib conformance pair: single-word
   delete, duplicate, and swap mutations, systematic and seeded. Every
   run must terminate, keep the refs table sorted and exactly covering
   the reference sites, keep every diagnostic well-formed, and stay
   within a bounded diagnostic count. Termination and memory safety
   are additionally enforced by the test-runner timeout and the
   sanitized build. */

#define CONF_DIR "docs/tests/conformance"

/* Number of seeded random mutations added to the systematic sweep. */
#define CORPUS_SEEDED 200
#define CORPUS_SEED 20261026u
#define CORPUS_MAX_DIAG 10000

typedef struct {
    char **words;
    size_t n;
} words_t;

static void words_free(words_t *w)
{
    size_t i;

    for (i = 0; i < w->n; i++) {
        free(w->words[i]);
    }
    free(w->words);
    w->words = NULL;
    w->n = 0;
}

/* Splits on whitespace: word boundaries are where a mutation cannot
   tear a token in half. */
static int words_split(const char *src, words_t *w)
{
    size_t i = 0, cap = 0;

    memset(w, 0, sizeof *w);
    for (;;) {
        size_t begin, end;
        char *copy;

        while (src[i] == ' ' || src[i] == '\t' || src[i] == '\n'
               || src[i] == '\r') {
            i++;
        }
        if (!src[i]) {
            return 1;
        }
        begin = i;
        while (src[i] && src[i] != ' ' && src[i] != '\t' && src[i] != '\n'
               && src[i] != '\r') {
            i++;
        }
        end = i;
        if (w->n == cap) {
            char **grown;

            cap = cap ? cap * 2 : 64;
            grown = realloc(w->words, cap * sizeof *grown);
            if (!grown) {
                words_free(w);
                return 0;
            }
            w->words = grown;
        }
        copy = malloc(end - begin + 1);
        if (!copy) {
            words_free(w);
            return 0;
        }
        memcpy(copy, src + begin, end - begin);
        copy[end - begin] = 0;
        w->words[w->n++] = copy;
    }
}

static char *words_render(const words_t *w, size_t skip, size_t dup,
                          size_t swap_a, size_t swap_b)
{
    size_t len = 1, i, at = 0;
    char *out;

    for (i = 0; i < w->n; i++) {
        len += strlen(w->words[i]) + 2;
    }
    if (dup < w->n) {
        len += strlen(w->words[dup]) + 1;
    }
    out = malloc(len);
    if (!out) {
        return NULL;
    }
    for (i = 0; i < w->n; i++) {
        const char *word = w->words[i];

        if (i == swap_a && swap_b < w->n) {
            word = w->words[swap_b];
        } else if (i == swap_b && swap_a < w->n) {
            word = w->words[swap_a];
        }
        if (i != skip) {
            if (at) {
                out[at++] = ' ';
            }
            memcpy(out + at, word, strlen(word));
            at += strlen(word);
        }
        if (i == dup) {
            out[at++] = ' ';
            memcpy(out + at, word, strlen(word));
            at += strlen(word);
        }
    }
    out[at] = 0;
    return out;
}

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

static size_t corpus_over(const char *path, const char *label, int *ok)
{
    char *src = rfx_read_file(path);
    words_t w;
    size_t i, mutations = 0, failures = 0;
    unsigned rng = CORPUS_SEED;

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
    for (i = 0; i < w.n; i++) {
        static const size_t none = (size_t)-1;
        struct {
            size_t skip, dup, swap_a, swap_b;
        } variants[3];
        size_t v;

        variants[0].skip = i;
        variants[0].dup = none;
        variants[0].swap_a = none;
        variants[0].swap_b = none;
        variants[1].skip = none;
        variants[1].dup = i;
        variants[1].swap_a = none;
        variants[1].swap_b = none;
        variants[2].skip = none;
        variants[2].dup = none;
        variants[2].swap_a = i;
        variants[2].swap_b = i + 1 < w.n ? i + 1 : none;
        for (v = 0; v < 3; v++) {
            char *mut = words_render(&w, variants[v].skip, variants[v].dup,
                                     variants[v].swap_a,
                                     variants[v].swap_b);

            if (mut) {
                if (run_one(mut, mutations) && failures < 5) {
                    fprintf(stderr, "resolve/corpus: %s mutation at word "
                            "%zu variant %zu\n", label, i, v);
                    failures++;
                }
                free(mut);
                mutations++;
            }
        }
    }
    for (i = 0; i < CORPUS_SEEDED; i++) {
        static const size_t none = (size_t)-1;
        size_t skip = none, dup = none, swap_a = none, swap_b = none;
        size_t a, b, op;
        char *mut;

        rng = rng * 1103515245u + 12345u;
        op = ((rng >> 16) % 3);
        rng = rng * 1103515245u + 12345u;
        a = (rng >> 16) % w.n;
        if (op == 0) {
            skip = a;
        } else if (op == 1) {
            dup = a;
        } else {
            rng = rng * 1103515245u + 12345u;
            b = a + (rng >> 16) % (w.n - a);
            swap_a = a;
            swap_b = b;
        }
        mut = words_render(&w, skip, dup, swap_a, swap_b);
        if (mut) {
            if (run_one(mut, mutations) && failures < 5) {
                fprintf(stderr, "resolve/corpus: %s seeded mutation %zu "
                        "at word %zu\n", label, i, a);
                failures++;
            }
            free(mut);
            mutations++;
        }
    }
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
