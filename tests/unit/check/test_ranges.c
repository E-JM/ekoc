#include "common.h"

/* Ranges are the generic std ADT over one ordered scalar: the two
   operator forms construct it, the derived services read it, and its
   private constructor admits no literal or pattern outside std. */

/* Both operator forms build a Range, and the derived equality,
   ordering, and display services apply to it. */
static void range_values(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Range Int = 1..9\n"
              "b: Range Int = 0..=100\n"
              "lt: Bool = a.lt(b)\n"
              "eq: Bool = a.eq(b)\n"
              "s: String = a.show()\n"
              "c2: Range Char = 'a'..'z'\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The endpoints must agree; the mismatch is the range's own
   diagnostic. */
static void mismatched_endpoints(void)
{
    cfx f;

    cfx_begin(&f, "x: Range Int = 1..'z'\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);
}

/* Bool is outside the endpoint domain. */
static void invalid_endpoint_type(void)
{
    cfx f;

    cfx_begin(&f, "bad := True..False\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-operator", 1));
    cfx_end(&f);
}

/* The constructor is private: a composite literal naming it is
   rejected in resolve, and check never sees a type error from it. */
static void private_literal(void)
{
    cfx f;

    cfx_begin(&f,
              "x: Range Int = _Range {start: 1, end: 9, closed: False}\n");
    EK_CHECK(cfx_slice_has(f.ds, f.dn, "E-private-name"));
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A constructor pattern names the same private name. */
static void private_pattern(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (r: Range Int) -> Int =\n"
              "    case r:\n"
              "        _Range {start: s, end: e, closed: c} -> s\n");
    EK_CHECK(cfx_slice_has(f.ds, f.dn, "E-private-name"));
    cfx_end(&f);
}

/* A wildcard case over a Range needs no constructor. */
static void wildcard_case(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (r: Range Int) -> Int =\n"
              "    case r:\n"
              "        _ -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    range_values();
    mismatched_endpoints();
    invalid_endpoint_type();
    private_literal();
    private_pattern();
    wildcard_case();
    return ek_test_report("check/ranges");
}
