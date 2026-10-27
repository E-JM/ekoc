#include "common.h"

/* The compiler-provided rows of the List namespace: the Annex C.3
   signatures over the structural list, both spellings, and the closed
   namespace — a name no row carries is absent, not bridged. */

static void list_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "n: Int = xs.length()\n"
              "g: Option Int = xs.get(0)\n"
              "t: []Int = xs.take(2)\n"
              "d: []Int = xs.drop(1)\n"
              "r: []Int = xs.reverse().append(xs)\n"
              "j: String = xs.join(\"-\")\n"
              "s: Option ([]Int) = xs.slice(0, 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "d: []Int = xs.map((x): x * 2)\n"
              "w: []Int = xs.filter((x): x > 1).sort()\n"
              "z: Int = xs.fold(0, (acc, x): acc + x)\n"
              "e: Bool = xs.any((x): x > 2)\n"
              "a: Bool = xs.all((x): x > 0)\n"
              "o: Option Int = xs.find_by((x): x > 1)\n"
              "b: []Int = xs.sort_by((p, q): p < q)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The type-namespace spelling carries the receiver as its first
       argument, and the chain head resolves through the List row. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []Int = List.map(xs, (x): x * 2)\n"
              "n: Int = List.length(xs)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_NAMESPACE));
    cfx_end(&f);
}

static void list_rows_types(void)
{
    cfx f;

    /* A row's result type is checked at the binding: map over Int
       does not land where []String is expected. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []String = xs.map((x): x * 2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);

    /* The receiver binds the element type first, so the callback's
       parameter is concrete: the argument mismatch and the result
       mismatch are both reported. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []Int = xs.map((x): x.eq(\"no\"))\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);

    /* Arity is the row's. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "n: Int = xs.length()\n"
              "m: Int = xs.length(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 3));
    cfx_end(&f);
}

static void list_namespace_closed(void)
{
    cfx f;

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "v: Int = xs.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "v: Int = List.bogus(xs)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    /* The derived services still derive over a list receiver; eq is
       structural, and the bogus miss is reported for it too. */
    cfx_begin(&f,
              "xs: []Int = {1, 2, 3}\n"
              "ys: []Int = {1, 2, 3}\n"
              "e: Bool = xs.eq(ys)\n"
              "v: Int = ys.inspect()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 4));
    cfx_end(&f);
}

int main(void)
{
    list_rows();
    list_rows_types();
    list_namespace_closed();
    return ek_test_report("check/namespaces");
}
