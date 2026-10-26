#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"

#include "lex/lex.h"
#include "support/arena.h"
#include "support/diag.h"
#include "support/utf8.h"

/* Black-box properties over a hostile corpus: every input, however
   broken, lexes to a stream that ends with exactly one EOF token, every
   token carries a 1-based position, slot runs hold no NEWLINE or EOF,
   and diagnostics stay bounded by the input length. The
   corpus is every truncation of each conformance module plus
   deterministic random soups; termination itself is enforced by the
   runner's timeout, not here. */

static const char *const modules[] = {
    "tests/conformance/access_drilling.eko",
    "tests/conformance/bindings.eko",
    "tests/conformance/case_patterns.eko",
    "tests/conformance/data_services.eko",
    "tests/conformance/derived_services.eko",
    "tests/conformance/generics.eko",
    "tests/conformance/geolib.eko",
    "tests/conformance/geolib_use.eko",
    "tests/conformance/lambdas_capture.eko",
    "tests/conformance/result_chains.eko",
    "tests/conformance/types_composites.eko",
};

static unsigned long long rng_state = 0x123456789ABCDEF7ull;

static unsigned next_rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (unsigned)(rng_state >> 11);
}

static size_t inputs_run;
static size_t violations;

/* Returns 0 when the token run obeys every property. A slot run holds
   no NEWLINE and no EOF (lex.h); a top-level run holds NEWLINEs. */
