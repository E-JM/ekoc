#include "common.h"

/* Exhaustiveness by column specialization: shape columns inside a
   product, or-pattern alternatives that split inside one column,
   fixed-length and rest list patterns, and dynamically sized payload
   obligations. */

static void shape_columns(void)
{
    cfx f;

    /* A list column and an algebraic column are products like any
       other: the missing ([], None) pair is a hole even though each
       column's patterns look non-empty. */
    cfx_begin(&f,
              "f: (xs: []Int, o: Option Int) -> Int =\n"
              "    case xs, o:\n"
              "        {h, ..t}, Some v -> v\n"
              "        {h2, ..t2}, None -> h2\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    /* A tuple column and a scalar column likewise. */
    cfx_begin(&f,
              "f: (p: [Int, Int], b: Bool) -> Int =\n"
              "    case p, b:\n"
              "        {0, _}, True -> 1\n"
              "        {1, _}, False -> 0\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    /* The product is exhaustive once every combination is named. */
    cfx_begin(&f,
              "f: (p: [Int, Int], b: Bool) -> Int =\n"
              "    case p, b:\n"
              "        {0, _}, True -> 1\n"
              "        {0, _}, False -> 2\n"
              "        {1, _}, True -> 3\n"
              "        {1, _}, False -> 4\n"
              "        _, _ -> 0\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void or_alternatives(void)
{
    cfx f;

    /* Two record alternatives that each name one field cover the
       field's type between them. */
    cfx_begin(&f,
              "R: type = [b: Bool, n: Int]\n"
              "f: (r: R) -> Int =\n"
              "    case r:\n"
              "        {b: True} | {b: False} -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* An or-pattern nested in a field splits the same way. */
    cfx_begin(&f,
              "R: type = [b: Bool, n: Int]\n"
              "f: (r: R) -> Int =\n"
              "    case r:\n"
              "        {b: True | False} -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Two payload alternatives covering one constructor count as two
       coverage rows; the payload catch-all completes the type. */
    cfx_begin(&f,
              "O: type = None | Some Int\n"
              "f: (o: O) -> Int =\n"
              "    case o:\n"
              "        Some 0 | Some 1 -> 1\n"
              "        Some _ -> 2\n"
              "        None -> 0\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Without the payload catch-all the other ints stay open. */
    cfx_begin(&f,
              "O: type = None | Some Int\n"
              "f: (o: O) -> Int =\n"
              "    case o:\n"
              "        Some 0 | Some 1 -> 1\n"
              "        None -> 0\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 3));
    cfx_end(&f);
}

static void list_lengths(void)
{
    cfx f;

    /* A fixed-length pattern and a catch-all: the catch-all answers
       the empty list and every other length. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Int =\n"
              "    case xs:\n"
              "        {x, y} -> x + y\n"
              "        _ -> 0\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A fixed length alone leaves longer lists open. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Int =\n"
              "    case xs:\n"
              "        {} -> 0\n"
              "        {x, y} -> x + y\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    /* A suffix rest covers only the lists whose head it matches: a
       literal head leaves the other heads open. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Int =\n"
              "    case xs:\n"
              "        {} -> 0\n"
              "        {5, ..t} -> 5\n");
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 2));
    cfx_end(&f);

    /* A variable head covers every head, so the rest completes the
       non-empty lists. */
    cfx_begin(&f,
              "f: (xs: []Int) -> Int =\n"
              "    case xs:\n"
              "        {} -> 0\n"
              "        {_, ..} -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A payload obligation is not capped: a twenty-component tuple
   payload is judged in full. */
static void wide_payload(void)
{
    cfx f;
    char buf[2048];
    size_t o = 0;
    int i;

    o += (size_t)snprintf(buf + o, sizeof buf - o,
                          "T: type = Mk [");
    for (i = 0; i < 20; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "%sInt",
                              i ? ", " : "");
    }
    o += (size_t)snprintf(buf + o, sizeof buf - o,
                          "]\nf: (t: T) -> Int =\n    case t:\n        Mk {");
    for (i = 0; i < 20; i++) {
        o += (size_t)snprintf(buf + o, sizeof buf - o, "%s_",
                              i ? ", " : "");
    }
    snprintf(buf + o, sizeof buf - o, "} -> 1\n");
    cfx_begin(&f, buf);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    shape_columns();
    or_alternatives();
    list_lengths();
    wide_payload();
    return ek_test_report("check/exhaust");
}
