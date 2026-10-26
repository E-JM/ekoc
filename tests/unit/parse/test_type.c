#include "common.h"

/* Type expressions of Clause 6.9: atoms, the bracket forms, arrows of
   6.9.3, and generic application of 6.9.4. */

/* Parses `x: <type> = 1` and returns the annotation. */
static const ek_node *ann_of(parse_fixture *f, const char *ty)
{
    char src[256];

    snprintf(src, sizeof src, "x: %s = 1\n", ty);
    pfx_begin(f, src);
    return pfx_ann(f);
}

static void atoms(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "Person");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_NAME);
    EK_CHECK_STR(t->u.path.data[0], "Person");
    pfx_end(&f);

    /* A module-qualified type (Clause 6.9.1). */
    t = ann_of(&f, "person.Person");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.path.count, 2);
    pfx_end(&f);

    /* A bare lowercase name is a type variable. */
    t = ann_of(&f, "elem");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_NAME);
    EK_CHECK_STR(t->u.path.data[0], "elem");
    pfx_end(&f);

    /* `_` is the unit type (Clause 6.9.1). */
    t = ann_of(&f, "_");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_STR(t->u.path.data[0], "_");
    pfx_end(&f);

    /* A group in complete-type position is a parameter group
       (Clause 6.9.3); a grouped type atom needs an atom position. */
    t = ann_of(&f, "([]Int)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_FN);
    EK_CHECK_U64(t->u.t_fn.params.count, 1);
    EK_CHECK_U64(t->u.t_fn.result, NULL);
    pfx_end(&f);

    t = ann_of(&f, "Option ([]Int)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_apply.args.data[0]->kind, EK_N_T_PAREN);
    pfx_end(&f);
}

static void brackets(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "[]Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_LIST);
    EK_CHECK_U64(t->u.wrap.inner->kind, EK_N_T_NAME);
    pfx_end(&f);

    t = ann_of(&f, "[][]Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_LIST);
    EK_CHECK_U64(t->u.wrap.inner->kind, EK_N_T_LIST);
    pfx_end(&f);

    t = ann_of(&f, "[String]Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_DICT);
    EK_CHECK_STR(t->u.pair.a->u.path.data[0], "String");
    pfx_end(&f);

    /* A dictionary keyed by lists (Clause 6.9.2). */
    t = ann_of(&f, "[[]String]Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_DICT);
    EK_CHECK_U64(t->u.pair.a->kind, EK_N_T_LIST);
    pfx_end(&f);

    t = ann_of(&f, "[String, Int]");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_TUPLE);
    EK_CHECK_U64(t->u.kids.count, 2);
    pfx_end(&f);

    t = ann_of(&f, "[name: String, age: UInt]");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_RECORD);
    EK_CHECK_U64(t->u.kids.count, 2);
    EK_CHECK_STR(t->u.kids.data[0]->u.t_field.name, "name");
    pfx_end(&f);

    /* A one-element bracket is not a valid type (Clause 6.9.2). */
    t = ann_of(&f, "[Int]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    /* A field is `name :`; junk after a comma recovers at `]` without
       being eaten as a field name. */
    t = ann_of(&f, "[a: Int, 5]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "expected a field in the record type"));
    pfx_end(&f);

    t = ann_of(&f, "[a: Int, ]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    /* A dictionary value may be an arrow (Clause 6.9.2). */
    t = ann_of(&f, "[String](Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_DICT);
    EK_CHECK_U64(t->u.pair.b->kind, EK_N_T_FN);
    pfx_end(&f);
}

static void arrows(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "(Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_FN);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_PURE);
    EK_CHECK_U64(t->u.t_fn.params.count, 1);
    EK_CHECK(t->u.t_fn.result != NULL);
    pfx_end(&f);

    /* A labeled parameter group (Clause 6.4.3). */
    t = ann_of(&f, "(a: Int, b: Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_STR(t->u.t_fn.params.data[0]->u.t_field.name, "a");
    EK_CHECK_U64(t->u.t_fn.params.data[1]->u.t_field.name == NULL, 0);
    pfx_end(&f);

    t = ann_of(&f, "(Int, Bool)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.params.data[0]->u.t_field.name, NULL);
    pfx_end(&f);

    /* The zero-parameter group. */
    t = ann_of(&f, "() -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.params.count, 0);
    pfx_end(&f);

    t = ann_of(&f, "(Int) !-> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_WORLD);
    pfx_end(&f);

    t = ann_of(&f, "(Int) ?-> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_OPEN);
    pfx_end(&f);

    /* The elided forms of Clause 6.9.3. */
    t = ann_of(&f, "(Int)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_FN);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_PURE);
    EK_CHECK_U64(t->u.t_fn.result, NULL);
    pfx_end(&f);

    t = ann_of(&f, "(Int) !");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_WORLD);
    EK_CHECK_U64(t->u.t_fn.result, NULL);
    pfx_end(&f);

    t = ann_of(&f, "(Int) ?");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_OPEN);
    pfx_end(&f);

    /* Arrows nest without parentheses (Clause 6.9.3). */
    t = ann_of(&f, "(Out) -> (String) !-> String");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.result->kind, EK_N_T_FN);
    EK_CHECK_U64(t->u.t_fn.result->u.t_fn.arrow, EK_ARROW_WORLD);
    pfx_end(&f);

    t = ann_of(&f, "(Out) !-> (String) -> String");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_WORLD);
    EK_CHECK_U64(t->u.t_fn.result->u.t_fn.arrow, EK_ARROW_PURE);
    pfx_end(&f);
}

static void generic_application(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "Value Int Float");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_APPLY);
    EK_CHECK_STR(t->u.t_apply.head->u.path.data[0], "Value");
    EK_CHECK_U64(t->u.t_apply.args.count, 2);
    pfx_end(&f);

    /* A composite argument is parenthesized (Clause 6.9.4). */
    t = ann_of(&f, "Option (Value Int Float)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_apply.args.count, 1);
    EK_CHECK_U64(t->u.t_apply.args.data[0]->kind, EK_N_T_PAREN);
    pfx_end(&f);

    /* The chain stops at the comma of an enclosing tuple. */
    t = ann_of(&f, "[Value Int Float, Bool]");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.kids.data[0]->kind, EK_N_T_APPLY);
    EK_CHECK_U64(t->u.kids.data[0]->u.t_apply.args.count, 2);
    pfx_end(&f);

    /* A list element carries its own chain (Clause 6.9.4). */
    t = ann_of(&f, "[]Value Int Float");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_LIST);
    EK_CHECK_U64(t->u.wrap.inner->kind, EK_N_T_APPLY);
    pfx_end(&f);

    /* A type variable must not head an application. */
    t = ann_of(&f, "t Int");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-apply-head"));
    pfx_end(&f);

    /* A composite argument without parentheses is rejected. */
    t = ann_of(&f, "Option [Value Int Float]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    /* Application depth: parenthesized composite arguments nest. */
    t = ann_of(&f, "Value (Option Int) (Pair Int Bool)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_apply.args.count, 2);
    EK_CHECK_U64(t->u.t_apply.args.data[0]->kind, EK_N_T_PAREN);
    EK_CHECK_U64(t->u.t_apply.args.data[1]->u.wrap.inner->kind, EK_N_T_APPLY);
    pfx_end(&f);
}

static void nested_signatures(void)
{
    parse_fixture f;
    const ek_node *t;

    /* A function-typed parameter (Clause 6.9.2). */
    t = ann_of(&f, "(f: (Int) -> Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.params.data[0]->u.t_field.type->kind, EK_N_T_FN);
    pfx_end(&f);

    /* The extern signature shape of Clause 6.3.8. */
    t = ann_of(&f, "(base: C.Ptr.Void, compar: (C.Ptr.Void) -> Int) !");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.arrow, EK_ARROW_WORLD);
    EK_CHECK_STR(t->u.t_fn.params.data[0]->u.t_field.type->u.path.data[0],
                 "C");
    pfx_end(&f);
}

int main(void)
{
    atoms();
    brackets();
    arrows();
    generic_application();
    nested_signatures();
    return ek_test_report("parse/type");
}
