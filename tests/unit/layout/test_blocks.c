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

/* An arm body on its own lines is a block: its members begin one
   level deeper than the arm line, so the arm block nests inside the
   case block.  A block-form if at the member column puts its else at
   that column; the else closes only the branch block and the arm block
   continues. */
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
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 5);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_DEDENT), 5);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S N I "
        "S S S N I "
        "S S N I "
        "S S S S S N I "
        "S N D "
        "S S N I "
        "S N D D "
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
        "S S N I "
        "S S S S S N I "
        "S S S S S S N I "
        "S S N D "
        "S S N I "
        "S S N D D "
        "S S N I "
        "S N D D "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* An inline-suite if as the first arm member opens no block; its else
   at the if header's column is still a continuation inside the arm
   block, so it neither diagnoses nor changes the stack. */
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
        "S S N I "
        "S S S S N "
        "S S S N D "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* An arm body block holds several members: bindings fix the block
   column like any other member, and the closing DEDENT follows the
   last member. */
static void test_arm_body_members(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (s: String) -> Result Int String =\n"
        "    case s:\n"
        "        \"a\" ->\n"
        "            n: Int = 1\n"
        "            Ok n\n"
        "        _ -> Err \"x\"\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S S S N I "
        "S S S N I "
        "S S N I "
        "S S S S S N "
        "S S N D "
        "S S S S N D D E");
    free(got);
    fx_end(&f);
}

/* A case in an arm body block opens its own arm block, so the nested
   arms sharpen the arm line's column with two more levels. */
static void test_arm_body_nested_case(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (s: String, t: String) -> Int =\n"
        "    case s:\n"
        "        \"a\" ->\n"
        "            case t:\n"
        "                \"b\" ->\n"
        "                    1\n"
        "                _ -> 2\n"
        "        _ -> 3\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_INDENT), 5);
    EK_CHECK_U64(fx_count_kind(&f, EK_LTOK_DEDENT), 5);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S S S S S N I "
        "S S S N I "
        "S S N I "
        "S S S N I "
        "S S N I "
        "S N D "
        "S S S N D D "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* A guarded or-pattern arm opens its body block from the arrow that
   ends the arm line, after the alternatives and the guard. */
static void test_arm_body_guard(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (k: Key, c: Bool) -> Int =\n"
        "    case k:\n"
        "        Up | Down if c ->\n"
        "            1\n"
        "        _ -> 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S S S S S N I "
        "S S S N I "
        "S S S S S S N I "
        "S N D "
        "S S S N D D E");
    free(got);
    fx_end(&f);
}

/* A multi-scrutinee case takes arm bodies of either shape; the block
   form arms each open their own body block. */
static void test_arm_body_multi_scrutinee(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (a: Bool, b: Bool) -> Int =\n"
        "    case a, b:\n"
        "        True, True ->\n"
        "            1\n"
        "        _, _ -> 2\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S S S S S N I "
        "S S S S S N I "
        "S S S S N I "
        "S N D "
        "S S S S S N D D E");
    free(got);
    fx_end(&f);
}

/* A rebinding in an arm body block is an ordinary member: its
   right-hand side ends the line and the next member follows. */
static void test_arm_body_rebind(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "f: (s: String) -> Int =\n"
        "    case s:\n"
        "        \"a\" ->\n"
        "            n := 0\n"
        "            n\n"
        "        _ -> 1\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S S N I "
        "S S S N I "
        "S S N I "
        "S S S N "
        "S N D "
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
    test_arm_body_members();
    test_arm_body_nested_case();
    test_arm_body_guard();
    test_arm_body_multi_scrutinee();
    test_arm_body_rebind();
    test_eof_closes_all_blocks();
    return ek_test_report("layout/blocks");
}
