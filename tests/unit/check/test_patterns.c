#include "common.h"

/* Pattern typing and exhaustiveness — variants, records, tuples,
   or-patterns, guards, lists, multiple scrutinees, the together-rule
   for one arm's patterns — plus the irrefutability rule for value
   bindings. */

static void variants(void)
{
    cfx f;

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "label: (s: Sex) -> String =\n"
              "    case s:\n"
              "        Man -> \"m\"\n"
              "        Woman -> \"w\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "label: (s: Sex) -> String =\n"
              "    case s:\n"
              "        Man -> \"m\"\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    /* A final `_` arm covers the rest. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "label: (s: Sex) -> String =\n"
              "    case s:\n"
              "        Man -> \"m\"\n"
              "        _ -> \"w\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Or-patterns cover what their alternatives cover. */
    cfx_begin(&f,
              "Key: type = Up | Down | Left | Right\n"
              "axis: (k: Key) -> String =\n"
              "    case k:\n"
              "        Up | Down -> \"vertical\"\n"
              "        Left | Right -> \"horizontal\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "Key: type = Up | Down | Left | Right\n"
              "axis: (k: Key) -> String =\n"
              "    case k:\n"
              "        Up | Down -> \"vertical\"\n"
              "        Left -> \"h\"\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    /* Payload coverage is recursive: a constructor payload must
       cover its payload type. */
    cfx_begin(&f,
              "Wrap: type = One Int | Two Int\n"
              "f: (w: Wrap) -> Int =\n"
              "    case w:\n"
              "        One 0 -> 0\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);
}

static void records_tuples(void)
{
    cfx f;

    /* A record pattern alone covers a record type. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "name: (h: Human) -> String =\n"
              "    case h:\n"
              "        {name} -> name\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A misspelled field is an error. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "name: (h: Human) -> String =\n"
              "    case h:\n"
              "        {nane} -> nane\n");
    EK_CHECK(cfx_has_code(&f, "E-field-missing"));
    cfx_end(&f);

    /* A tuple pattern needs the scrutinee's arity; components must
       cover. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "f: (p: [Sex, Int]) -> Int =\n"
              "    case p:\n"
              "        {Man, n} -> n\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "f: (p: [Sex, Int]) -> Int =\n"
              "    case p:\n"
              "        {Man, n} -> n\n"
              "        {Woman, n} -> n\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "f: (p: [Int, Int]) -> Int =\n"
              "    case p:\n"
              "        {a} -> a\n"
              "        _ -> 0\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);
}

static void lists(void)
{
    cfx f;

    cfx_begin(&f,
              "first: (xs: []t) -> Option t =\n"
              "    case xs:\n"
              "        {x, ..} -> Some x\n"
              "        {} -> None\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The empty list and the non-empty lists must both be covered. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Int =\n"
              "    case xs:\n"
              "        {x, y} -> x\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    /* A prefix-rest covers non-empty when its last element covers;
       a literal names one value of the element type and does not. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Int =\n"
              "    case xs:\n"
              "        {.., last} -> last\n"
              "        {} -> 0\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "f: (xs: []Int) -> Bool =\n"
              "    case xs:\n"
              "        {.., 42} -> True\n"
              "        {} -> False\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);
}

static void guards_and_scrutinees(void)
{
    cfx f;

    /* A guarded arm contributes no coverage, but a final unguarded
       arm completes the case. */
    cfx_begin(&f,
              "sign: (n: Int) -> String =\n"
              "    case n:\n"
              "        n if n < 0 -> \"neg\"\n"
              "        _ -> \"pos\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A guarded case must end with an unguarded arm. */
    cfx_begin(&f,
              "sign: (n: Int) -> String =\n"
              "    case n:\n"
              "        _ if n < 0 -> \"neg\"\n"
              "        n if n == 0 -> \"zero\"\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    /* Several scrutinees: the product must be covered. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "f: (a: Sex, b: Sex) -> String =\n"
              "    case a, b:\n"
              "        Man, Man -> \"mm\"\n"
              "        Man, Woman -> \"mw\"\n"
              "        Woman, Man -> \"wm\"\n"
              "        Woman, Woman -> \"ww\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "f: (a: Sex, b: Sex) -> String =\n"
              "    case a, b:\n"
              "        Man, Man -> \"mm\"\n"
              "        Woman, Woman -> \"ww\"\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    /* Every arm has the same type. */
    cfx_begin(&f,
              "f: (n: Int) -> String =\n"
              "    case n:\n"
              "        0 -> \"zero\"\n"
              "        _ -> 1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);
}

/* A case arm body may be a block: its bindings scope over the rest of
   the block, the last member carries the arm's value and the expected
   type, and coverage counts the arm the same way either shape does. */
static void arm_body_blocks(void)
{
    cfx f;

    cfx_begin(&f,
              "Outcome: type = [count: Int, note: String]\n"
              "route: (cmd: String) -> Result Outcome String =\n"
              "    case cmd:\n"
              "        \"list\" ->\n"
              "            n: Int = 2\n"
              "            Ok {count: n, note: \"ok\"}\n"
              "        _ -> Err \"unknown\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_bits_filled(&f));
    cfx_end(&f);

    /* An aligned `if`/`else` as the arm body's only member keeps the
       arm's value. */
    cfx_begin(&f,
              "f: (n: Int) -> Int =\n"
              "    case n:\n"
              "        0 ->\n"
              "            if n < 0:\n"
              "                -1\n"
              "            else:\n"
              "                1\n"
              "        _ -> 2\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The last member decides the arm's type, so a block arm whose
       value disagrees with the other arms reports once at the case. */
    cfx_begin(&f,
              "f: (n: Int) -> String =\n"
              "    case n:\n"
              "        0 ->\n"
              "            s: String = \"zero\"\n"
              "            s\n"
              "        _ -> 1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);

    /* A block arm covers its patterns like an inline one: the two
       constructors complete the case. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "label: (s: Sex) -> String =\n"
              "    case s:\n"
              "        Man ->\n"
              "            \"m\"\n"
              "        Woman -> \"w\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "label: (s: Sex) -> String =\n"
              "    case s:\n"
              "        Man ->\n"
              "            \"m\"\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    /* A block arm body is its own scope: a binding made there is not
       visible to the next arm. */
    cfx_begin(&f,
              "f: (n: Int) -> Int =\n"
              "    case n:\n"
              "        0 ->\n"
              "            k: Int = 1\n"
              "            k\n"
              "        _ -> k\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 1);
    EK_CHECK(cfx_slice_has(f.ds, f.dn, "E-unbound-name"));
    cfx_end(&f);
}

/* One arm of a multi-scrutinee case binds together: a name that two of
   the arm's patterns bind is a conflict at the later occurrence, an
   or-pattern's set counts whole, distinct names pass, different arms
   reuse freely, and twice inside one pattern stays the resolver's. */
static void cross_pattern(void)
{
    cfx f;
    size_t i;
    int hit;

    cfx_begin(&f,
              "pair: (p: [Int, Int], q: [Int, Int]) -> Int =\n"
              "    case p, q:\n"
              "        {x, _}, {x, _} -> x\n");
    EK_CHECK(cfx_code_at_line(&f, "E-name-conflict", 3));
    cfx_end(&f);

    /* Distinct names across the arm's patterns bind independently. */
    cfx_begin(&f,
              "pair: (p: [Int, Int], q: [Int, Int]) -> Int =\n"
              "    case p, q:\n"
              "        {x, _}, {y, _} -> x + y\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Different arms may reuse the same names. */
    cfx_begin(&f,
              "pair: (p: [Int, Int], q: [Int, Int]) -> Int =\n"
              "    case p, q:\n"
              "        {x, _}, {y, _} -> x + y\n"
              "        {x, _}, {y, _} -> x * y\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The or-pattern's set is whole: its `x` meets `n` as distinct,
       another `x` as a conflict. */
    cfx_begin(&f,
              "B: type = Box Int | Mark Int\n"
              "unbox: (b: B, n: Int) -> Int =\n"
              "    case b, n:\n"
              "        Box x | Mark x, n -> x + n\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "B: type = Box Int | Mark Int\n"
              "unbox: (b: B, n: Int) -> Int =\n"
              "    case b, n:\n"
              "        Box x | Mark x, x -> x\n");
    EK_CHECK(cfx_code_at_line(&f, "E-name-conflict", 4));
    cfx_end(&f);

    /* Twice inside one pattern is the resolver's rejection, and the
       single-pattern arm draws no check-side conflict. */
    cfx_begin(&f,
              "pair: (p: [Int, Int]) -> Int =\n"
              "    case p:\n"
              "        {x, x} -> x\n");
    hit = 0;
    for (i = 0; i < f.dn; i++) {
        if (f.ds[i].code && strcmp(f.ds[i].code, "E-name-conflict") == 0
            && f.ds[i].line == 3) {
            hit = 1;
        }
    }
    EK_CHECK(hit);
    EK_CHECK(!cfx_has_code(&f, "E-name-conflict"));
    cfx_end(&f);
}

static void bindings(void)
{
    cfx f;

    /* Irrefutable bindings: `_`, variables, tuple groups, record
       subsets. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "read: () -> Int = (): 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "pair: [Int, Int] = {1, 2}\n"
              "(a, b) = pair\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A list pattern in a value binding is refutable. */
    cfx_begin(&f, "xs: []Int = {1}\n"
                  "{h, ..t} = xs\n");
    EK_CHECK(cfx_code_at_line(&f, "E-refutable-binding", 2));
    cfx_end(&f);

    /* Or-patterns occur only in case arms. */
    cfx_begin(&f, "xs: []Int = {1}\n"
                  "(a | b) = xs\n");
    EK_CHECK(cfx_code_at_line(&f, "E-refutable-binding", 2));
    cfx_end(&f);

    /* `<-` admits parameter-position patterns only. */
    cfx_begin(&f,
              "disp: (cb: (Int) -> Int) -> Int = cb(1)\n"
              "{h, ..t} <- disp((n): n)\n");
    EK_CHECK(cfx_code_at_line(&f, "E-refutable-binding", 2));
    cfx_end(&f);

    /* A literal pattern parses and is rejected at the binding. */
    cfx_begin(&f, "s: String = \"go\"\n"
                  "\"go\" = s\n");
    EK_CHECK(cfx_code_at_line(&f, "E-refutable-binding", 2));
    cfx_end(&f);

    cfx_begin(&f, "n: Int = -1\n"
                  "-1 = n\n");
    EK_CHECK(cfx_code_at_line(&f, "E-refutable-binding", 2));
    cfx_end(&f);

    /* A constructor pattern with its payload parses and is rejected at
       the binding. */
    cfx_begin(&f,
              "r: Result Int String = Ok 1\n"
              "Ok v = r\n");
    EK_CHECK(cfx_code_at_line(&f, "E-refutable-binding", 2));
    cfx_end(&f);

    /* The annotated group binding keeps its check-stage route. */
    cfx_begin(&f,
              "row: [Int, Int] = {1, 2}\n"
              "(a, b): [Int, Int] = row\n"
              "s: Int = a + b\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* One wide multi-scrutinee case over a two-constructor ADT: mode 0 is
   the catch-all arm, mode 1 names the first constructor everywhere, and
   mode 2 spans both constructors with an or-pattern. */
static void wide_case(char *buf, size_t cap, int cols, int mode)
{
    size_t o = 0;
    int i;

    o += (size_t)snprintf(buf + o, cap - o, "B: type = L | R\nf: (");
    for (i = 0; i < cols; i++) {
        o += (size_t)snprintf(buf + o, cap - o, "x%d: B%s", i,
                              i == cols - 1 ? ") -> _ = " : ", ");
    }
    o += (size_t)snprintf(buf + o, cap - o, "\n    case ");
    for (i = 0; i < cols; i++) {
        o += (size_t)snprintf(buf + o, cap - o, "%sx%d", i ? ", " : "", i);
    }
    o += (size_t)snprintf(buf + o, cap - o, ":\n        ");
    for (i = 0; i < cols; i++) {
        const char *pat = mode == 1 ? "L" : mode == 2 ? "L | R" : "_";

        o += (size_t)snprintf(buf + o, cap - o, "%s%s", i ? ", " : "", pat);
    }
    snprintf(buf + o, cap - o, " -> _\n");
}

/* Constructor coverage at the top level and the wide product: a
   constructor pattern never acts as a catch-all, and the product
   judgment terminates over many small scrutinees. */
static void constructor_and_product_coverage(void)
{
    cfx f;
    char buf[8192];

    cfx_begin(&f,
              "P: type = Mk Int | End\n"
              "g: (p: P) -> Int =\n"
              "    case p:\n"
              "        Mk x -> x\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "P: type = Mk Int | End\n"
              "g: (p: P) -> Int =\n"
              "    case p:\n"
              "        Mk _ -> 1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "P: type = Mk Int | End\n"
              "g: (p: P) -> Int =\n"
              "    case p:\n"
              "        Mk x -> x\n"
              "        End -> 0\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    wide_case(buf, sizeof buf, 28, 0);
    cfx_begin(&f, buf);
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_rows_sorted(&f));
    EK_CHECK(cfx_bits_filled(&f));
    cfx_end(&f);

    wide_case(buf, sizeof buf, 28, 1);
    cfx_begin(&f, buf);
    EK_CHECK(cfx_has_code(&f, "E-exhaust"));
    cfx_end(&f);

    wide_case(buf, sizeof buf, 28, 2);
    cfx_begin(&f, buf);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A rejected or-alternative still binds its payload names, so the arm
   reports the constructor mismatch alone instead of a second missing
   binding; a genuine bind-set difference still reports. */
static void rejected_alternative(void)
{
    cfx f;

    cfx_begin(&f,
              "A: type = Only Int\n"
              "B: type = Both Int\n"
              "f: (b: B) -> Int =\n"
              "    case b:\n"
              "        Both n | Only n -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 5));
    EK_CHECK(!cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* A different name set is a second, independent defect, and the
       payload-less alternative also misses the constructor's
       payload. */
    cfx_begin(&f,
              "A: type = Both Int\n"
              "f: (b: A) -> Int =\n"
              "    case b:\n"
              "        Both n | Both -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 4));
    cfx_end(&f);
}

/* The arms headed by one constructor cover its payload jointly: a
   nested ADT may be split across arms. */
static void nested_payloads(void)
{
    cfx f;

    cfx_begin(&f,
              "A: type = X | Y\n"
              "R: type = Wrap A | Nope Int\n"
              "f: (r: R) -> Int =\n"
              "    case r:\n"
              "        Nope n -> n\n"
              "        Wrap X -> 1\n"
              "        Wrap Y -> 2\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "A: type = X | Y\n"
              "R: type = Wrap A | Nope Int\n"
              "f: (r: R) -> Int =\n"
              "    case r:\n"
              "        Nope n -> n\n"
              "        Wrap X -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 4));
    cfx_end(&f);

    /* A wildcard payload covers the whole payload. */
    cfx_begin(&f,
              "A: type = X | Y\n"
              "R: type = Wrap A | Nope Int\n"
              "f: (r: R) -> Int =\n"
              "    case r:\n"
              "        Wrap _ -> 1\n"
              "        Nope n -> n\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* Coverage at depth: guards contribute nothing, constructor payloads
   are judged inside the product, and a scrutinee without case
   structure is covered only by a catch-all. */
static void depth_and_scalar_coverage(void)
{
    cfx f;

    /* A guarded arm covers nothing: Man rides only on its unguarded
       arm. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "f: (s: Sex) -> Int =\n"
              "    case s:\n"
              "        Man if True -> 1\n"
              "        Woman -> 2\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "f: (s: Sex, g: Bool) -> Int =\n"
              "    case s:\n"
              "        Man if g -> 1\n"
              "        Man -> 2\n"
              "        Woman -> 3\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The payload of a constructor column is judged per split: the
       True rows answer `Some None` only. */
    cfx_begin(&f,
              "f: (o: Option (Option Int), b: Bool) -> Int =\n"
              "    case o, b:\n"
              "        Some None, True -> 1\n"
              "        Some _, False -> 2\n"
              "        None, _ -> 3\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "f: (o: Option (Option Int), b: Bool) -> Int =\n"
              "    case o, b:\n"
              "        Some None, True -> 1\n"
              "        Some (Some v), _ -> v\n"
              "        Some None, False -> 2\n"
              "        None, _ -> 3\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A scalar scrutinee is covered only by a variable or `_`. */
    cfx_begin(&f,
              "f: (n: Int) -> String =\n"
              "    case n:\n"
              "        0 -> \"zero\"\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "f: (n: Int) -> String =\n"
              "    case n:\n"
              "        0 -> \"zero\"\n"
              "        _ -> \"many\"\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Scalar columns split over their literals; everything else wants
       a catch-all row. */
    cfx_begin(&f,
              "f: (n: Int, b: Bool) -> Int =\n"
              "    case n, b:\n"
              "        0, True -> 1\n"
              "        _, False -> 2\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "f: (n: Int, b: Bool) -> Int =\n"
              "    case n, b:\n"
              "        0, True -> 1\n"
              "        _, True -> 2\n"
              "        _, False -> 3\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A string or character literal pattern sits at the scrutinee type:
   against a scrutinee of any other visible type it is a mismatch at
   the literal, while the matching std type and the numeric literals
   stay as they were. */
static void literal_scrutinee(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (n: Int) -> Int =\n"
              "    case n:\n"
              "        \"a\" -> 0\n"
              "        _ -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "f: (c: Char) -> Int =\n"
              "    case c:\n"
              "        \"ab\" -> 0\n"
              "        _ -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "f: (n: Int) -> Int =\n"
              "    case n:\n"
              "        \'a\' -> 0\n"
              "        _ -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 3));
    cfx_end(&f);

    /* A literal at its own type, in both directions, stays clean. */
    cfx_begin(&f,
              "f: (s: String) -> Int =\n"
              "    case s:\n"
              "        \"a\" -> 0\n"
              "        _ -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "f: (c: Char) -> Int =\n"
              "    case c:\n"
              "        \'a\' -> 0\n"
              "        _ -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A constructor pattern carries a payload pattern exactly when the
   constructor has one: a payload written for a zero-payload
   constructor and a missing payload for a payload-carrying one are
   mismatches, and a bare arm does not cover its constructor. */
static void payload_arity(void)
{
    cfx f;

    cfx_begin(&f,
              "O: type = None | Some Int\n"
              "f: (o: O) -> Int =\n"
              "    case o:\n"
              "        None x -> 0\n"
              "        Some n -> n\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 4));
    cfx_end(&f);

    cfx_begin(&f,
              "O: type = None | Some Int\n"
              "f: (o: O) -> Int =\n"
              "    case o:\n"
              "        None -> 0\n"
              "        Some -> 1\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 5));
    cfx_end(&f);

    /* A bare arm over a payload-carrying constructor covers nothing:
       the case stays non-exhaustive. */
    cfx_begin(&f,
              "O: type = None | Some Int\n"
              "f: (o: O) -> Int =\n"
              "    case o:\n"
              "        Some -> 1\n"
              "        None -> 0\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);

    /* The zero-payload form stays legal and complete. */
    cfx_begin(&f,
              "O: type = None | Some Int\n"
              "f: (o: O) -> Int =\n"
              "    case o:\n"
              "        None -> 0\n"
              "        Some n -> n\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    variants();
    records_tuples();
    lists();
    guards_and_scrutinees();
    arm_body_blocks();
    cross_pattern();
    bindings();
    constructor_and_product_coverage();
    nested_payloads();
    rejected_alternative();
    depth_and_scalar_coverage();
    literal_scrutinee();
    payload_arity();
    return ek_test_report("check/patterns");
}
