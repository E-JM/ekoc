#include "common.h"

/* Bracket groups: free formatting inside ( [ {, callback blocks, and
   the DEDENT before a closing bracket. */

/* Line breaks and arbitrary indentation inside brackets are free. */
static void test_free_formatting(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f(a,\n  b,\n        c)\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S S S N E");
    free(got);
    fx_end(&f);
}

/* A callback block opens with NEWLINE INDENT and its DEDENT precedes
   the call's closing bracket. */
static void test_trailing_callback(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "rows.each((row):\n    x)\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S S S N I S D S N E");
    free(got);
    fx_end(&f);
}

/* A callback body member misaligned with the first member's column is
   an error and drains as a member. */
static void test_callback_member_misaligned(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f((a):\n        x\n       y))\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-indent-column"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S N I S N S D S S N E");
    free(got);
    fx_end(&f);
}

/* A line at or left of the header column closes the callback block
   before its own tokens. */
static void test_callback_closed_by_dedent(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "f(\n    (a):\n        x\n    y)\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S S S N I S D S S N E");
    free(got);
    fx_end(&f);
}

/* A multi-line callback inside a case scrutinee: the call closed by a
   line ending ')', and the case arms opening after the colon. */
static void test_case_scrutinee_with_callback(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f,
        "m: (fs: Fs) ! =\n"
        "    case fs.with_file(\"d\", Read,\n"
        "            (file):\n"
        "                x <- file.read()\n"
        "                Ok x):\n"
        "        Ok _ -> _\n"
        "        Err e -> e\n");
    EK_CHECK_U64(fx_errors(&f), 0);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got,
        "S S S S S S S S S N I "
        "S S S S S S S S S S S S S "
        "N I S S S S S S S "
        "N S S D S S "
        "N I S S S S "
        "N S S S S "
        "N D D E");
    free(got);
    fx_end(&f);
}

int main(void)
{
    test_free_formatting();
    test_trailing_callback();
    test_callback_member_misaligned();
    test_callback_closed_by_dedent();
    test_case_scrutinee_with_callback();
    return ek_test_report("layout/brackets");
}
