#include "common.h"

/* Expression forms with the full precedence ladder: postfix and
   receiver chains, and lambdas. */

/* Parses `_t: Int = <expr>` and returns the right-hand side, or NULL
   when the failed RHS takes the whole binding with it. */
static const ek_node *expr_of(parse_fixture *f, const char *rhs)
{
    char line[256];
    const ek_node *item;

    snprintf(line, sizeof line, "_t: Int = %s\n", rhs);
    pfx_begin(f, line);
    item = pfx_item(f, 0);
    return item && item->kind == EK_N_LET ? item->u.bind.value : NULL;
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

    /* `_` in value position is the unit value. */
    e = expr_of(&f, "_");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_NAME);
    EK_CHECK_STR(e->u.path.data[0], "_");
    pfx_end(&f);
}

/* The precedence ladder, tightest to loosest. */
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

    /* receiver call and member chains */
    e = expr_of(&f, "a.b(c).d");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_FIELD);
    EK_CHECK_STR(e->u.field.name, "d");
    EK_CHECK_U64(e->u.field.recv->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.field.recv->u.call.args.count, 1);
    pfx_end(&f);

    /* a chain may attach a module: a.Mod.f(b) */
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

    /* postfix on a literal */
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

    /* nested records */
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

    /* a nested parameter group is a group of parameters */
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

    /* a case or if operand must be parenthesized */
    e = expr_of(&f, "1 + if c: 2 else: 3");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-operand-block"));
    EK_CHECK(pfx_msg_has(&f, "must be parenthesized"));
    pfx_end(&f);

    /* the whole argument is fine */
    e = expr_of(&f, "f(if c: 1 else: 2) + 0");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_ADD);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_CALL);
    pfx_end(&f);
}

static void precedence_corners(void)
{
    parse_fixture f;
    const ek_node *e;

    /* Unary minus never chains. */
    (void)expr_of(&f, "- -x");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    /* A binary minus feeds a unary operand: `a - -b` is `a - (-b)`. */
    e = expr_of(&f, "a - -b");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_SUB);
    EK_CHECK_U64(e->u.binary.rhs->kind, EK_N_UNARY);
    pfx_end(&f);

    /* Range chains are diagnosed. */
    (void)expr_of(&f, "1..2..3");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-chained-range"));
    pfx_end(&f);

    /* Comparison over a range folds without error. */
    e = expr_of(&f, "a < b..c");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_LT);
    EK_CHECK_U64(e->u.binary.rhs->u.binary.op, EK_BIN_RANGE_EXC);
    pfx_end(&f);

    /* The juxtaposition argument is one postfix expression, so the
       chain under it binds first. */
    e = expr_of(&f, "Ok x.f()");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_APPLY);
    EK_CHECK_U64(e->u.pair.b->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.pair.b->u.call.callee->kind, EK_N_FIELD);
    pfx_end(&f);

    e = expr_of(&f, "Succ (Succ Zero)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_APPLY);
    EK_CHECK_U64(e->u.pair.b->kind, EK_N_PAREN);
    EK_CHECK_U64(e->u.pair.b->u.wrap.inner->kind, EK_N_APPLY);
    pfx_end(&f);

    /* The juxtaposed argument is an operand position: a `case` or
       `if` there must be parenthesized. */
    e = expr_of(&f, "Ok if c: 1 else: 2");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "6.5.1"));
    EK_CHECK_U64(e->kind, EK_N_APPLY);
    pfx_end(&f);

    e = expr_of(&f, "Ok (if c: 1 else: 2)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_APPLY);
    EK_CHECK_U64(e->u.pair.b->kind, EK_N_PAREN);
    pfx_end(&f);
}

