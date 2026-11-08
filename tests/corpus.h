#ifndef EK_TESTS_CORPUS_H
#define EK_TESTS_CORPUS_H

/* Shared machinery for the mutation-corpus suites: a whitespace word
   split, single-word mutations and prefix truncations, and the
   systematic and seeded sweeps over one source. The caller supplies
   the per-run invariant check and owns the failure bookkeeping. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Seed of the seeded sweep; the RNG stream it draws is fixed. */
#define CORPUS_SEED 20261026u

typedef struct {
    char **words;
    size_t n;
} words_t;

static inline void words_free(words_t *w)
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
static inline int words_split(const char *src, words_t *w)
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

/* A fresh malloc'd rendering each call: skip one word, duplicate one,
   or swap two; none means no such edit. */
static inline char *words_render(const words_t *w, size_t skip, size_t dup,
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

/* The first keep words, space-joined: a truncation. */
static inline char *words_prefix(const words_t *w, size_t keep)
{
    size_t len = 1, i, at = 0;
    char *out;

    for (i = 0; i < keep && i < w->n; i++) {
        len += strlen(w->words[i]) + 2;
    }
    out = malloc(len);
    if (!out) {
        return NULL;
    }
    for (i = 0; i < keep && i < w->n; i++) {
        if (at) {
            out[at++] = ' ';
        }
        memcpy(out + at, w->words[i], strlen(w->words[i]));
        at += strlen(w->words[i]);
    }
    out[at] = 0;
    return out;
}

/* One run's invariant check, as installed by the suite. */
typedef int (*corpus_run)(const char *src, size_t index);

/* The systematic sweep: per word, delete it, duplicate it, and swap it
   with its neighbor; every fifth word also truncates after it when
   truncations is set. */
static inline void corpus_systematic(const words_t *w, const char *who,
                                     const char *label, corpus_run run,
                                     int truncations, size_t *mutations,
                                     size_t *failures)
{
    static const size_t none = (size_t)-1;
    size_t i;

    for (i = 0; i < w->n; i++) {
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
        variants[2].swap_b = i + 1 < w->n ? i + 1 : none;
        for (v = 0; v < 3; v++) {
            char *mut = words_render(w, variants[v].skip, variants[v].dup,
                                     variants[v].swap_a,
                                     variants[v].swap_b);

            if (mut) {
                if (run(mut, *mutations) && *failures < 5) {
                    fprintf(stderr, "%s: %s mutation at word %zu variant "
                            "%zu\n", who, label, i, v);
                    (*failures)++;
                }
                free(mut);
                (*mutations)++;
            }
        }
        if (truncations && i % 5 == 0) {
            char *cut = words_prefix(w, i + 1);

            if (cut) {
                if (run(cut, *mutations) && *failures < 5) {
                    fprintf(stderr, "%s: %s truncation after word %zu\n",
                            who, label, i);
                    (*failures)++;
                }
                free(cut);
                (*mutations)++;
            }
        }
    }
}

/* The seeded sweep: count draws of the fixed RNG stream, each a
   delete, a duplicate, or a swap. */
static inline void corpus_seeded(const words_t *w, const char *who,
                                 const char *label, corpus_run run,
                                 size_t count, size_t *mutations,
                                 size_t *failures)
{
    static const size_t none = (size_t)-1;
    unsigned rng = CORPUS_SEED;
    size_t i;

    for (i = 0; i < count; i++) {
        size_t skip = none, dup = none, swap_a = none, swap_b = none;
        size_t a, b, op;
        char *mut;

        rng = rng * 1103515245u + 12345u;
        op = (rng >> 16) % 3;
        rng = rng * 1103515245u + 12345u;
        a = (rng >> 16) % w->n;
        if (op == 0) {
            skip = a;
        } else if (op == 1) {
            dup = a;
        } else {
            rng = rng * 1103515245u + 12345u;
            b = a + (rng >> 16) % (w->n - a);
            swap_a = a;
            swap_b = b;
        }
        mut = words_render(w, skip, dup, swap_a, swap_b);
        if (mut) {
            if (run(mut, *mutations) && *failures < 5) {
                fprintf(stderr, "%s: %s seeded mutation %zu at word %zu\n",
                        who, label, i, a);
                (*failures)++;
            }
            free(mut);
            (*mutations)++;
        }
    }
}

#endif
