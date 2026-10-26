#include "common.h"

#include "lex/internal.h"

/* String literals: escapes, interpolation, and the
   single-line rule. */

/* Counts literal segments and slot runs of the first token. */
static void test_plain_strings(void)
{
    lex_fixture f;

    fx_begin(&f, "\"\"");
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_STRING);
    EK_CHECK_U64(fx_tok(&f, 0).part_count, 0);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "\"text\"");
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_STRING);
    EK_CHECK_U64(fx_tok(&f, 0).part_count, 1);
    EK_CHECK(fx_tok(&f, 0).parts[0].text != NULL);
    EK_CHECK_U64(fx_tok(&f, 0).parts[0].text_len, 4);
    EK_CHECK(memcmp(fx_tok(&f, 0).parts[0].text, "text", 4) == 0);
    fx_end(&f);

    fx_begin(&f, "\"h\xC3\xA9llo\"");
    EK_CHECK_U64(fx_tok(&f, 0).part_count, 1);
    EK_CHECK_U64(fx_tok(&f, 0).parts[0].text_len, 6);
    EK_CHECK(memcmp(fx_tok(&f, 0).parts[0].text, "h\xC3\xA9llo", 6) == 0);
    fx_end(&f);
}

static void test_escapes(void)
{
    struct {
        const char *src;
        const char *cooked;
        size_t len;
    } cases[] = {
        { "\"a\\nb\"", "a\nb", 3 },
        { "\"a\\tb\"", "a\tb", 3 },
        { "\"a\\rb\"", "a\rb", 3 },
        { "\"a\\\\b\"", "a\\b", 3 },
        { "\"a\\\"b\"", "a\"b", 3 },
        { "\"\\{\"", "{", 1 },
        { "\"\\u{41}\"", "A", 1 },
        { "\"a\\u{1F600}b\"", "a\xF0\x9F\x98\x80" "b", 6 },
        { "\"\\u{0}\"", "\0", 1 },
        { "\"\\u{10FFFF}\"", "\xF4\x8F\xBF\xBF", 4 },
    };
    size_t i;

    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        lex_fixture f;

        fx_begin(&f, cases[i].src);
        EK_CHECK_U64(fx_count(&f), 2);
        if (fx_count(&f) == 2) {
            ek_token t = fx_tok(&f, 0);
            EK_CHECK_U64(t.kind, EK_TOK_STRING);
            EK_CHECK_U64(t.part_count, 1);
            if (t.part_count == 1) {
                EK_CHECK_U64(t.parts[0].text_len, cases[i].len);
                if (t.parts[0].text_len == cases[i].len) {
                    EK_CHECK(memcmp(t.parts[0].text, cases[i].cooked,
                                    cases[i].len) == 0);
                }
            }
        }
        EK_CHECK_U64(fx_errors(&f), 0);
        fx_end(&f);
    }
}

static void test_invalid_escapes(void)
{
    static const char *const bad[] = {
        "\"\\q\"", "\"\\x\"", "\"\\1\"", "\"\\'",
        "\"\\u{}\"", "\"\\u{110000}\"", "\"\\u{D800}\"",
        "\"\\u{DFFF}\"", "\"\\u{1234567}\"", "\"\\u{ag}\"",
        "\"\\u{ 41}\"", "\"\\u41\"", "\"\\u{123", "\"\\u",
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        lex_fixture f;

        fx_begin(&f, bad[i]);
        EK_CHECK(fx_has_error(&f, "E-escape"));
        fx_end(&f);
    }
}

static void test_single_line_rule(void)
{
    lex_fixture f;

    fx_begin(&f, "\"abc\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);

    fx_begin(&f, "\"abc");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);

    fx_begin(&f, "\"abc\r\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);

    fx_begin(&f, "\"abc\\");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);

    /* a stray CR inside the string is the source-level fault */
    fx_begin(&f, "\"a\rb\"\nx");
    EK_CHECK(fx_has_error(&f, "E-stray-cr"));
    fx_end(&f);
}

