#include "common.h"

/* Expression forms of 6.4 to 6.9 with the precedence ladder of
   Clause 6.5.5, postfix and receiver chains, and lambdas. */

/* Parses `_t: Int = <expr>` and returns the right-hand side. */
static const ek_node *expr_of(parse_fixture *f, const char *rhs)
{
    char line[256];

    snprintf(line, sizeof line, "_t: Int = %s\n", rhs);
    pfx_begin(f, line);
    return pfx_item(f, 0)->u.bind.value;
}

static void literals_and_names(void)
{
    parse_fixture f;
    const ek_node *e;

    e = expr_of(&f, "42");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_INT_LIT);
    EK_CHECK_STR(e->u.int_lit.text, "42");
    pfx_end(&f);

    e = expr_of(&f, "1.5e3");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_FLOAT_LIT);
    pfx_end(&f);

    e = expr_of(&f, "'a'");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CHAR_LIT);
    EK_CHECK_U64(e->u.char_lit.scalar, 'a');
    pfx_end(&f);

    e = expr_of(&f, "Math.pi");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_FIELD);
    EK_CHECK_STR(e->u.field.name, "pi");
    EK_CHECK_U64(e->u.field.recv->kind, EK_N_NAME);
    pfx_end(&f);

    /* `_` in value position is the unit value (Clause 6.3.2). */
    e = expr_of(&f, "_");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_NAME);
    EK_CHECK_STR(e->u.path.data[0], "_");
    pfx_end(&f);
}

/* Ladder of Clause 6.5.5, tightest to loosest. */
static void precedence(void)
{
    parse_fixture f;
    const ek_node *e;

    /* mul over add */
    e = expr_of(&f, "1 + 2 * 3");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_BINARY);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_ADD);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_INT_LIT);
    EK_CHECK_U64(e->u.binary.rhs->kind, EK_N_BINARY);
    EK_CHECK_U64(e->u.binary.rhs->u.binary.op, EK_BIN_MUL);
    pfx_end(&f);

    /* left associativity */
    e = expr_of(&f, "1 + 2 + 3");
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_BINARY);
    EK_CHECK_U64(e->u.binary.rhs->kind, EK_N_INT_LIT);
    pfx_end(&f);

    /* unary minus binds over mul: `-a * b` is `(-a) * b` */
    e = expr_of(&f, "-a * b");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_MUL);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_UNARY);
    pfx_end(&f);

    /* postfix before unary: `-x.f()` parses as `-(x.f())` */
    e = expr_of(&f, "-x.f()");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_UNARY);
    EK_CHECK_U64(e->u.wrap.inner->kind, EK_N_CALL);
    pfx_end(&f);

    /* range below add, above comparison */
    e = expr_of(&f, "1 + 2..3 * 4");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_RANGE_EXC);
    EK_CHECK_U64(e->u.binary.lhs->u.binary.op, EK_BIN_ADD);
    pfx_end(&f);

    e = expr_of(&f, "1..=10");
    EK_CHECK_U64(e->u.binary.op, EK_BIN_RANGE_INC);
    pfx_end(&f);

    /* comparison above range */
    e = expr_of(&f, "1..2 == 3");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_EQ);
    EK_CHECK_U64(e->u.binary.lhs->u.binary.op, EK_BIN_RANGE_EXC);
    pfx_end(&f);

    /* && over || */
    e = expr_of(&f, "a || b && c");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_OR);
    EK_CHECK_U64(e->u.binary.rhs->u.binary.op, EK_BIN_AND);
    pfx_end(&f);

    /* concat is level 4 */
    e = expr_of(&f, "xs ++ ys ++ zs");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_CONCAT);
    pfx_end(&f);

    /* parentheses group */
    e = expr_of(&f, "(1 + 2) * 3");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_MUL);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_PAREN);
    pfx_end(&f);
}