static int bad_run(const ek_token *t, size_t count, int depth, int in_slot)
{
    size_t i;

    if (depth > 200) {
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (t[i].kind == EK_TOK_EOF) {
            return 1;
        }
        if (in_slot && t[i].kind == EK_TOK_NEWLINE) {
            return 1;
        }
        if (t[i].line < 1 || t[i].col < 1) {
            return 1;
        }
        if (t[i].kind == EK_TOK_STRING) {
            size_t j;

            if (t[i].part_count > 0 && t[i].parts == NULL) {
                return 1;
            }
            for (j = 0; j < t[i].part_count; j++) {
                const ek_string_part *p = &t[i].parts[j];

                if ((p->text == NULL) == (p->tokens == NULL)) {
                    return 1;
                }
                if (p->tokens != NULL
                    && bad_run(p->tokens, p->token_count, depth + 1, 1)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* The first violated stream property, or NULL when the token stream
   obeys them all. */
static const char *stream_violation(const ek_lex_result *r)
{
    size_t i, eofs = 0;
    const char *why = NULL;

    if (r->tokens == NULL || r->count == 0) {
        return "no token stream";
    }
    if (r->tokens[r->count - 1].kind != EK_TOK_EOF) {
        return "stream does not end with EOF";
    }
    for (i = 0; i < r->count && why == NULL; i++) {
        if (r->tokens[i].kind == EK_TOK_EOF) {
            eofs++;
        }
        if (r->tokens[i].line < 1 || r->tokens[i].col < 1) {
            why = "token outside the source";
        }
    }
    if (why == NULL && eofs != 1) {
        why = "more than one EOF";
    }
    if (why == NULL && bad_run(r->tokens, r->count - 1, 0, 0)) {
        why = "slot run holds NEWLINE/EOF or broken part";
    }
    return why;
}

static void check_input(const char *src, size_t len, const char *what)
{
    ek_arena *arena = ek_arena_create();
    ek_diag *diag = ek_diag_create();
    ek_lex_result r;
    const char *why;

    inputs_run++;
    if (arena == NULL || diag == NULL) {
        violations++;
        fprintf(stderr, "corpus: harness allocation failed on %s\n", what);
        EK_CHECK(0);
        if (diag != NULL) {
            ek_diag_destroy(diag);
        }
        if (arena != NULL) {
            ek_arena_destroy(arena);
        }
        return;
    }
    r = ek_lex(src, len, "corpus.eko", arena, diag);
    why = stream_violation(&r);
    if (why == NULL && ek_diag_count(diag, EK_SEV_ERROR) > len + 16) {
        why = "unbounded diagnostics";
    }
    if (why != NULL) {
        violations++;
        if (violations <= 10) {
            fprintf(stderr, "corpus: %s on %s\n", why, what);
        }
    }
    ek_diag_destroy(diag);
    ek_arena_destroy(arena);
}

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
    *len = (size_t)size;
    return data;
}

/* Every truncation of a valid program must diagnose or end cleanly. */
static void truncate_module(const char *path)
{
    size_t len = 0, cut;
    char *data = read_file(path, &len);

    EK_CHECK(data != NULL);
    if (!data) {
        return;
    }
    for (cut = 0; cut <= len; cut++) {
        char label[96];

        snprintf(label, sizeof label, "%s@%lu", path, (unsigned long)cut);
        check_input(data, cut, label);
    }
    free(data);
}

static void fill_raw(unsigned char *buf, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = (unsigned char)next_rng();
    }
}

static void fill_bait(unsigned char *buf, size_t len)
{
    static const char bait[] = " \t\r\n{}\"'\\0123456789abcdefxbo._:!<>+-*"
                               "/%&|?=,;()[]@";
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = (unsigned char)bait[next_rng() % (sizeof bait - 1)];
    }
}

static void fill_utf8(unsigned char *buf, size_t len)
{
    size_t i = 0;

    while (i < len) {
        uint32_t cp = next_rng() % 0x110000u;
        char out[4];
        size_t n;

        if (cp >= 0xD800 && cp <= 0xDFFF) {
            cp = 'x';
        }
        n = ek_utf8_encode(cp, out);
        if (n == 0 || i + n > len) {
            buf[i++] = 'x';
            continue;
        }
        memcpy(buf + i, out, n);
        i += n;
    }
}

static void fill_babble(char *buf, size_t cap, size_t *len)
{
    static const char *const pieces[] = {
        "a", "x1", "_", "1", "1x", "0x", "0b", "0x1g", "1_", "1e", "1e+",
        "1.5", ".5", "5.", "0x_1", "0b2", "\"", "'", "\\", "{", "}",
        "//", "/*", "// c\n", "\r", "\n", "\r\n", "\t", " ",
        "\xEF\xBB\xBF", "!->", "?->", "..", "..=", ":=", "->", "<-",
        "==", "!=", "&&", "||", "++", "\"{", "}\"", "\\u{", "\\u{}",
        "\\u{41}", "\\n", "import", "case", "\xCE\xBB", "\xCC\x81",
        "\xF0\x9F\x98\x80", "\xFF", "\xC3", ";", ",", "(", ")", "[", "]",
        "@", "$", "`", "~", "^", "&", "|", "%", "/", "1_000.000_1", "1e-3",
    };
    const size_t count = sizeof pieces / sizeof pieces[0];
    size_t used = 0, want = 8 + next_rng() % 24, k;

    for (k = 0; k < want && used + 16 < cap; k++) {
        const char *p = pieces[next_rng() % count];
        size_t n = strlen(p);

        memcpy(buf + used, p, n);
        used += n;
    }
    *len = used;
}

int main(void)
{
    unsigned char buf[256];
    char babbie[512];
    size_t i, m;

    for (m = 0; m < sizeof modules / sizeof modules[0]; m++) {
        truncate_module(modules[m]);
    }
    for (i = 0; i < 400; i++) {
        size_t len = 1 + next_rng() % sizeof buf;

        fill_raw(buf, len);
        check_input((const char *)buf, len, "raw bytes");
    }
    for (i = 0; i < 400; i++) {
        size_t len = 1 + next_rng() % sizeof buf;

        fill_bait(buf, len);
        check_input((const char *)buf, len, "bait bytes");
    }
    for (i = 0; i < 400; i++) {
        size_t len = 1 + next_rng() % sizeof buf;

        fill_utf8(buf, len);
        check_input((const char *)buf, len, "random UTF-8");
    }
    for (i = 0; i < 400; i++) {
        size_t len = 0;

        fill_babble(babbie, sizeof babbie, &len);
        check_input(babbie, len, "token babble");
    }
    EK_CHECK_U64(violations, 0);
    printf("lex/corpus: %lu hostile inputs\n", (unsigned long)inputs_run);
    return ek_test_report("lex/corpus");
}