static void test_interpolation_parts(void)
{
    lex_fixture f;
    const ek_token *t;

    fx_begin(&f, "\"hi {name}, n={n}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count(&f), 2);
    t = &f.r.tokens[0];
    EK_CHECK_U64(t->kind, EK_TOK_STRING);
    EK_CHECK_U64(t->part_count, 4);
    if (t->part_count == 4) {
        EK_CHECK(t->parts[0].text != NULL);
        EK_CHECK_U64(t->parts[0].text_len, 3);
        EK_CHECK(memcmp(t->parts[0].text, "hi ", 3) == 0);

        EK_CHECK(t->parts[1].tokens != NULL);
        EK_CHECK_U64(t->parts[1].token_count, 1);
        if (t->parts[1].token_count == 1) {
            EK_CHECK_U64(t->parts[1].tokens[0].kind, EK_TOK_IDENT);
            EK_CHECK_STR(t->parts[1].tokens[0].text, "name");
        }

        EK_CHECK_U64(t->parts[2].text_len, 4);
        EK_CHECK(memcmp(t->parts[2].text, ", n=", 4) == 0);

        EK_CHECK(t->parts[3].tokens != NULL);
        EK_CHECK_U64(t->parts[3].token_count, 1);
        if (t->parts[3].token_count == 1) {
            EK_CHECK_U64(t->parts[3].tokens[0].kind, EK_TOK_IDENT);
        }
    }
    fx_end(&f);
}

static void test_slot_tokens(void)
{
    lex_fixture f;
    const ek_token *t;

    fx_begin(&f, "\"{a + b}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    t = &f.r.tokens[0];
    EK_CHECK_U64(t->part_count, 1);
    if (t->part_count == 1) {
        EK_CHECK_U64(t->parts[0].token_count, 3);
        if (t->parts[0].token_count == 3) {
            EK_CHECK_U64(t->parts[0].tokens[0].kind, EK_TOK_IDENT);
            EK_CHECK_U64(t->parts[0].tokens[1].kind, EK_TOK_PLUS);
            EK_CHECK_U64(t->parts[0].tokens[2].kind, EK_TOK_IDENT);
        }
    }
    fx_end(&f);

    /* braces of a composite literal balance inside the slot */
    fx_begin(&f, "\"{f({1, 2})}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    t = &f.r.tokens[0];
    EK_CHECK_U64(t->part_count, 1);
    if (t->part_count == 1) {
        EK_CHECK_U64(t->parts[0].token_count, 8);
    }
    fx_end(&f);

    /* a nested string is a STRING token carrying its own slot */
    fx_begin(&f, "\"outer {\"inner {n}\"}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    t = &f.r.tokens[0];
    EK_CHECK_U64(t->part_count, 2);
    if (t->part_count == 2) {
        const ek_token *inner = &t->parts[1].tokens[0];
        EK_CHECK_U64(t->parts[1].token_count, 1);
        EK_CHECK_U64(inner->kind, EK_TOK_STRING);
        EK_CHECK_U64(inner->part_count, 2);
        if (inner->part_count == 2) {
            EK_CHECK_U64(inner->parts[0].text_len, 6);
            EK_CHECK(memcmp(inner->parts[0].text, "inner ", 6) == 0);
            EK_CHECK_U64(inner->parts[1].token_count, 1);
            if (inner->parts[1].token_count == 1) {
                EK_CHECK_U64(inner->parts[1].tokens[0].kind, EK_TOK_IDENT);
            }
        }
    }
    fx_end(&f);
}

static void test_stray_brace_is_ordinary(void)
{
    lex_fixture f;

    fx_begin(&f, "\"a}b\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_tok(&f, 0).part_count, 1);
    EK_CHECK_U64(fx_tok(&f, 0).parts[0].text_len, 3);
    EK_CHECK(memcmp(fx_tok(&f, 0).parts[0].text, "a}b", 3) == 0);
    fx_end(&f);

    fx_begin(&f, "\"}\"");
    EK_CHECK_U64(fx_tok(&f, 0).parts[0].text_len, 1);
    fx_end(&f);

    /* \{ never interpolates */
    fx_begin(&f, "\"\\{name}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_tok(&f, 0).part_count, 1);
    EK_CHECK_U64(fx_tok(&f, 0).parts[0].text_len, 6);
    fx_end(&f);
}

static void test_empty_slot(void)
{
    lex_fixture f;

    fx_begin(&f, "\"{}\"");
    EK_CHECK(fx_has_error(&f, "E-empty-slot"));
    fx_end(&f);

    fx_begin(&f, "\"{ }\"");
    EK_CHECK(fx_has_error(&f, "E-empty-slot"));
    fx_end(&f);

    fx_begin(&f, "\"a{}b\"");
    EK_CHECK(fx_has_error(&f, "E-empty-slot"));
    fx_end(&f);
}

/* The slot is an expression region: a tab there is whitespace between
   tokens, while a tab in literal text is a scalar. */
static void test_slot_tab(void)
{
    lex_fixture f;

    fx_begin(&f, "\"{a\tb}\"");
    EK_CHECK(fx_has_error(&f, "E-tab"));
    fx_end(&f);

    fx_begin(&f, "\"a\t{name}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

/* A string may hold nothing but slots. */
static void test_slot_only_string(void)
{
    lex_fixture f;
    const ek_token *t;

    fx_begin(&f, "\"{x}\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    t = &f.r.tokens[0];
    EK_CHECK_U64(t->kind, EK_TOK_STRING);
    EK_CHECK_U64(t->part_count, 1);
    if (t->part_count == 1) {
        EK_CHECK_U64(t->parts[0].token_count, 1);
        EK_CHECK_U64(t->parts[0].tokens[0].kind, EK_TOK_IDENT);
    }
    fx_end(&f);
}

/* Past the implementation's own nesting bound, each
   refused '{' costs one E-slot-depth. Slot depth needs nested strings:
   "{ "{ x }" }" opens one slot per level. */
static size_t nest_slots(char *src, int depth)
{
    size_t used = 0, k;

    src[used++] = '"';
    for (k = 0; k < (size_t)depth; k++) {
        src[used++] = '{';
        src[used++] = '"';
    }
    src[used++] = 'x';
    for (k = 0; k < (size_t)depth; k++) {
        src[used++] = '"';
        src[used++] = '}';
    }
    src[used++] = '"';
    src[used] = '\0';
    return used;
}

static void test_slot_depth(void)
{
    char src[512];
    lex_fixture f;

    nest_slots(src, 60);
    fx_begin(&f, src);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    nest_slots(src, EK_LEX_MAX_SLOT_DEPTH + 5);
    fx_begin(&f, src);
    EK_CHECK(fx_has_error(&f, "E-slot-depth"));
    fx_end(&f);
}

/* A surplus of '}' swallows the closing quote; the string then dies at
   end of line (see scan_slot). */
static void test_surplus_brace_in_slot(void)
{
    lex_fixture f;

    fx_begin(&f, "\"{x}}\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);
}

/* A slot comment runs to end of line and takes the string's close. */
static void test_slot_comment_to_eol(void)
{
    lex_fixture f;

    fx_begin(&f, "\"{// x}");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);
}

/* Unbalanced braces swallow the closing quote, so the string always
   dies at end of line and E-unterminated-string is the diagnosis. */
static void test_unbalanced_slot(void)
{
    lex_fixture f;

    fx_begin(&f, "\"a{b\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);

    fx_begin(&f, "\"a{x: {1\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);

    fx_begin(&f, "\"a{b");
    EK_CHECK(fx_has_error(&f, "E-unterminated-string"));
    fx_end(&f);
}

static void test_string_position(void)
{
    lex_fixture f;

    fx_begin(&f, "  \"x\"");
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_STRING);
    EK_CHECK_U64(fx_tok(&f, 0).line, 1);
    EK_CHECK_U64(fx_tok(&f, 0).col, 3);
    fx_end(&f);
}

int main(void)
{
    test_plain_strings();
    test_escapes();
    test_invalid_escapes();
    test_single_line_rule();
    test_interpolation_parts();
    test_slot_tokens();
    test_stray_brace_is_ordinary();
    test_empty_slot();
    test_slot_tab();
    test_slot_only_string();
    test_slot_depth();
    test_surplus_brace_in_slot();
    test_slot_comment_to_eol();
    test_unbalanced_slot();
    test_string_position();
    return ek_test_report("lex/strings");
}
