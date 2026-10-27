#include "common.h"

/* The assert family is std source: a call carries the real arrow, so
   arity and argument types are checked at the call site. The derived
   equality lives in the generic body, so a pair of capability values
   still passes while the operator form keeps reporting E-no-eq. */

/* The three calls in their ordinary form are clean and pure; a pure
   declaration carrying them reconciles. */
static void bare_forms(void)
{
    cfx f;

    cfx_begin(&f,
              "f: () -> _ =\n"
              "    assert(True)\n"
              "    assert_eq(1, 1)\n"
              "    assert_ne(\"a\", \"b\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_bit_at(&f, 2, EK_BIT_PURE));
    EK_CHECK(cfx_bit_at(&f, 3, EK_BIT_PURE));
    EK_CHECK(cfx_bit_at(&f, 4, EK_BIT_PURE));
    cfx_end(&f);

    cfx_begin(&f,
              "t: test =\n"
              "    assert_eq(1, 1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* assert takes Bool; the comparisons lock their two arguments to one
   type, so a mixed pair is rejected at the argument. */
static void argument_types(void)
{
    cfx f;

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert(1)\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert_eq(1, \"a\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert_ne(\"a\", 1)\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);
}

/* The signature fixes the parameter count. */
static void arity(void)
{
    cfx f;

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert_eq(1)\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert()\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert_ne(\"a\", \"b\", \"c\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);
}

/* Two capability values unify the body's single type variable; no eq
   derivation runs at the call site, so the pair stays clean. */
static void capability_pair(void)
{
    cfx f;

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    assert_eq(Out.null(), Out.null())\n"
              "    assert_ne(Out.null(), Out.null())\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The test-binding shape of the reported snippet stays clean. */
    cfx_begin(&f,
              "t: test =\n"
              "    assert_eq(Out.null(), Out.null())\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* An assertion inside a function that holds a capability is still a
   pure call; the capability parameter changes nothing. */
static void capability_in_scope(void)
{
    cfx f;

    cfx_begin(&f, "main: (out: Out) -> _ = assert(True)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_bit_at(&f, 1, EK_BIT_PURE));
    cfx_end(&f);
}

/* The operator form over a capability keeps firing: the gap covers the
   comparison inside the body, not `==` at the call site. */
static void capability_operator(void)
{
    cfx f;

    cfx_begin(&f, "main: (out: Out) -> _ = assert(out == out)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 1));
    cfx_end(&f);
}

/* The qualified spelling denotes the same declaration and carries the
   same arrow. */
static void qualified_calls(void)
{
    cfx f;

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    std.assert(True)\n"
              "    std.assert_eq(1, 2)\n"
              "    std.assert_ne(\"a\", \"b\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    std.assert(1)\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "g: () -> _ =\n"
              "    std.assert_eq(1)\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 2));
    cfx_end(&f);
}

int main(void)
{
    bare_forms();
    argument_types();
    arity();
    capability_pair();
    capability_in_scope();
    capability_operator();
    qualified_calls();
    return ek_test_report("check/asserts");
}
