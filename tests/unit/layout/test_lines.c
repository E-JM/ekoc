#include "common.h"

/* Lines between members: blanks and comments, semicolons, chain and
   scrutinee continuation, and else. */

static void test_blank_lines(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "a: Int = 1\n\n\nb: Int = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S N S S S S S N E");
    free(got);
    fx_end(&f);
}

/* Comment-only lines reach layout as token-free lines: the lexer drops
   comments, so they establish nothing and terminate nothing. */
static void test_comment_only_lines(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "a: Int = 1\n// note\n/// doc\nb: Int = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S N S S S S S N E");
    free(got);
    fx_end(&f);
}

/* A semicolon emits no token and changes no stack; it passes through as
   a source token. */
static void test_semicolons(void)
{
    layout_fixture f;
    char *got;
    size_t i;
    int semi = 0;

    fx_begin(&f, "f: Int = 1; g: Int = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S S S S S S N E");
    free(got);
    for (i = 0; i < fx_count(&f); i++) {
        if (fx_tok(&f, i).kind == EK_LTOK_SOURCE
            && fx_tok(&f, i).tok.kind == EK_TOK_SEMI) {
            semi = 1;
        }
    }
    EK_CHECK(semi);
    fx_end(&f);
}

/* An aligned chain continuation splices: no NEWLINE at the end of the
   previous line. */
static void test_chain_continuation_aligned(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "c.split(\"\\n\")\n .map(f)\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S S S S S S N E");
    free(got);
    fx_end(&f);
}

/* A dot at any other column is an alignment error, still spliced for
   the parser. */
static void test_chain_continuation_misaligned(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "c.split(\"\\n\")\n     .map(f)\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-column"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S S S S S S N E");
    free(got);
    fx_end(&f);
}

/* A deeper line with no block expected splices without a NEWLINE: the
   layout layer emits no token, and a rogue line surfaces as a parse
   error in the joined stream. */
static void test_unlicensed_deeper_line_splices(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "a: Int = 1\n   b: Int = 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S S S S S N E");
    free(got);
    fx_end(&f);
}

/* A continued scrutinee: deeper lines after a non-header case line
   splice into the header. */
static void test_continued_scrutinee(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "case x,\n     y:\n    a\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S N I S N D E");
    free(got);
    fx_end(&f);
}

/* Block-form else: the previous member's NEWLINE, then the DEDENTs the
   stack requires, and no INDENT — the parser reattaches else. */
static void test_else_block_form(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f: Int =\n    if x:\n        a\n    else:\n        b\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S S S N I S N D S S N I S N D D E");
    free(got);
    fx_end(&f);
}

/* Inline-form else at the if's column: no DEDENT and no new member. */
static void test_else_inline_form(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f: Int =\n    if x: a\n    else: b\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S S S S N S S S N D E");
    free(got);
    fx_end(&f);
}

int main(void)
{
    test_blank_lines();
    test_comment_only_lines();
    test_semicolons();
    test_chain_continuation_aligned();
    test_chain_continuation_misaligned();
    test_unlicensed_deeper_line_splices();
    test_continued_scrutinee();
    test_else_block_form();
    test_else_inline_form();
    return ek_test_report("layout/lines");
}
