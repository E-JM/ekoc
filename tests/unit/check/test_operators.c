#include "common.h"

/* The operator grid — arithmetic, equality and ordering domains,
   logical operators, concatenation, ranges — with same-type operands
   and no implicit conversion. */

static void arithmetic(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Int = 7 / 2\n"
              "b: Int = -7 % 2\n"
              "c: Float = 1.5 * 2.0\n"
              "d: UInt = 1 + 2\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* `+` does not exist on String. */
    cfx_begin(&f, "a: String = \"a\" + \"b\"\n");
    EK_CHECK(cfx_has_code(&f, "E-no-operator"));
    cfx_end(&f);

    /* Mixed Int and Float operands in a synthesized position. */
    cfx_begin(&f,
              "n: Int = 3\n"
              "x: Float = n / 2.0\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);
}

static void equality_ordering(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Bool = 1 == 2\n"
              "b: Bool = \"a\" != \"b\"\n"
              "c: Bool = 'a' < 'b'\n"
              "d: Bool = \"ab\" < \"ac\"\n"
              "e: Bool = 1 <= 2 && 3 > 2\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Bool has no ordering. */
    cfx_begin(&f, "k: (b: Bool, c: Bool) -> Bool = b < c\n");
    EK_CHECK(cfx_has_code(&f, "E-no-operator"));
    cfx_end(&f);

    /* Equality off the value layer is the member `T.eq`. */
    cfx_begin(&f,
              "Human: type = [name: String]\n"
              "a: Human = {name: \"ada\"}\n"
              "b: Human = {name: \"ada\"}\n"
              "c: Bool = a == b\n");
    EK_CHECK(cfx_has_code(&f, "E-no-operator"));
    cfx_end(&f);

    /* `1 == 1.0`: both operands must have the same type. */
    cfx_begin(&f, "a: Bool = 1 == 1.0\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* A value whose static type is a type variable may use `==`. */
    cfx_begin(&f, "eq: (x: t, y: t) -> Bool = x == y\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void logical(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Bool = True && False\n"
              "b: Bool = True || False\n"
              "c: Bool = (1 < 2) && (3 < 4)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "a: Bool = 1 && 2\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

}

static void concat(void)
{
    cfx f;

    cfx_begin(&f,
              "s: String = \"ab\" ++ \"cd\"\n"
              "xs: []Int = {1} ++ {2}\n"
              "d: [String]Int = {\"a\": 1} ++ {\"b\": 2}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* Characters do not concatenate with strings. */
    cfx_begin(&f, "s: String = 'a' ++ \"bc\"\n");
    EK_CHECK(cfx_has_code(&f, "E-no-operator"));
    cfx_end(&f);

    /* Tuples and records do not support `++`. */
    cfx_begin(&f,
              "p: [Int, Int] = {1, 2}\n"
              "q: [Int, Int] = {3, 4}\n"
              "x: [Int, Int] = p ++ q\n");
    EK_CHECK(cfx_has_code(&f, "E-no-operator"));
    cfx_end(&f);

    /* List element types must agree. */
    cfx_begin(&f, "x: []Int = {1} ++ {\"a\"}\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    cfx_end(&f);

    /* Either operand may supply the other's literal expectation. */
    cfx_begin(&f,
              "h: (ys: []Int) -> []Int =\n"
              "    x := {1} ++ ys\n"
              "    x\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

static void ranges(void)
{
    cfx f;

    cfx_begin(&f,
              "a: Range Int = 0..10\n"
              "b: Range Int = 0..=10\n"
              "c: Range Char = 'a'..'c'\n"
              "d: Range UInt = 0 + 1..2\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A Float endpoint does not exist. */
    cfx_begin(&f, "a: Range Float = 0.0..1.0\n");
    EK_CHECK(cfx_has_code(&f, "E-no-operator"));
    cfx_end(&f);

    /* Endpoints must have the same type. */
    cfx_begin(&f, "a: Range Int = 0..'c'\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    cfx_end(&f);
}

/* Two determinate operand types unify or the operator is a mismatch,
   even when neither operand is a literal; a type-variable pair and a
   literal against its variable stay legal. */
static void operand_mismatch(void)
{
    cfx f;

    cfx_begin(&f,
              "f: (n: Int, m: Float) -> Bool =\n"
              "    s: Int = n + m\n"
              "    n.eq(1)\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 2));
    cfx_end(&f);

    cfx_begin(&f, "f: (n: Int, m: Float) -> Bool = n == m\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);

    cfx_begin(&f, "f: (n: Int, m: Float) -> Bool = n < m\n");
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);

    /* A type-variable pair keeps compiling, even at two distinct
       variables whose unification fails. */
    cfx_begin(&f,
              "eq: (x: t, y: t) -> Bool = x == y\n"
              "any_eq: (x: t, y: u) -> Bool = x == y\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* A literal operand is placed at the other operand's type. */
    cfx_begin(&f, "f: (n: Int) -> Int = n + 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    arithmetic();
    equality_ordering();
    logical();
    concat();
    ranges();
    operand_mismatch();
    return ek_test_report("check/operators");
}
