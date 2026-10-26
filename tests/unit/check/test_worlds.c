#include "common.h"

/* The world bit: the negative conformance entries N05-N08, N13, and
   N20 at their exact codes, and the positive flows -- a pure body
   declared pure, a world body declared `!`, an open declaration fed by
   a `?` parameter and instantiated at its uses, the pure factory,
   dispatcher propagation, and test bindings that evaluate without
   reconciliation. */

/* N05 -- world bit under-declared. */
static void n05(void)
{
    cfx f;

    cfx_begin(&f,
              "greet: (out: Out) -> _ =\n"
              "    out.println(\"hi\")\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);
}

/* N06 -- under-declared through a dispatcher: the opaque `each`
   instantiates its open bit at `!` from the printing callback (the
   opacity rule in check.h). */
static void n06(void)
{
    cfx f;

    cfx_begin(&f,
              "print_all: (out: Out, xs: []String) -> _ =\n"
              "    xs.each((s): out.println(s))\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);
}

/* N07 -- world bit over-declared: a pure body cannot carry a surplus
   `!`. Bit diagnostics are errors, never warnings. */
static void n07(void)
{
    cfx f;

    cfx_begin(&f, "double: (n: Int) !-> Int = n * 2\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);
}

/* N08 -- open bit not fed by a parameter. */
static void n08(void)
{
    cfx f;

    cfx_begin(&f, "count: (xs: []Int) ?-> Int = List.length(xs)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-open-bit-unfed", 1));
    cfx_end(&f);
}

/* N13 -- no equality over capabilities. The `==` on a capability
   value is the form that fires under the bridge; the manifest's
   assert_eq snippet stays opaque (the gap is reported). */
static void n13_cap(void)
{
    cfx f;

    cfx_begin(&f, "main: (out: Out) -> _ = assert(out == out)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 1));
    cfx_end(&f);
}

/* N13 over functions: the eq derivation halts at function types. */
static void n13_fn(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (n: Int) -> Int = n + 1\n"
              "same: Bool = f == f\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 2));
    cfx_end(&f);
}

/* N13 through the derived member: `T.eq` does not exist where the
   derivation halts. */
static void n13_member(void)
{
    cfx f;

    cfx_begin(&f,
              "Box: type = [run: (Int) -> Int]\n"
              "a: Box = {run: (n): n}\n"
              "same: Bool = Box.eq(a, a)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-no-eq", 3));
    cfx_end(&f);
}

/* N20 -- `?` in a container element position. */
static void n20(void)
{
    cfx f;

    cfx_begin(&f,
              "is_zero: (n: Int) -> Bool = n == 0\n"
              "is_negative: (n: Int) -> Bool = n < 0\n"
              "\n"
              "handlers: [](Int) ?-> Bool = {is_zero, is_negative}\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-open-bit-position", 4));
    cfx_end(&f);
}

/* The open bit is also forbidden in a result type, nested or not. */
static void open_in_result(void)
{
    cfx f;

    cfx_begin(&f, "mk: () -> (Int) ?-> Bool = (n): True\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-open-bit-position", 1));
    cfx_end(&f);
}

/* A pure body under a pure declaration carries no diagnostic, and the
   rows carry their derived bits. */
