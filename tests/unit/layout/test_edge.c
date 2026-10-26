#include "common.h"

/* Edge cases from the Annex A.4 white-box audit: the first significant
   line (rules 5 and 6), header terminators (Clause 6.2.3), dedents
   across and between levels (Clause 5.2.2), bracket asymmetry at end of
   file (Clause 5.3.3), callback block edges (Clause 6.2.5), and blank
   input. */

/* The first significant line is deeper than the stack bottom with no
   header before it: rule 6 ignores its indentation and leaves the
   diagnosis to parse, so layout emits no INDENT and no error. */
static void test_first_line_indented(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "    x: Int = 1\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S N E");
    free(got);
    fx_end(&f);
}

/* An else at the stack bottom column needs no DEDENT and passes
   through; the parser judges whether an if exists to reattach. */
static void test_else_at_column_zero_at_start(void)
{
    fx_skeleton_check("else:\n    b\n", "S S N I S N D E");
}

/* An indented else demands a block at its column; with only the bottom
   on the stack nothing matches, which is the rule 5 error. */
static void test_else_indented_at_start(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "    else:\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-dedent"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S N D E");
    free(got);
    fx_end(&f);
}

/* ':=' is one token and neither of the rule 6 terminators ':' and '=',
   so it opens no block; the deeper line splices (Clause 6.3.1). */
static void test_coloneq_does_not_open_block(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "count :=\n    a\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S N E");
    free(got);
    fx_end(&f);
}

/* '=' is a rule 6 terminator: the deeper line is the block's first
   member, processed by rule 5. */
static void test_eq_opens_block(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "count =\n    a\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S N I S N D E");
    free(got);
    fx_end(&f);
}

/* '==' and '..=' end in '=' as characters but are other tokens; they
   open no block. */
static void test_eqeq_does_not_open_block(void)
{
    layout_fixture f;

    fx_begin(&f, "a ==\n    b\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 0);
    fx_end(&f);
}

/* A dedent may close several levels at once (Clause 5.2.2): else at
   column zero pops both the if's and the case's block. */
static void test_dedent_past_two_levels(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "a: Int =\n    if x:\n        b\nelse:\n    c\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_DEDENT), 3);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S S S N I S N D D S S N I S N D E");
    free(got);
    fx_end(&f);
}

/* A dedent to a column between two open blocks pops past them and
   reports the mismatch once (Clause 5.2.2). */
static void test_dedent_between_levels(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "a: Int =\n    if x:\n        b\n   c\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-dedent"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S S S N I S N D D S N D E");
    free(got);
    fx_end(&f);
}

/* Steps of twelve are legal; steps of one and three are diagnosed
   (Clause 5.2.2) and drain by opening the block anyway. */
static void test_indent_steps(void)
{
    fx_skeleton_check("a: Int =\n            x\n",
                      "S S S S N I S N D E");
    layout_fixture f;
    char *got;

    fx_begin(&f, "a: Int =\n x\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-step"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S N D E");
    free(got);
    fx_end(&f);

    fx_begin(&f, "a: Int =\n   x\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-step"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S N D E");
    free(got);
    fx_end(&f);
}

/* Blank lines do not break a header from its first member (5.2.4):
   next_sig skips them for the splice decision. */
static void test_blank_between_header_and_member(void)
{
    fx_skeleton_check("a: Int =\n\n    x\n", "S S S S N I S N D E");
}

/* An unclosed bracket at end of file emits no NEWLINE and no DEDENT;
   the missing closer is a parse diagnosis (Clause 5.3.3). */
static void test_unclosed_bracket_at_eof(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f(a,\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S E");
    free(got);
    fx_end(&f);
}

/* An extra closer at depth zero and mismatched kinds pass through; the
   bracket depth is one counter over all six brackets (A.4 rule 3). */
static void test_extra_and_mismatched_brackets(void)
{
    fx_skeleton_check("a)\nb\n", "S S N S N E");
    fx_skeleton_check("f([)\n]\n", "S S S S S N E");
}

/* A callback header immediately before end of file never opened, so
   nothing is owed (A.4 rule 4). */
static void test_callback_header_at_eof(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f((a):\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_DEDENT), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S E");
    free(got);
    fx_end(&f);
}

/* A bracket opened on line 1 and closed on line 2: free formatting, no
   NEWLINE anywhere (A.4 rule 3). */
static void test_bracket_opened_on_line_one(void)
{
    fx_skeleton_check("f(a,\nb)\n", "S S S S S S N E");
}

/* Sibling callbacks inside one call: the first block's DEDENT precedes
   its closing bracket, the second header opens after it, and an
   inline body splices with no NEWLINE (Clauses 6.2.4 and 6.2.5). */
static void test_sibling_callbacks(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f(g((a):\n        x\n        ), (b):\n        y)\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
                 "S S S S S S S S N I S N D S S S S S S S S N E");
    free(got);
    fx_end(&f);
}

/* A callback body line deeper than the member column is free
   formatting: rule 3 ignores it, so it splices (A.4 rule 3). */
static void test_callback_member_deeper_splices(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f((a):\n    x\n        y)\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S N I S S D S N E");
    free(got);
    fx_end(&f);
}

/* A memberless callback block: the deeper closing-bracket line is the
   first member by column, so its DEDENT follows the INDENT directly
   and the empty member list surfaces at parse (A.4 rule 4). */
static void test_memberless_callback(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f((a):\n    )\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S N I D S N E");
    free(got);
    fx_end(&f);
}

/* An else deeper than the if it could match matches no open block and
   is diagnosed (A.4 rule 8, Clause 5.2.2). */
static void test_else_deeper_than_if(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "if x:\n    a\n        else:\n    b\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-dedent"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S N I S S S N D S N D E");
    free(got);
    fx_end(&f);
}

/* A header directly before end of file opened no member, so no INDENT
   is owed; the NEWLINE sits at the terminator (A.4 rules 6 and 10). */
static void test_header_at_eof(void)
{
    fx_skeleton_check("a: Int =\n", "S S S S N E");
}

/* Empty input and input of only NEWLINEs produce the lone EOF marker;
   blank lines are discarded by rule 2. */
static void test_empty_and_newline_only(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "");
    EK_CHECK_U64(fx_count(&f), 1);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_LTOK_EOF);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "\n\n\n");
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "E");
    free(got);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

int main(void)
{
    test_first_line_indented();
    test_else_at_column_zero_at_start();
    test_else_indented_at_start();
    test_coloneq_does_not_open_block();
    test_eq_opens_block();
    test_eqeq_does_not_open_block();
    test_dedent_past_two_levels();
    test_dedent_between_levels();
    test_indent_steps();
    test_blank_between_header_and_member();
    test_unclosed_bracket_at_eof();
    test_extra_and_mismatched_brackets();
    test_callback_header_at_eof();
    test_bracket_opened_on_line_one();
    test_sibling_callbacks();
    test_callback_member_deeper_splices();
    test_memberless_callback();
    test_else_deeper_than_if();
    test_header_at_eof();
    test_empty_and_newline_only();
    return ek_test_report("layout/edge");
}
