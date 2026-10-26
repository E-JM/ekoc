#include "common.h"

/* Pattern typing and exhaustiveness — variants, records, tuples,
   or-patterns, guards, lists, multiple scrutinees — plus the
   irrefutability rule for value bindings. */

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

    /* A prefix-rest covers non-empty when its last element covers. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Bool =\n"
              "    case xs:\n"
              "        {.., 42} -> True\n"
              "        {} -> False\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
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

int main(void)
{
    variants();
    records_tuples();
    lists();
    guards_and_scrutinees();
    bindings();
    constructor_and_product_coverage();
    return ek_test_report("check/patterns");
}
