#include "common.h"

/* Clause 7.5: nominal vs structural records, shape adaptation in both
   directions, type equality without field order, tuple shapes into
   generic instantiations, and literal placement (Clause 5.7.4) with
   composite literal checks (Clause 7.5.3). */

static void nominal_vs_structural(void)
{
    cfx f;

    /* Two declarations of one shape are different types; a value
       flows into the same declaration freely. */
    cfx_begin(&f,
              "Celsius: type = [deg: Float]\n"
              "c: Celsius = {deg: 0.0}\n"
              "c2: Celsius = c\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* An anonymous value never flows into a nominal type. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "a: [name: String, age: Int] = {name: \"ada\", age: 36}\n"
              "h: Human = a\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* Field order is not part of record equality (Clause 7.5.7). */
    cfx_begin(&f,
              "a: [x: Int, y: String] = {x: 1, y: \"s\"}\n"
              "b: [y: String, x: Int] = a\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void shape_adaptation(void)
{
    cfx f;

    /* Accepted: nominal to same-shape anonymous at an expected-type
       position (Clause 7.5.5). */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "h: Human = {name: \"ada\", age: 36}\n"
              "view: [name: String, age: Int] = h\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Rejected the other way: fewer fields is not the same shape. */
    cfx_begin(&f,
              "Wide: type = [name: String, age: Int, city: String]\n"
              "w: Wide = {name: \"ada\", age: 36, city: \"x\"}\n"
              "v: [name: String, age: Int] = w\n");
    EK_CHECK(cfx_code_at_line(&f, "E-shape-mismatch", 3));
    cfx_end(&f);

    /* A tuple shape extends to a generic instantiation by the shape
       of its right-hand side. */
    cfx_begin(&f,
              "Pair: type t u = [t, u]\n"
              "p: Pair Int String = {1, \"a\"}\n"
              "q: [Int, String] = p\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* An algebraic shape does not extend. */
    cfx_begin(&f,
              "Wrap: type t = Wrap t\n"
              "w: Wrap Int = Wrap 1\n"
              "v: [Int, Int] = w\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_has_code(&f, "E-shape-mismatch"));
    cfx_end(&f);
}

static void record_literals(void)
{
    cfx f;

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "h: Human = {name: \"ada\", age: 36}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "Human: type = [name: String, age: Int]\n"
                  "h: Human = {name: \"ada\"}\n");
    EK_CHECK(cfx_has_code(&f, "E-partial-record"));
    cfx_end(&f);

    cfx_begin(&f, "Human: type = [name: String, age: Int]\n"
                  "h: Human = {name: \"ada\", age: 36, city: \"x\"}\n");
    EK_CHECK(cfx_has_code(&f, "E-field-missing"));
    cfx_end(&f);

    cfx_begin(&f, "Human: type = [name: String, age: Int]\n"
                  "h: Human = {name: \"ada\", name: \"w\", age: 36}\n");
    EK_CHECK(cfx_has_code(&f, "E-field-duplicate"));
    cfx_end(&f);

    /* A composite literal with no expected type requires an
       annotation (Clause 7.5.3). */
    cfx_begin(&f, "x := {1, 2}\n");
    EK_CHECK(cfx_has_code(&f, "E-literal-expected"));
    cfx_end(&f);
}

static void literal_placement(void)
{
    cfx f;

    /* Int, UInt, and Float placements (Clause 5.7.4). The machine
       words are outside the placeholder std surface, so an `U8`
       annotation reads as an open type here; their ranges are
       exercised as soon as the real std interface lands. */
    cfx_begin(&f,
              "a: Int = 255\n"
              "b: UInt = 0xFF\n"
              "d: Float = 3.5\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "b: UInt = -1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);

    cfx_begin(&f, "b: UInt = 18446744073709551616\n");
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);

    /* A floating literal in an Int position is an error. */
    cfx_begin(&f, "x: Int = 1.5\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);

    /* An integer literal in a Float position is an error. */
    cfx_begin(&f, "x: Float = 1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);

    /* An unsigned binding does not take a floating literal either. */
    cfx_begin(&f, "x: UInt = 1.5\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* Placement flows through operators: the whole expression is
       UInt, so its literals land at UInt (Clause 7.8.4). */
    cfx_begin(&f, "x: UInt = 1 + 2\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* No expected type: the literal is Int. */
    cfx_begin(&f, "x := 255\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void never_adaptation(void)
{
    cfx f;

    /* A declared result of `Never` adapts at an expected-type
       position; the adaptation applies at value positions, so the
       `else` arm types as `Int` (Clause 7.5.4). */
    cfx_begin(&f,
              "die: (msg: String) -> Never = panic(msg)\n"
              "v: Int = if True: 1 else: die(\"no\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The adaptation does not enter a function type: `(a) -> Never`
       does not adapt to `(a) -> Int`. */
    cfx_begin(&f,
              "f: () -> Never = panic(\"x\")\n"
              "g: () -> Int = f\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);
}

int main(void)
{
    nominal_vs_structural();
    shape_adaptation();
    record_literals();
    literal_placement();
    never_adaptation();
    return ek_test_report("check/types");
}
