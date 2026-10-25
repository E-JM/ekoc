#include "common.h"

/* The parse-rejected negative entries of Annex D: N04, N16, and N17
   have no published code, so the diagnostic names the violated clause
   (Annex D.3, Annex B.3). Each module must come back as a partial AST
   with an error, never a hang. */

/* N04 (Clause 6.7.1): a bare expression is not an arm. */
static void n04_bare_panic_arm(void)
{
    parse_fixture f;

    pfx_begin(&f,
        "f: (r: Result Int String) -> Int =\n"
        "    case r:\n"
        "        Ok v -> v\n"
        "        panic(\"missing\")\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-bare-case-arm"));
    EK_CHECK(pfx_msg_has(&f, "6.7.1"));
    EK_CHECK(f.ast.module != NULL);
    pfx_end(&f);
}

/* N16 (Clause 6.5.5): comparisons do not associate. */
static void n16_chained_comparison(void)
{
    parse_fixture f;

    pfx_begin(&f,
        "f: (a: Int, b: Int, c: Int) -> Bool =\n"
        "    a < b < c\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-chained-comparison"));
    EK_CHECK(pfx_msg_has(&f, "6.5.5"));
    EK_CHECK(f.ast.module != NULL);
    pfx_end(&f);

    /* The same rule covers equality (Clause 6.5.5). */
    pfx_begin(&f, "f: Int =\n    1 == 2 == 3\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-chained-comparison"));
    pfx_end(&f);
}

/* N17 (Clause 6.10): index syntax does not exist. */
static void n17_index_syntax(void)
{
    parse_fixture f;

    pfx_begin(&f, "f: (xs: []Int) -> Int =\n    xs[0]\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-index-syntax"));
    EK_CHECK(pfx_msg_has(&f, "6.10"));
    EK_CHECK(f.ast.module != NULL);
    pfx_end(&f);

    /* Braces are the only value brackets (Clause 6.10). */
    pfx_begin(&f, "f: Int =\n    [1, 2]\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-value-bracket"));
    EK_CHECK(pfx_msg_has(&f, "6.10"));
    pfx_end(&f);
}

int main(void)
{
    n04_bare_panic_arm();
    n16_chained_comparison();
    n17_index_syntax();
    return ek_test_report("parse/negative");
}
