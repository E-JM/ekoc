#include "common.h"

/* Clause 7.6: inference through lambdas and generics. Implicit type
   variables, generalization at annotated bindings, instantiation by
   unification with argument types and the expected type, and the
   parameter-binding rule that keeps every variable determined. */

static void implicit_tvars(void)
{
    cfx f;

    /* An unbound lowercase name is a type variable of the function;
       it occurs in a parameter, so it is determined (7.6.1, 7.6.5). */
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
       parameters come from that type (Clause 7.6.3). */
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
       is fully applied (7.5.6, 7.6.2). */
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
       declaration line (Clause 7.4.2, 7.6.3). */
    cfx_begin(&f,
              "even: (n: Int) -> Bool = case n: 0 -> True; _ -> odd(n)\n"
              "odd: (n: Int) -> Bool = case n: 0 -> False; _ -> even(n)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A bare generic constructor has no self-anchoring type
       (Clause 7.6.4). */
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
       right-hand side too (Clause 7.6.2). */
    cfx_begin(&f, "Tree: type t = Leaf t | Node [Tree, Tree, t]\n");
    EK_CHECK(cfx_has_code(&f, "E-type-arity"));
    cfx_end(&f);
}

int main(void)
{
    implicit_tvars();
    lambdas();
    generic_decls();
    return ek_test_report("check/inference");
}