static void pure_declared_pure(void)
{
    cfx f;

    cfx_begin(&f,
              "inc: (n: Int) -> Int =\n"
              "    n + 1\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bits_filled(&f));
    EK_CHECK(cfx_bit_at(&f, 1, EK_BIT_PURE));
    EK_CHECK(!cfx_has_code(&f, "E-world-bit"));
    cfx_end(&f);
}

/* A world body declared `!` passes, and the world bits show in the
   rows. */
static void world_declared_world(void)
{
    cfx f;

    cfx_begin(&f,
              "hit: (o: Out, s: String) !-> _ =\n"
              "    o.println(s)\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bits_filled(&f));
    EK_CHECK(cfx_bit_at(&f, 1, EK_BIT_WORLD));
    EK_CHECK(cfx_bit_at(&f, 2, EK_BIT_WORLD));
    cfx_end(&f);
}

/* The pure factory: a pure body returning a `!` closure. The lambda's
   calls belong to the closure's arrow. */
static void pure_factory_world_closure(void)
{
    cfx f;

    cfx_begin(&f,
              "mk_logger: (o: Out) -> (Int) !-> Int =\n"
              "    (n): o.println(show(n)); n + 1\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bit_at(&f, 1, EK_BIT_PURE));
    EK_CHECK(cfx_bit_at(&f, 2, EK_BIT_WORLD));
    cfx_end(&f);
}

/* An open declaration fed by a `?` parameter, instantiated at its
   uses: a pure callback keeps the call pure, a world-touching one
   makes it world. */
static void open_fed_and_instantiated(void)
{
    cfx f;

    cfx_begin(&f,
              "apply: (f: (Int) ?-> Int, n: Int) ?-> Int = f(n)\n"
              "\n"
              "safe: (n: Int) -> Int = apply((x): x + 1, n)\n"
              "\n"
              "logged: (o: Out, n: Int) !-> Int =\n"
              "    apply((x): o.println(\"run\"); x, n)\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bit_at(&f, 1, EK_BIT_OPEN));
    EK_CHECK(cfx_bits_filled(&f));
    cfx_end(&f);
}

/* A named pure function value also feeds a `?` parameter with a pure
   instantiation. */
static void open_fed_by_value(void)
{
    cfx f;

    cfx_begin(&f,
              "inc: (n: Int) -> Int = n + 1\n"
              "apply: (f: (Int) ?-> Int, n: Int) ?-> Int = f(n)\n"
              "use: (n: Int) -> Int = apply(inc, n)\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bit_at(&f, 3, EK_BIT_PURE));
    cfx_end(&f);
}

/* Under-declaration through a user dispatcher whose own arrow is open:
   a world-touching callback body makes the use world-touching. */
static void user_dispatcher_world_use(void)
{
    cfx f;

    cfx_begin(&f,
              "apply: (f: (Int) ?-> Int, n: Int) ?-> Int = f(n)\n"
              "bad: (o: Out, n: Int) -> Int =\n"
              "    apply((x): o.println(\"run\"); x, n)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 2));
    cfx_end(&f);
}

/* A lambda cannot adapt downward: a world-touching body under a pure
   expected arrow is E-world-bit. */
static void world_lambda_pure_param(void)
{
    cfx f;

    cfx_begin(&f,
              "use: (f: (Int) -> Int) -> Int = f(1)\n"
              "bad: (o: Out) -> Int = use((x): o.println(\"x\"); 0)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 2));
    cfx_end(&f);
}

/* A world-touching body in a declared open declaration is E-world-bit
   (the loud_apply shape). */
static void world_body_open_decl(void)
{
    cfx f;

    cfx_begin(&f,
              "loud: (o: Out, f: (Int) ?-> Int, n: Int) ?-> Int =\n"
              "    o.println(\"call\")\n"
              "    f(n)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);
}

/* A pure body under a declared open dispatch is E-world-bit (the
   twice_fast shape). */
static void pure_body_open_decl(void)
{
    cfx f;

    cfx_begin(&f, "twice: (f: (Int) ?-> Int, n: Int) -> Int = f(f(n))\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-world-bit", 1));
    cfx_end(&f);
}

/* Test bindings evaluate without reconciliation: the same
   world-touching body a `!`-less declaration would reject runs
   unchecked, and the rows still carry their bits. */
static void test_binding_no_reconcile(void)
{
    cfx f;

    cfx_begin(&f,
              "probe: test =\n"
              "    xs: []Int = {1}\n"
              "    xs.each((n): Out.println(\"x\"))\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bits_filled(&f));
    EK_CHECK(cfx_bit_at(&f, 3, EK_BIT_WORLD));
    EK_CHECK(!cfx_has_code(&f, "E-world-bit"));
    cfx_end(&f);
}

/* An inverted binding has no separate bit rule: the callback body's
   world calls surface in the enclosing body. */
static void invert_feeds_dispatcher(void)
{
    cfx f;

    cfx_begin(&f,
              "run: (o: Out) !-> _ =\n"
              "    x <- o.println(\"go\")\n"
              "    x\n");
    EK_CHECK(cfx_errors(&f) == 0);
    EK_CHECK(cfx_bit_at(&f, 1, EK_BIT_WORLD));
    cfx_end(&f);
}

int main(void)
{
    n05();
    n06();
    n07();
    n08();
    n13_cap();
    n13_fn();
    n13_member();
    n20();
    open_in_result();
    pure_declared_pure();
    world_declared_world();
    pure_factory_world_closure();
    open_fed_and_instantiated();
    open_fed_by_value();
    user_dispatcher_world_use();
    world_lambda_pure_param();
    world_body_open_decl();
    pure_body_open_decl();
    test_binding_no_reconcile();
    invert_feeds_dispatcher();
    return ek_test_report("check/worlds");
}
