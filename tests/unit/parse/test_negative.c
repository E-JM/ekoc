#include "common.h"

/* Parse-rejected sources with no dedicated error code, so the
   diagnostic names the violated clause in its message text. Each
   module must come back as a partial AST with an error, never a
   hang. */

/* N04: a bare expression is not an arm. */
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

/* N16: comparisons do not associate. */
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

    /* The same check covers equality. */
    pfx_begin(&f, "f: Int =\n    1 == 2 == 3\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-chained-comparison"));
    pfx_end(&f);
}

/* N17: index syntax does not exist. */
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

    /* Braces are the only value brackets. */
    pfx_begin(&f, "f: Int =\n    [1, 2]\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-value-bracket"));
    EK_CHECK(pfx_msg_has(&f, "6.10"));
    pfx_end(&f);
}

/* N31: a record right-hand side on its own indented line. A value
   binding admits its right-hand side on the following lines; a `type`
   right-hand side after a newline is a vertical variant list, so every
   indented line leads with `|` (Annex A.3.2). */
static void n31_indented_record_rhs(void)
{
    parse_fixture f;

    pfx_begin(&f, "R: type =\n    [name: String]\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-type-decl-form"));
    EK_CHECK(pfx_msg_has(&f, "6.3.5"));
    EK_CHECK(f.ast.module != NULL);
    pfx_end(&f);

    pfx_begin(&f, "R: type =\n    (a: String, b: Int)\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-type-decl-form"));
    pfx_end(&f);

    pfx_begin(&f, "R: type =\n    []String\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-type-decl-form"));
    pfx_end(&f);
}

/* A comma inside a parenthesized group: the group holds one expression
   and the tuple literal is a brace form. The rejection is one
   diagnostic at the first comma, and the recovery keeps the group's
   binding and the members that follow it. */
static void paren_comma_group(void)
{
    parse_fixture f;

    /* The binding survives its rejected value, and the members after
       the line stay members of the same block. */
    pfx_begin(&f,
        "main: () -> _ =\n"
        "    p = (1, \"a\")\n"
        "    q = p\n"
        "    _ := q\n"
        "    _\n");
    EK_CHECK_U64(pfx_errors(&f), 1);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "braces"));
    EK_CHECK(pfx_msg_has(&f, "6.6.1"));
    EK_CHECK(f.ast.module != NULL);
    if (f.ast.module) {
        const ek_node *main = f.ast.module->u.kids.data[0];
        const ek_node *block = main->u.bind.value;

        EK_CHECK_U64(block->u.kids.count, 4);
        if (block->u.kids.count == 4) {
            EK_CHECK_U64(block->u.kids.data[0]->kind, EK_N_LET);
            EK_CHECK_U64(block->u.kids.data[1]->kind, EK_N_LET);
            EK_CHECK_U64(block->u.kids.data[2]->kind, EK_N_REBIND);
            EK_CHECK_U64(block->u.kids.data[3]->kind, EK_N_NAME);
        }
    }
    pfx_end(&f);

    /* A one-line binding value: one diagnostic, the LET survives. */
    pfx_begin(&f, "f: Int = (1, \"a\")\n");
    EK_CHECK_U64(pfx_errors(&f), 1);
    EK_CHECK(pfx_item(&f, 0) != NULL);
    pfx_end(&f);
}

/* N42 — a dotted head takes its arguments in parentheses; the
   juxtaposition form belongs to the bare constructor spelling. */
static void n41_dotted_head_juxtaposition(void)
{
    parse_fixture f;

    pfx_begin(&f,
        "main: () -> _ =\n"
        "    x := p.f 1\n"
        "    _\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    EK_CHECK(pfx_has_code(&f, "E-syntax"));
    EK_CHECK(pfx_msg_has(&f, "6.5.2"));
    EK_CHECK(f.ast.module != NULL);
    pfx_end(&f);
}

int main(void)
{
    n41_dotted_head_juxtaposition();
    n04_bare_panic_arm();
    n16_chained_comparison();
    n17_index_syntax();
    n31_indented_record_rhs();
    paren_comma_group();
    return ek_test_report("parse/negative");
}
