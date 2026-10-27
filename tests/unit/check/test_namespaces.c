#include "common.h"

/* The compiler-provided rows of the namespace table: the List
   signatures over the structural list, the scalar arithmetic rows, and
   the Out capability rows, both spellings, and the closed namespaces —
   a name no row carries is absent, not bridged. */

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

static void ints_rows(void)
{
    cfx f;

    /* Both spellings, a mixed operand type, and a constant member. */
    cfx_begin(&f,
              "a: Int = 1.add(2)\n"
              "b: Int = Int.sub(10, 4)\n"
              "c: Int = 2.pow(10)\n"
              "d: Int = Int.min\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The row's result lands at the binding. */
    cfx_begin(&f, "a: String = 1.add(2)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    cfx_end(&f);

    /* The row's parameter rejects the wrong operand type. */
    cfx_begin(&f, "a: Int = 1.add(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 1));
    cfx_end(&f);

    /* The scalar row set is partial, so a name it does not carry
       keeps the bridge until the set is complete. */
    cfx_begin(&f, "v: Int = Int.bogus(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 1, EK_RD_EXTERNAL));
    cfx_end(&f);

    /* A constant member is a value, not a function. */
    cfx_begin(&f, "v: Int = Int.min()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-not-callable", 1));
    cfx_end(&f);

    /* Unsigned values have no negation; the miss reports when the
       scalar set completes. */
    cfx_begin(&f, "v: UInt = UInt.neg(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void out_rows(void)
{
    cfx f;

    /* Printing touches the world, so a pure arrow cannot carry it. */
    cfx_begin(&f,
              "log: (out: Out) -> _ =\n"
              "    out.println(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);

    /* A world arrow admits the print, in both spellings. */
    cfx_begin(&f,
              "log: (out: Out) !-> _ =\n"
              "    out.println(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "log: (out: Out) !-> _ =\n"
              "    Out.println(out, \"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* null is a pure derivation. */
    cfx_begin(&f,
              "cap: () -> Out =\n"
              "    Out.null()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* tee observes through a world arrow but derives purely. */
    cfx_begin(&f,
              "t: (out: Out, obs: (String) !-> _) -> Out =\n"
              "    out.tee(obs)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The namespace is closed; the absent
       member leaves the body pure, so the surplus `!` is the second
       diagnostic. */
    cfx_begin(&f,
              "log: (out: Out) !-> Int =\n"
              "    v: Int = out.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 2));
    cfx_end(&f);
}

int main(void)
{
    list_rows();
    list_rows_types();
    list_namespace_closed();
    ints_rows();
    out_rows();
    return ek_test_report("check/namespaces");
}
