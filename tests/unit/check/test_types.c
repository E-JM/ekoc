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

    /* The unit value is `_` itself: it is not a hole, and it adapts
       to nothing but the unit type. */
    cfx_begin(&f,
              "a: _ = _\n"
              "b: String = _\n"
              "c: Never = _\n"
              "d: []Int = {_}\n");
    EK_CHECK_U64(cfx_errors(&f), 3);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 3));
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 4));
    cfx_end(&f);

    /* A member of a bottom receiver never evaluates: the access
       types as `Never`, which adapts to the declared result. */
    cfx_begin(&f,
              "lt: (a: Never, b: Never) -> Bool = a.lt(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 1, EK_RD_NONE));
    cfx_end(&f);

    /* A call of a bottom callee does not return: it types as
       `Never` without a not-callable diagnostic, and adapts. */
    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "v: Int = die(\"x\")(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Coverage belongs to patterns: a diverging arm's pattern still
       covers its constructor. */
    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: (r: Result Int String) -> Int =\n"
              "    case r:\n"
              "        Ok n -> n\n"
              "        Err e -> die(\"no\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Every arm diverging is still exhaustive; the case is `Never`. */
    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: (b: Bool) -> Never =\n"
              "    case b:\n"
              "        True -> die(\"a\")\n"
              "        False -> die(\"b\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
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

    /* A std type name has no composite literal shape. */
    cfx_begin(&f, "s: String = {}\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);
}


/* A diagnostic at one line contains the needle. */
static int diag_has(const cfx *f, int line, const char *needle)
{
    size_t i;

    for (i = 0; i < f->cdn; i++) {
        if (f->cds[i].line == line && f->cds[i].message
            && strstr(f->cds[i].message, needle)) {
            return 1;
        }
    }
    return 0;
}

/* Diagnostics spell types the way the language writes them: list and
   dictionary brackets, nominal arguments, and function arrows. */
static void type_names(void)
{
    cfx f;

    cfx_begin(&f, "f: (xs: []Int) -> []String = xs\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    EK_CHECK(diag_has(&f, 1, "body is []Int"));
    EK_CHECK(diag_has(&f, 1, "result type is []String"));
    cfx_end(&f);

    cfx_begin(&f,
              "Box: type t = [v: t]\n"
              "mk_i: (n: Int) -> Box Int = (n): {v: n}\n"
              "mk_s: (s: String) -> Box String = (s): {v: s}\n"
              "g: (b: Bool) -> Box Int =\n"
              "    case b:\n"
              "        True -> mk_i(1)\n"
              "        False -> mk_s(\"s\")\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    EK_CHECK(diag_has(&f, 5, "Box Int and Box String"));
    cfx_end(&f);

    cfx_begin(&f, "d: (m: [String]Int) -> Int = m\n");
    EK_CHECK(diag_has(&f, 1, "body is [String]Int"));
    cfx_end(&f);

    cfx_begin(&f,
              "twice: (f: (Int) -> Int) -> Int = f(1)\n"
              "id: (s: String) -> String = s\n"
              "x: Int = twice(id)\n");
    EK_CHECK(diag_has(&f, 3, "argument is (String) -> String"));
    EK_CHECK(diag_has(&f, 3, "parameter is (Int) -> Int"));
    cfx_end(&f);

    cfx_begin(&f,
              "run: () !-> Int = (): Out.println(\"x\"); 1\n"
              "use: (f: () -> Int) -> Int = f()\n"
              "x: Int = use(run)\n");
    EK_CHECK(diag_has(&f, 3, "argument is () !-> Int"));
    EK_CHECK(diag_has(&f, 3, "parameter is () -> Int"));
    cfx_end(&f);
}

/* A type variable denotes a type, never a type constructor: an
   application headed by a variable is reported at the head, while the
   same application under a declared generic type stays legal. */
static void tvar_application(void)
{
    cfx f;

    cfx_begin(&f, "f: (x: t Int) -> Int = 0\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-apply", 1));
    cfx_end(&f);

    cfx_begin(&f,
              "Pair: type t u = [first: t, second: u]\n"
              "p: Pair Int String = {first: 1, second: \"a\"}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A type name that two modules declare prints with its module, so a
   message about the two reads as two distinct types; a name only one
   module declares stays bare. */
static void shared_names(void)
{
    cfx f;

    cfx_begin(&f,
              "Json: type = [text: String]\n"
              "j: Json = {text: \"x\"}\n"
              "k: std.Json = j\n");
    EK_CHECK(cfx_has_code(&f, "E-nominal-mismatch"));
    EK_CHECK(diag_has(&f, 3, "test.Json"));
    EK_CHECK(diag_has(&f, 3, "std.Json"));
    cfx_end(&f);

    cfx_begin(&f,
              "A: type = [x: Int]\n"
              "B: type = [x: Int]\n"
              "a: A = {x: 1}\n"
              "b: B = a\n");
    EK_CHECK(diag_has(&f, 4, "binding is A"));
    EK_CHECK(diag_has(&f, 4, "annotation is B"));
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
    type_names();
    tvar_application();
    shared_names();
    return ek_test_report("check/types");
}