static void calls_and_chains(void)
{
    parse_fixture f;
    const ek_node *e;

    /* prefix call */
    e = expr_of(&f, "f(a, b)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_STR(e->u.call.callee->u.path.data[0], "f");
    EK_CHECK_U64(e->u.call.args.count, 2);
    pfx_end(&f);

    /* receiver call and member chains (Clause 6.5.3) */
    e = expr_of(&f, "a.b(c).d");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_FIELD);
    EK_CHECK_STR(e->u.field.name, "d");
    EK_CHECK_U64(e->u.field.recv->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.field.recv->u.call.args.count, 1);
    pfx_end(&f);

    /* a chain may attach a module: a.Mod.f(b) (Clause 7.4.4) */
    e = expr_of(&f, "p.geolib.coord_sum()");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.call.callee->kind, EK_N_FIELD);
    EK_CHECK_STR(e->u.call.callee->u.field.name, "coord_sum");
    EK_CHECK_STR(e->u.call.callee->u.field.recv->u.field.name, "geolib");
    pfx_end(&f);

    /* constructor juxtaposition binds tighter than every operator */
    e = expr_of(&f, "Ok 1 + 2");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_ADD);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_APPLY);
    EK_CHECK_STR(e->u.binary.lhs->u.pair.a->u.path.data[0], "Ok");
    pfx_end(&f);

    e = expr_of(&f, "Ok x || y");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_OR);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_APPLY);
    pfx_end(&f);

    /* qualified constructor application Mod.Name x */
    e = expr_of(&f, "Mod.Ok x");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_APPLY);
    EK_CHECK_U64(e->u.pair.a->kind, EK_N_FIELD);
    pfx_end(&f);

    /* a composite literal argument */
    e = expr_of(&f, "Node {1, 2}");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_APPLY);
    EK_CHECK_U64(e->u.pair.b->kind, EK_N_COMPOSITE);
    EK_CHECK_U64(e->u.pair.b->u.kids.count, 2);
    pfx_end(&f);

    /* postfix on a literal (Clause 5.7.4) */
    e = expr_of(&f, "255.to_hex()");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.call.callee->kind, EK_N_FIELD);
    EK_CHECK_U64(e->u.call.callee->u.field.recv->kind, EK_N_INT_LIT);
    pfx_end(&f);
}

static void composite_literals(void)
{
    parse_fixture f;
    const ek_node *e;

    e = expr_of(&f, "{}");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_COMPOSITE);
    EK_CHECK_U64(e->u.kids.count, 0);
    pfx_end(&f);

    e = expr_of(&f, "{1, 2, 3}");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.kids.count, 3);
    EK_CHECK_U64(e->u.kids.data[0]->kind, EK_N_COMPOSITE_ITEM);
    EK_CHECK_U64(e->u.kids.data[0]->u.pair.a, NULL);
    pfx_end(&f);

    e = expr_of(&f, "{\"1\": 1, \"2\": 2}");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.kids.data[0]->u.pair.a->kind, EK_N_STRING_LIT);
    EK_CHECK_U64(e->u.kids.data[0]->u.pair.b->kind, EK_N_INT_LIT);
    pfx_end(&f);

    /* nested records (Clause 6.6.3) */
    e = expr_of(&f, "{address: {city: \"y\"}}");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.kids.data[0]->u.pair.b->kind, EK_N_COMPOSITE);
    pfx_end(&f);
}

static void lambdas(void)
{
    parse_fixture f;
    const ek_node *e;

    e = expr_of(&f, "(n): n + 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_LAMBDA);
    EK_CHECK_U64(e->u.pair.a->u.kids.count, 1);
    EK_CHECK_U64(e->u.pair.b->kind, EK_N_BINARY);
    pfx_end(&f);

    e = expr_of(&f, "(): _");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.pair.a->u.kids.count, 0);
    pfx_end(&f);

    /* a nested parameter group is a group of parameters (6.4.3) */
    e = expr_of(&f, "((a, b)): a");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.pair.a->kind, EK_N_LAMBDA_PARAMS);
    EK_CHECK_U64(e->u.pair.a->u.kids.data[0]->kind, EK_N_LAMBDA_PARAMS);
    pfx_end(&f);

    /* parenthesized expression, not a lambda */
    e = expr_of(&f, "(1 + 2)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_PAREN);
    pfx_end(&f);
}

static void operand_blocks(void)
{
    parse_fixture f;
    const ek_node *e;

    /* a case or if operand is parenthesized (Clause 6.5.1) */
    e = expr_of(&f, "1 + if c: 2 else: 3");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-operand-block"));
    pfx_end(&f);

    /* the whole argument is fine */
    e = expr_of(&f, "f(if c: 1 else: 2) + 0");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_ADD);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_CALL);
    pfx_end(&f);
}

int main(void)
{
    literals_and_names();
    precedence();
    calls_and_chains();
    composite_literals();
    lambdas();
    operand_blocks();
    return ek_test_report("parse/expr");
}
