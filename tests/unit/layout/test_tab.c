#include "common.h"

/* A raw tab as indentation is E-tab, diagnosed once by the lexer and
   passed through by layout. The tab counts one column, so "\t   " puts
   the member at column four and the rest of the program still lays out
   without a second diagnostic. */

static void test_tab_indentation_diagnosed_once(void)
{
    layout_fixture f;
    char *got;

    fx_begin(&f, "count: Int =\n\t   0\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-tab"), 1);
    got = fx_skeleton(&f);
    EK_CHECK_STR(got, "S S S S N I S N D E");
    free(got);
    fx_end(&f);
}

/* A tab between tokens is the same lexical error; layout passes it
   through untouched. */
static void test_tab_between_tokens(void)
{
    layout_fixture f;

    fx_begin(&f, "x <-\tb\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK_U64(fx_count_code(&f, "E-tab"), 1);
    EK_CHECK(fx_count(&f) > 0);
    EK_CHECK_U64(fx_tok(&f, fx_count(&f) - 1).kind, EK_LTOK_EOF);
    fx_end(&f);
}

int main(void)
{
    test_tab_indentation_diagnosed_once();
    test_tab_between_tokens();
    return ek_test_report("layout/tab");
}
