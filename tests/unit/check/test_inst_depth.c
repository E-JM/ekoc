#include "common.h"

/* The instance set of Clause 8.7.3: expansion follows the generic call
   graph from its ground seeds. A polymorphic recursion that keeps
   producing a distinct instantiation is rejected; a recursion whose
   instances repeat, and a diverging generic no ground use reaches,
   are not. */

static void growing_recursion(void)
{
    cfx f;

    /* A direct polymorphic recursion at a fresh wrapper type: the
       instantiation types are Int, W Int, W (W Int), ... forever. */
    cfx_begin(&f,
              "W: type t = W t\n"
              "wrap: (x: t) -> W t = W x\n"
              "f: (x: t) -> Int = f(wrap(x))\n"
              "n: Int = f(0)\n");
    EK_CHECK(cfx_code_at_line(&f, "E-instantiation-depth", 3));
    cfx_end(&f);

    /* A mutual pair grows the same way. */
    cfx_begin(&f,
              "W: type t = W t\n"
              "wrap: (x: t) -> W t = W x\n"
              "f: (x: t) -> Int = g(wrap(x))\n"
              "g: (y: t) -> Int = f(wrap(y))\n"
              "n: Int = f(0)\n");
    EK_CHECK(cfx_has_code(&f, "E-instantiation-depth"));
    cfx_end(&f);
}

static void namespace_recursion(void)
{
    cfx f;

    /* A type-namespace member recurses through another member at a
       fresh wrapper type. */
    cfx_begin(&f,
              "M: type t = M t\n"
              "M.wrap: (x: t) -> M t = M x\n"
              "M.loop: (x: t) -> Int = M.loop(M.wrap(x))\n"
              "n: Int = M.loop(0)\n");
    EK_CHECK(cfx_code_at_line(&f, "E-instantiation-depth", 3));
    cfx_end(&f);
}

static void finite_or_uncalled(void)
{
    cfx f;

    /* Recursion that keeps the instantiation: the length walk calls
       itself at the same list type. */
    cfx_begin(&f,
              "len: (l: []t) -> Int =\n"
              "    case l:\n"
              "        {} -> 0\n"
              "        {h, ..r} -> 1 + len(r)\n"
              "xs: []Int = {1, 2, 3}\n"
              "n: Int = len(xs)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Two ground uses instantiate one generic at two types; the set
       is finite. */
    cfx_begin(&f,
              "id: (x: t) -> t = x\n"
              "a: Int = id(1)\n"
              "b: String = id(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* An uncalled diverging generic has no instance, so it is not an
       error (Clause 8.7.3). */
    cfx_begin(&f,
              "W: type t = W t\n"
              "wrap: (x: t) -> W t = W x\n"
              "f: (x: t) -> Int = f(wrap(x))\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "M: type t = M t\n"
              "M.wrap: (x: t) -> M t = M x\n"
              "M.loop: (x: t) -> Int = M.loop(M.wrap(x))\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    growing_recursion();
    namespace_recursion();
    finite_or_uncalled();
    return ek_test_report("check/inst-depth");
}
