#include "common.h"

/* An inline annotation on an argument expression is not a form: the
   paren group parses to an empty sentinel, one directed diagnostic
   lands at the colon, and the argument list keeps its count and its
   own separators. */

static void directed_at_colon(void)
{
    parse_fixture f;

    pfx_begin(&f,
              "main: Int =\n"
              "    words.fold(({}: []String), (acc, w): acc)\n");
    EK_CHECK_U64(pfx_errors(&f), 1);
    pfx_collect(&f);
    EK_CHECK(f.dn == 1);
    EK_CHECK_STR(f.ds[0].code, "E-syntax");
    EK_CHECK(f.ds[0].line == 2 && f.ds[0].col == 19);
    EK_CHECK(pfx_msg_has(&f, "carries no annotation"));
    pfx_end(&f);
}

/* The legal shapes around the rejection: a lambda argument keeps its
   colon, and a plain parenthesized expression stays an expression. */
static void legal_shapes(void)
{
    parse_fixture f;

    pfx_begin(&f,
              "main: Int =\n"
              "    words.map((w): w.length()).fold(0, (a, n): a + n)\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    pfx_begin(&f,
              "main: Int =\n"
              "    one: Int = (1 + 2)\n"
              "    (one)\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);
}

int main(void)
{
    directed_at_colon();
    legal_shapes();
    return ek_test_report("parse/annotated-arg");
}
