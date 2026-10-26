#include "common.h"

/* Block structure: nesting, first-member column, relative indentation,
   dedents, and end of file. */

/* A header opens a block; its NEWLINE INDENT pairs with the EOF DEDENT. */
static void test_simple_block(void)
{
    fx_skeleton_check("f: Int =\n    x\n", "S S S S N I S N D E");
}

/* The first member fixes the block column; two levels deeper is legal,
   and later members join without a second INDENT. */
static void test_first_member_fixes_column(void)
{
    fx_skeleton_check("f: Int =\n        x\n        y\n",
                      "S S S S N I S N S N D E");
    fx_skeleton_check("f: Int =\n        if x:\n            y\n",
                      "S S S S N I S S S N I S N D D E");
}

/* A step that is not a positive multiple of four is an error; layout
   drains by opening the block anyway. */
static void test_step_not_multiple_of_four(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f: Int =\n     x\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-step"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S N D E");
    free(got);
    fx_end(&f);
}

/* A dedent to a column matching no open block cannot close the block
   consistently; layout drains by pretending it did. */
static void test_dedent_to_unmatched_column(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f: Int =\n    a\n   b\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-dedent"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S N D S N D E");
    free(got);
    fx_end(&f);
}

/* An arm body on its own line is a continued expression: its header
   line splices into the arm.  A block-form if in that body puts its
   else at the if header's column, which is not an open stack level;
   the else still closes the branch block and continues the if, and the
   next arm lands back at the arm block's column. */
static void test_arm_body_else_at_header_column(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    case n:\n"
        "        0 ->\n"
        "            if n == 0:\n"
        "                1\n"
        "            else:\n"
        "                2\n"
        "        _ -> 3\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S N I "
        "S S S N I "
        "S S S S S S S N I "
        "S N D "
        "S S N I "
        "S N D "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* Else lines reattach through every arm-body depth: each branch block
   closes with its DEDENT and the enclosing if's branch continues. */
static void test_arm_body_nested_else(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (n: Int) -> Int =\n"
        "    case n:\n"
        "        0 ->\n"
        "            if n < 0:\n"
        "                if n < -10:\n"
        "                    -2\n"
        "                else:\n"
        "                    -1\n"
        "            else:\n"
        "                1\n"
        "        _ -> 3\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S N I "
        "S S S N I "
        "S S S S S S S N I "
        "S S S S S S N I "
        "S S N D "
        "S S N I "
        "S S N D D "
        "S S N I "
        "S N D "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* An inline-suite if in an arm body opens no block; its else at the if
   header's column is still a continuation, so it neither diagnoses nor
   changes the stack. */
static void test_arm_body_inline_if_else(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (s: String) -> Int =\n"
        "    case s:\n"
        "        \"a\" ->\n"
        "            if x: 1\n"
        "            else: 2\n"
        "        _ -> 3\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S N I "
        "S S S N I "
        "S S S S S S S S S N "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* End of file terminates the last line and closes every open block. */
static void test_eof_closes_all_blocks(void)
{
    fx_skeleton_check("a: Int =\n    b: Int =\n        c: Int =\n"
                      "            d\n",
                      "S S S S N I S S S S N I S S S S N I S N D D D E");
}

int main(void)
{
    test_simple_block();
    test_first_member_fixes_column();
    test_step_not_multiple_of_four();
    test_dedent_to_unmatched_column();
    test_arm_body_else_at_header_column();
    test_arm_body_nested_else();
    test_arm_body_inline_if_else();
    test_eof_closes_all_blocks();
    return ek_test_report("layout/blocks");
}