static void lambda_shapes(void)
{
    parse_fixture f;
    const ek_node *e;

    /* Several parameters, flat and grouped. */
    e = expr_of(&f, "(a, b, c): a");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.pair.a->u.kids.count, 3);
    pfx_end(&f);

    /* Lambda parameters carry no types; `(a: Int)` is not a group. */
    (void)expr_of(&f, "(a: Int): a");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    /* An anonymous parameter. */
    e = expr_of(&f, "(_): 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_STR(e->u.pair.a->u.kids.data[0]->u.path.data[0], "_");
    pfx_end(&f);

    /* A lambda body may be a `;` suite. */
    e = expr_of(&f, "(n): a(); n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.pair.b->kind, EK_N_BLOCK);
    EK_CHECK_U64(e->u.pair.b->u.kids.count, 2);
    pfx_end(&f);
}

/* The record-update form: a postfix member call whose one argument is
   a composite literal (annex A.3.5 Postfix; Clauses 6.6.3, 9.8.6).
   There is no `with` operator in the ladder; the form sits at level 1
   with every other postfix link. */
static void with_updates(void)
{
    parse_fixture f;
    const ek_node *e;

    e = expr_of(&f, "p.with({age: 37})");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.call.callee->kind, EK_N_FIELD);
    EK_CHECK_STR(e->u.call.callee->u.field.name, "with");
    EK_CHECK_STR(e->u.call.callee->u.field.recv->u.path.data[0], "p");
    EK_CHECK_U64(e->u.call.args.count, 1);
    EK_CHECK_U64(e->u.call.args.data[0]->kind, EK_N_COMPOSITE);
    EK_CHECK_U64(e->u.call.args.data[0]->u.kids.count, 1);
    pfx_end(&f);

    /* The update is one postfix link: field access composes on it, and
       the operators bind around the whole chain. */
    e = expr_of(&f, "p.with({age: 37}).age + 1");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_ADD);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_FIELD);
    EK_CHECK_STR(e->u.binary.lhs->u.field.name, "age");
    EK_CHECK_U64(e->u.binary.lhs->u.field.recv->kind, EK_N_CALL);
    pfx_end(&f);

    e = expr_of(&f, "p.with({age: 37}).age == 36");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_EQ);
    EK_CHECK_U64(e->u.binary.lhs->kind, EK_N_FIELD);
    pfx_end(&f);

    e = expr_of(&f, "p.with({age: 37}).age > 0 && q.ok");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.binary.op, EK_BIN_AND);
    EK_CHECK_U64(e->u.binary.lhs->u.binary.op, EK_BIN_GT);
    pfx_end(&f);

    /* an update whose field value is itself a literal */
    e = expr_of(&f, "p.with({address: {city: \"cd\"}})");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.call.args.data[0]->u.kids.data[0]->u.pair.b->kind,
                 EK_N_COMPOSITE);
    pfx_end(&f);

    /* breaks inside the argument list are free formatting */
    e = expr_of(&f, "p.with(\n        {age: 37})");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.call.args.count, 1);
    pfx_end(&f);

    e = expr_of(&f, "p.with({\n        age: 37})");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->kind, EK_N_CALL);
    EK_CHECK_U64(e->u.call.args.data[0]->u.kids.count, 1);
    pfx_end(&f);

    /* The argument grammar is CompositeItem, so a rest element is no
       expression and the braces are rejected here; a shorthand item is
       an element, which only check can reject against the record. */
    (void)expr_of(&f, "p.with({..})");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    e = expr_of(&f, "p.with({age})");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(e->u.call.args.data[0]->u.kids.data[0]->u.pair.a, NULL);
    pfx_end(&f);
}

/* The operand-position lambda rule: `( parameters ) :` in operand
   position is a grouping followed by the header colon, never a
   lambda; statement, argument, and callback positions keep the
   unparenthesized form. */
static void operand_lambdas(void)
{
    parse_fixture f;

    /* A call argument reads the lambda form. */
    pfx_begin(&f, "_t: Int = xs.map((n): n * 2)\n");
    pfx_item(&f, 0);
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    /* A statement opens with the lambda form. */
    pfx_begin(&f, "_run: (xs: []Int) -> Int =\n    (n): n + 1\n");
    pfx_item(&f, 0);
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    /* A binary operand before a header colon is a grouping. */
    pfx_begin(&f,
              "_f: (a: Bool, b: Bool) -> Int =\n"
              "    if (a) || (b): 1 else: 2\n");
    pfx_item(&f, 0);
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    /* The whole condition, parenthesized, keeps the lambda reading
       inside its brackets. */
    pfx_begin(&f,
              "_f: (a: Bool) -> Bool =\n"
              "    if ((a) || pick()): a else: a\n");
    pfx_item(&f, 0);
    pfx_end(&f);

    /* An operand lambda without its parentheses is rejected. */
    pfx_begin(&f, "_t: Int =\n    1 + (n): 2\n");
    pfx_item(&f, 0);
    EK_CHECK(pfx_errors(&f) >= 1);
    pfx_end(&f);
}


/* A header expression whose first bracket group is followed by the
   header colon: the group is a parenthesized condition, and the
   lambda reading is suppressed. */
static void header_paren_conditions(void)
{
    parse_fixture f;

    pfx_begin(&f,
              "_g: (b: Bool) -> Int =\n"
              "    if (b): 1 else: 2\n");
    pfx_item(&f, 0);
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    pfx_begin(&f,
              "_k: (b: Bool) -> String =\n"
              "    case (b):\n"
              "        True -> \"t\"\n"
              "        False -> \"f\"\n");
    pfx_item(&f, 0);
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);

    /* A nested lambda inside the group keeps its reading. */
    pfx_begin(&f,
              "_m: (xs: []Int) -> Int =\n"
              "    if (xs.map((n): n).length() == 0): 0 else: 1\n");
    pfx_item(&f, 0);
    EK_CHECK_U64(pfx_errors(&f), 0);
    pfx_end(&f);
}

int main(void)
{
    literals_and_names();
    precedence();
    precedence_corners();
    calls_and_chains();
    composite_literals();
    with_updates();
    lambdas();
    lambda_shapes();
    operand_blocks();
    operand_lambdas();
    header_paren_conditions();
    return ek_test_report("parse/expr");
}
