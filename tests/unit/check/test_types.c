#include "common.h"

/* Nominal vs structural records, shape adaptation in both directions,
   type equality without field order, tuple shapes into generic
   instantiations, literal placement, and composite literal checks. */

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

    /* Field order is not part of record equality. */
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
       position. */
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
       annotation. */
    cfx_begin(&f, "x := {1, 2}\n");
    EK_CHECK(cfx_has_code(&f, "E-literal-expected"));
    cfx_end(&f);
}

static void literal_placement(void)
{
    cfx f;

    /* Int, UInt, and Float placements. The machine words are outside
       the placeholder std surface, so an `U8` annotation reads as an
       open type here; their ranges are exercised as soon as the real
       std interface lands. */
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
       UInt, so its literals land at UInt. */
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
       `else` arm types as `Int`. */
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

/* A declared type whose width passes any fixed buffer: every binder
   occurs in the right-hand side, so the phantom check must stay
   silent at any arity. */
static void wide_decls(void)
{
    char buf[8192];
    size_t o = 0;
    int i;
    cfx f;

    o += (size_t)snprintf(buf + o, sizeof buf - o, "V: type ");
    for (i = 1; i <= 65; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "t%d%s", i,
                              i == 65 ? " = C [" : " ");
    }
    for (i = 1; i <= 65; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "%st%d",
                              i == 1 ? "" : ", ", i);
    }
    snprintf(buf + o, sizeof buf - o, "]\n");
    cfx_begin(&f, buf);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}


/* An empty brace literal takes its family from the expected type:
   the empty dictionary and the empty list are values, an empty record
   still names no field of a nonempty record, and an opaque expected
   type leaves the literal opaque. */
static void empty_literals(void)
{
    cfx f;

    cfx_begin(&f,
              "d: [String]Int = {}\n"
              "xs: []Int = {}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Nested empty dictionaries take their field's type. */
    cfx_begin(&f,
              "S: type = [d: [String]Int]\n"
              "s: S = {d: {}}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A nonempty record literal must name every field; `{}` names
       none. */
    cfx_begin(&f,
              "R: type = [x: Int]\n"
              "r: R = {}\n");
    EK_CHECK(cfx_code_at_line(&f, "E-partial-record", 2));
    cfx_end(&f);

    /* A tuple has at least two components. */
    cfx_begin(&f,
              "p: [Int, Int] = {}\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);
}

int main(void)
{
    nominal_vs_structural();
    shape_adaptation();
    record_literals();
    empty_literals();
    literal_placement();
    never_adaptation();
    wide_decls();
    return ek_test_report("check/types");
}
