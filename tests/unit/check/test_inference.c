#include "common.h"

/* Inference through lambdas and generics: implicit type variables,
   generalization at annotated bindings, instantiation by unification
   with argument types and the expected type, and the parameter-binding
   rule that keeps every variable determined. */

static void implicit_tvars(void)
{
    cfx f;

    /* An unbound lowercase name is a type variable of the function;
       it occurs in a parameter, so it is determined. */
    cfx_begin(&f,
              "id: (x: t) -> t = x\n"
              "n: Int = id(42)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The instantiation binds exactly: `String` does not become
       `Int`. (`True` is opaque under the placeholder surface, so a
       typed value anchors the instance.) */
    cfx_begin(&f,
              "id: (x: t) -> t = x\n"
              "n: Int = id(1)\n"
              "s: String = id(2)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_has_code(&f, "E-nominal-mismatch"));
    cfx_end(&f);

    /* A binder that occurs in the right-hand side is fine — the
       counterpart of N10. */
    cfx_begin(&f, "Pair: type t u = [t, u]\n"
                  "p: Pair Int String = {1, \"a\"}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void lambdas(void)
{
    cfx f;

    /* A lambda is checked against its expected function type; its
       parameters come from that type. */
    cfx_begin(&f,
              "inc: (Int) -> Int = (n): n + 1\n"
              "twice: (f: (Int) -> Int, n: Int) -> Int = f(f(n))\n"
              "v: Int = twice(inc, 1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* An unannotated binding is monomorphic and legal over a
       non-lambda right-hand side. */
    cfx_begin(&f, "x := 42\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Lambda arguments are checked last and absorb the callee's open
       parameter types (the opacity reading). */
    cfx_begin(&f,
              "apply: (f: (Int) -> Int, n: Int) -> Int = f(n)\n"
              "v: Int = apply((n): n * 2, 3)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A binding whose right-hand side is a lambda MUST carry an
       annotation, `=` included. */
    cfx_begin(&f, "k = (x): x\n");
    EK_CHECK(cfx_has_code(&f, "E-lambda-check-only"));
    cfx_end(&f);

    /* A lambda of the wrong arity under its annotation. */
    cfx_begin(&f, "k: (Int) -> Int = (a, b): a\n");
    EK_CHECK(cfx_has_code(&f, "E-call-arity"));
    cfx_end(&f);
}

static void generic_decls(void)
{
    cfx f;

    /* A recursive generic tree; every reference to the declaration
       is fully applied. */
    cfx_begin(&f,
              "Tree: type t = Leaf t | Node [Tree t, Tree t, t]\n"
              "\n"
              "size: (tree: Tree t) -> Int =\n"
              "    case tree:\n"
              "        Leaf _ -> 1\n"
              "        Node {l, r, _} -> size(l) + size(r)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Mutual recursion: each side may call the other before its
       declaration line. */
    cfx_begin(&f,
              "even: (n: Int) -> Bool = case n: 0 -> True; _ -> odd(n)\n"
              "odd: (n: Int) -> Bool = case n: 0 -> False; _ -> even(n)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A bare generic constructor has no self-anchoring type. */
    cfx_begin(&f,
              "Tree: type t = Leaf t | Node [Tree t, Tree t, t]\n"
              "x := Leaf 1\n");
    EK_CHECK(cfx_has_code(&f, "E-undetermined"));
    cfx_end(&f);

    /* With a known expected type the instance is determined. */
    cfx_begin(&f,
              "Tree: type t = Leaf t | Node [Tree t, Tree t, t]\n"
              "x: Tree Int = Leaf 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A generic reference must be fully applied, inside its own
       right-hand side too. */
    cfx_begin(&f, "Tree: type t = Leaf t | Node [Tree, Tree, t]\n");
    EK_CHECK(cfx_has_code(&f, "E-type-arity"));
    cfx_end(&f);
}

/* Generalization and instantiation at a width past any fixed buffer:
   every distinct variable of an annotation is generalized and replaced
   at the use, and the phantom-variable rule still sees the last one. */
static void wide_generics(void)
{
    char buf[8192];
    size_t o = 0;
    int i;
    cfx f;

    o += (size_t)snprintf(buf + o, sizeof buf - o, "pick: (");
    for (i = 1; i <= 65; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "p%d: t%d%s", i, i,
                              i == 65 ? ")" : ", ");
    }
    o += (size_t)snprintf(buf + o, sizeof buf - o, " -> t1 = p1\nuse: ");
    o += (size_t)snprintf(buf + o, sizeof buf - o,
                          "Int = pick(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,"
                          " 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,"
                          " 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,"
                          " 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,"
                          " 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,"
                          " 61, 62, 63, 64, 65)\n");
    cfx_begin(&f, buf);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    o = (size_t)snprintf(buf, sizeof buf, "f: (");
    for (i = 1; i <= 64; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "p%d: t%d, ", i, i);
    }
    snprintf(buf + o, sizeof buf - o, "p65: Int) -> t65 = p65\n");
    cfx_begin(&f, buf);
    EK_CHECK(cfx_has_code(&f, "E-phantom"));
    cfx_end(&f);
}

int main(void)
{
    implicit_tvars();
    lambdas();
    generic_decls();
    wide_generics();
    return ek_test_report("check/inference");
}
