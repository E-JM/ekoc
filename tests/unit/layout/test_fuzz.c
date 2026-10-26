#include "common.h"

#include <stdio.h>
#include <stdlib.h>

/* Property probes over hostile inputs. Every truncation of every
   conformance module is lexed (errors allowed) and laid out, and
   pseudo-random plausible token streams go straight to ek_layout.
   Invariants, always: the stream is nonempty and ends with exactly one
   EOF, a nonempty stream starts with a source token, and every INDENT
   directly follows a NEWLINE. On a run with no error diagnostic,
   INDENT and DEDENT also balance, with no DEDENT before its INDENT.
   The imbalance of diagnosed runs is the recovery of dedent_to, which
   pushes a level it never opened. An INDENT directly followed by a
   DEDENT is legal: a memberless callback block whose deeper line is
   the call's closing bracket, so no adjacency is checked between
   them. */

static unsigned long long fuzz_cases;
static unsigned long long fuzz_failures;
static unsigned long long fuzz_reported;
static unsigned long long fuzz_truncations;

/* Bitmask of violated invariants; bit meanings as in the checks
   below. Reports the first ten failures with their case ids. */
static void check_stream(const ek_layout_result *r, size_t errors,
                         unsigned long long id, const char *what)
{
    unsigned mask = 0;
    size_t i, eofs = 0, indents = 0, dedents = 0;
    long long open = 0;
    int prefix_ok = 1;

    fuzz_cases++;
    if (r->count < 1) {
        mask |= 1u;
    }
    for (i = 0; i < r->count; i++) {
        ek_layout_kind k = r->tokens[i].kind;

        if (k == EK_LTOK_EOF) {
            eofs++;
        } else if (k == EK_LTOK_INDENT) {
            if (i == 0 || r->tokens[i - 1].kind != EK_LTOK_NEWLINE) {
                mask |= 8u;
            }
            indents++;
            open++;
        } else if (k == EK_LTOK_DEDENT) {
            dedents++;
            open--;
            if (open < 0) {
                prefix_ok = 0;
            }
        }
    }
    if (eofs != 1 || (r->count > 0
                      && r->tokens[r->count - 1].kind != EK_LTOK_EOF)) {
        mask |= 2u;
    }
    /* A stream with any content starts with a source token; only an
       input with no significant line at all is the lone EOF. */
    if (r->count > 1 && r->tokens[0].kind != EK_LTOK_SOURCE) {
        mask |= 4u;
    }
    if (errors == 0
        && (indents != dedents || !prefix_ok)) {
        mask |= 32u;
    }
    if (mask != 0) {
        char *kinds = fx_kinds(r);

        fuzz_failures++;
        if (fuzz_reported < 10) {
            fprintf(stderr, "fuzz %s case %llu: invariants mask %u"
                    " (tokens %llu, errors %llu) kinds %s\n", what, id,
                    mask, (unsigned long long)r->count,
                    (unsigned long long)errors,
                    kinds != NULL ? kinds : "(oom)");
            fuzz_reported++;
        }
        free(kinds);
    }
}

static unsigned long long rng_state = 0x9e3779b97f4a7c15ull;

static unsigned long long rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* One plausible line-shaped token run: up to four tokens per line at a
   random indent, NEWLINE per line, blank lines included, sometimes no
   trailing NEWLINE, closed by EOF. */
static size_t random_stream(ek_token *t, size_t cap)
{
    static const ek_token_kind pool[] = {
        EK_TOK_IDENT,   EK_TOK_IDENT,   EK_TOK_INT,      EK_TOK_FLOAT,
        EK_TOK_KW_IF,   EK_TOK_KW_ELSE, EK_TOK_COLON,    EK_TOK_EQ,
        EK_TOK_COLONEQ, EK_TOK_DOT,     EK_TOK_LPAREN,   EK_TOK_RPAREN,
        EK_TOK_LBRACKET, EK_TOK_RBRACKET, EK_TOK_LBRACE, EK_TOK_RBRACE,
        EK_TOK_COMMA,   EK_TOK_SEMI,    EK_TOK_ARROW,    EK_TOK_LARROW,
    };
    const size_t pool_n = sizeof pool / sizeof pool[0];
    size_t i = 0;
    int line = 1;

    while (i + 6 < cap) {
        size_t len = rnd() % 5; /* 0 = a blank line */
        int col = (int)(rnd() % 24) + 1;
        size_t k;

        for (k = 0; k < len; k++) {
            memset(&t[i], 0, sizeof t[i]);
            t[i].kind = pool[rnd() % pool_n];
            t[i].line = line;
            t[i].col = col;
            col += (int)(rnd() % 4);
            i++;
        }
        memset(&t[i], 0, sizeof t[i]);
        t[i].kind = EK_TOK_NEWLINE;
        t[i].line = line;
        t[i].col = col;
        i++;
        line++;
    }
    if (i > 0 && t[i - 1].kind == EK_TOK_NEWLINE && rnd() % 4 == 0) {
        i--; /* sometimes the file ends without a line terminator */
    }
    memset(&t[i], 0, sizeof t[i]);
    t[i].kind = EK_TOK_EOF;
    t[i].line = line;
    t[i].col = 1;
    return i + 1;
}

static void test_random_streams(void)
{
    enum { CAP = 200, CASES = 4000 };
    ek_token toks[CAP];
    int c;

    for (c = 0; c < CASES; c++) {
        ek_arena *arena = ek_arena_create();
        ek_diag *diag = ek_diag_create();
        ek_layout_result r;
        size_t n;

        n = random_stream(toks, CAP);
        r = ek_layout(toks, n, "fuzz.eko", arena, diag);
        check_stream(&r, ek_diag_count(diag, EK_SEV_ERROR),
                     (unsigned long long)c, "random");
        ek_diag_destroy(diag);
        ek_arena_destroy(arena);
    }
}

static void test_truncations(void)
{
    size_t mods = 0, m;
    unsigned long long id = 0;
    const char *const *paths = fx_modules(&mods);

    for (m = 0; m < mods; m++) {
        size_t len = 0, off;
        char *src = fx_read_file(paths[m], &len);
        char *prefix;

        EK_CHECK(src != NULL);
        if (!src) {
            continue;
        }
        prefix = malloc(len + 1);
        EK_CHECK(prefix != NULL);
        if (!prefix) {
            free(src);
            continue;
        }
        for (off = 0; off <= len; off++) {
            layout_fixture f;

            memcpy(prefix, src, off);
            prefix[off] = '\0';
            fx_begin(&f, prefix);
            check_stream(&f.r, ek_diag_count(f.diag, EK_SEV_ERROR),
                         id++, "trunc");
            fx_end(&f);
        }
        free(prefix);
        free(src);
    }
    fuzz_truncations = id;
}

int main(void)
{
    test_truncations();
    test_random_streams();
    printf("layout/fuzz: %llu truncation and random cases checked\n",
           fuzz_cases);
    EK_CHECK_U64(fuzz_failures, 0);
    EK_CHECK_U64(fuzz_truncations > 0, 1);
    return ek_test_report("layout/fuzz");
}
