/* The eq and JSON halt judgments judge deep and recursive shapes
   exactly: a function or a capability buried at any depth still halts
   the derivation, and a recursive shape gets the verdict of its
   acyclic unrolling. */

#include "common.h"

/* A left-nested tuple type `depth` levels over one leaf type. */
static void deep_type(char *out, size_t cap, int depth, const char *leaf)
{
    char prev[512];
    size_t n;

    n = (size_t)snprintf(prev, sizeof prev, "%s", leaf);
    while (depth-- > 0 && n + 9 < sizeof prev) {
        n = (size_t)snprintf(out, cap, "[%s, Int]", prev);
        memcpy(prev, out, n + 1);
    }
    snprintf(out, cap, "%s", prev);
}

/* eq over a deep tuple: a function leaf halts at any depth, an Int
   leaf never halts. */
static void case_deep_eq(void)
{
    char ty[512];
    char src[700];
    cfx f;

    deep_type(ty, sizeof ty, 30, "(Int) -> Int");
    snprintf(src, sizeof src, "Deep: type = %s\n\nf: (d: Deep) -> Bool = "
                              "d.eq(d)\n",
             ty);
    cfx_begin(&f, src);
    EK_CHECK(cfx_has_code(&f, "E-no-eq"));
    cfx_end(&f);

    deep_type(ty, sizeof ty, 30, "Int");
    snprintf(src, sizeof src, "Deep: type = %s\n\nf: (d: Deep) -> Bool = "
                              "d.eq(d)\n",
             ty);
    cfx_begin(&f, src);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The JSON pair follows the same reachability. */
static void case_deep_json(void)
{
    char ty[512];
    char src[700];
    cfx f;

    deep_type(ty, sizeof ty, 30, "(Int) -> Int");
    snprintf(src, sizeof src, "Deep: type = %s\n\nf: (d: Deep) -> Json = "
                              "d.to_json()\n",
             ty);
    cfx_begin(&f, src);
    EK_CHECK(cfx_has_code(&f, "E-json-shape"));
    cfx_end(&f);

    deep_type(ty, sizeof ty, 30, "Int");
    snprintf(src, sizeof src, "Deep: type = %s\n\nf: (d: Deep) -> Json = "
                              "d.to_json()\n",
             ty);
    cfx_begin(&f, src);
    EK_CHECK(cfx_has_code(&f, "E-json-shape") == 0);
    cfx_end(&f);
}

/* A recursive shape is judged by what it unrolls to: a function in
   the cycle halts, a plain cycle does not. */
static void case_recursive(void)
{
    cfx f;

    cfx_begin(&f,
              "R: type = [r: R, f: (Int) -> Int]\n"
              "\n"
              "g: (x: R) -> Bool = x.eq(x)\n");
    EK_CHECK(cfx_has_code(&f, "E-no-eq"));
    cfx_end(&f);

    cfx_begin(&f,
              "Nat: type = Zero | Succ Nat\n"
              "\n"
              "g: (n: Nat) -> Bool = n.eq(n)\n");
    EK_CHECK(cfx_has_code(&f, "E-no-eq") == 0);
    cfx_end(&f);

    cfx_begin(&f,
              "R: type = [r: R, f: (Int) -> Int]\n"
              "\n"
              "g: (x: R) -> Json = x.to_json()\n");
    EK_CHECK(cfx_has_code(&f, "E-json-shape"));
    cfx_end(&f);
}

/* A generic declaration's payloads are judged at the instance: the
   binder's replacement reaches the halt. */
static void case_generic_payload(void)
{
    cfx f;

    cfx_begin(&f,
              "T: type t = Wrap ((t) -> Int)\n"
              "\n"
              "f: (d: T Int) -> Bool = d.eq(d)\n");
    EK_CHECK(cfx_has_code(&f, "E-no-eq"));
    cfx_end(&f);

    cfx_begin(&f,
              "T: type t = Wrap t\n"
              "\n"
              "f: (d: T Int) -> Bool = d.eq(d)\n");
    EK_CHECK(cfx_has_code(&f, "E-no-eq") == 0);
    cfx_end(&f);
}

/* The JSON form wants a string key; any other key halts whatever the
   value is. */
static void case_json_keys(void)
{
    cfx f;

    cfx_begin(&f, "f: (d: [Int] Int) -> Json = d.to_json()\n");
    EK_CHECK(cfx_has_code(&f, "E-json-shape"));
    cfx_end(&f);

    cfx_begin(&f, "f: (d: [String] Int) -> Json = d.to_json()\n");
    EK_CHECK(cfx_has_code(&f, "E-json-shape") == 0);
    cfx_end(&f);
}

int main(void)
{
    case_deep_eq();
    case_deep_json();
    case_recursive();
    case_generic_payload();
    case_json_keys();
    return ek_test_report("check/halt");
}
