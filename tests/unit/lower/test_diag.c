#include "common.h"

/* The E-lower diagnostics: constructs outside the lowerer's coverage
   come back as errors at the construct's position, and the parts that
   lowered still produce text. */

static void case_expr(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ = case 1: _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    EK_CHECK_U64(lfx_errors(&f), 1);
    EK_CHECK(lfx_has_code(&f, "E-lower"));
    EK_CHECK(lfx_code_at_line(&f, "E-lower", 1));
    EK_CHECK(f.low.text != NULL);
    EK_CHECK(lfx_has(&f, "#include \"ekort.h\""));
    EK_CHECK(lfx_has(&f, "int main(void)"));
    lfx_end(&f);
}

static void lambda(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    f: (Int) -> Int = (x): x\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    EK_CHECK_U64(lfx_errors(&f), 1);
    EK_CHECK(lfx_has_code(&f, "E-lower"));
    EK_CHECK(lfx_code_at_line(&f, "E-lower", 2));
    EK_CHECK(f.low.text != NULL);
    lfx_end(&f);
}

static void list_literal(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    xs: []Int = {1, 2}\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    EK_CHECK_U64(lfx_errors(&f), 1);
    EK_CHECK(lfx_has_code(&f, "E-lower"));
    EK_CHECK(lfx_code_at_line(&f, "E-lower", 2));
    EK_CHECK(f.low.text != NULL);
    lfx_end(&f);
}

static void interpolated_string(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    s = \"a{1}\"\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_has_code(&f, "E-lower"));
    EK_CHECK(lfx_code_at_line(&f, "E-lower", 2));
    EK_CHECK_U64(lfx_errors(&f), 1);
    EK_CHECK(f.low.text != NULL);
    lfx_end(&f);
}

int main(void)
{
    case_expr();
    lambda();
    list_literal();
    interpolated_string();
    return ek_test_report("lower/diag");
}
