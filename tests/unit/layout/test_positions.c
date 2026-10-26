#include "common.h"

/* Diagnostic positions and the columns named in their messages are
   counted from one, like every other stage. */

/* Drains the fixture's diagnostics into a caller array. */
static size_t drain(const layout_fixture *f, const ek_diagnostic *out[],
                    size_t cap)
{
    size_t n = 0;
    const ek_diagnostic *ds = ek_diag_drain(f->diag, &n);

    *out = ds;
    return n < cap ? n : cap;
}

/* A dedent matching no level names the line's column from one. */
static void test_dedent_column_is_one_based(void)
{
    layout_fixture f;
    const ek_diagnostic *ds;
    size_t n;

    fx_begin(&f, "f: Int =\n    a\n   b\n");
    n = drain(&f, &ds, 4);
    EK_CHECK_U64(n, 1);
    if (n == 1) {
        EK_CHECK_U64(ds[0].line, 3);
        EK_CHECK_U64(ds[0].col, 4);
        EK_CHECK_STR(ds[0].code, "E-indent-dedent");
        EK_CHECK_STR(ds[0].message,
                     "dedent to column 4 matches no open block");
    }
    fx_end(&f);
}

/* A callback member before the first member's column reports both
   columns from one. */
static void test_callback_columns_are_one_based(void)
{
    layout_fixture f;
    const ek_diagnostic *ds;
    size_t n;

    fx_begin(&f, "f((a):\n        x\n       y))\n");
    n = drain(&f, &ds, 4);
    EK_CHECK_U64(n, 1);
    if (n == 1) {
        EK_CHECK_U64(ds[0].line, 3);
        EK_CHECK_U64(ds[0].col, 8);
        EK_CHECK_STR(ds[0].code, "E-indent-column");
        EK_CHECK_STR(ds[0].message, "callback body line begins at column "
                     "8; its members begin in column 9");
    }
    fx_end(&f);
}

/* The chain alignment message already names source columns; keep both
   from one. */
static void test_chain_columns_are_one_based(void)
{
    layout_fixture f;
    const ek_diagnostic *ds;
    size_t n;

    fx_begin(&f, "c.split(\"\\n\")\n     .map(f)\n");
    n = drain(&f, &ds, 4);
    EK_CHECK_U64(n, 1);
    if (n == 1) {
        EK_CHECK_U64(ds[0].line, 2);
        EK_CHECK_U64(ds[0].col, 6);
        EK_CHECK_STR(ds[0].code, "E-indent-column");
        EK_CHECK_STR(ds[0].message, "chain line begins at column 6; "
                     "the chain's first dot is at column 2");
    }
    fx_end(&f);
}

/* An indentation-step message names the width of the step, not a
   position, so it is unaffected by the column base. */
static void test_step_message_names_width(void)
{
    layout_fixture f;
    const ek_diagnostic *ds;
    size_t n;

    fx_begin(&f, "f: Int =\n   x\n");
    n = drain(&f, &ds, 4);
    EK_CHECK_U64(n, 1);
    if (n == 1) {
        EK_CHECK_U64(ds[0].line, 2);
        EK_CHECK_U64(ds[0].col, 4);
        EK_CHECK_STR(ds[0].code, "E-indent-step");
        EK_CHECK_STR(ds[0].message,
                     "indentation step of 3 is not a multiple of four");
    }
    fx_end(&f);
}

int main(void)
{
    test_dedent_column_is_one_based();
    test_callback_columns_are_one_based();
    test_chain_columns_are_one_based();
    test_step_message_names_width();
    return ek_test_report("layout/positions");
}
