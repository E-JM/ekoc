#include "common.h"

/* Type expressions: atoms, the bracket forms, arrows, and generic
   application. */

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

    /* A module-qualified type. */
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

    /* `_` is the unit type. */
    t = ann_of(&f, "_");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_STR(t->u.path.data[0], "_");
    pfx_end(&f);

    /* A group in complete-type position is a parameter group; a
       grouped type atom needs an atom position. */
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

/* Lists, dictionaries, tuples, and records, keyed and fielded. */
static void bracket_composites(void)
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

    /* A dictionary keyed by lists. */
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
}

/* A field name is snake_case, a one-element bracket is not a valid
   type, and junk after a comma recovers at `]` without being eaten as
   a field name. */
static void bracket_rejects(void)
{
    parse_fixture f;

    /* A field name is snake_case. */
    (void)ann_of(&f, "[Name: String]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-name-case"));
    pfx_end(&f);

    /* A one-element bracket is not a valid type. */
    (void)ann_of(&f, "[Int]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);

    /* A field is `name :`; junk after a comma recovers at `]` without
       being eaten as a field name. */
    (void)ann_of(&f, "[a: Int, 5]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_msg_has(&f, "expected a field in the record type"));
    pfx_end(&f);

    (void)ann_of(&f, "[a: Int, ]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);
}

/* A dictionary value may be an arrow. */
static void bracket_dict_arrow(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "[String](Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_DICT);
    EK_CHECK_U64(t->u.pair.b->kind, EK_N_T_FN);
    pfx_end(&f);
}

static void brackets(void)
{
    bracket_composites();
    bracket_rejects();
    bracket_dict_arrow();
}

/* Function parameters, labeled and unlabeled, and the pure arrow. */
static void arrow_params(void)
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

    /* A labeled parameter group. */
    t = ann_of(&f, "(a: Int, b: Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_STR(t->u.t_fn.params.data[0]->u.t_field.name, "a");
    EK_CHECK_U64(t->u.t_fn.params.data[1]->u.t_field.name == NULL, 0);
    pfx_end(&f);

    t = ann_of(&f, "(Int, Bool)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.params.data[0]->u.t_field.name, NULL);
    pfx_end(&f);
}

/* The zero-parameter group and the world and open arrows. */
static void arrow_kinds(void)
{
    parse_fixture f;
    const ek_node *t;

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
}

/* The elided forms. */
static void arrow_elided(void)
{
    parse_fixture f;
    const ek_node *t;

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
}

/* Arrows nest without parentheses. */
static void arrow_nested(void)
{
    parse_fixture f;
    const ek_node *t;

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

static void arrows(void)
{
    arrow_params();
    arrow_kinds();
    arrow_elided();
    arrow_nested();
}

/* Applications in operand, tuple-element, and list-element
   positions. */
static void apply_positions(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "Value Int Float");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_APPLY);
    EK_CHECK_STR(t->u.t_apply.head->u.path.data[0], "Value");
    EK_CHECK_U64(t->u.t_apply.args.count, 2);
    pfx_end(&f);

    /* A composite argument is parenthesized. */
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

    /* A list element carries its own chain. */
    t = ann_of(&f, "[]Value Int Float");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_LIST);
    EK_CHECK_U64(t->u.wrap.inner->kind, EK_N_T_APPLY);
    pfx_end(&f);
}

/* Heads and arguments: a type variable must not head an application, a
   qualified head names the type in its last segment, and a composite
   argument without parentheses is rejected. */
static void apply_heads(void)
{
    parse_fixture f;
    const ek_node *t;

    /* A type variable must not head an application. */
    t = ann_of(&f, "t Int");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-apply-head"));
    pfx_end(&f);

    /* A qualified head names the type in its last segment. */
    t = ann_of(&f, "geolib.Value Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->kind, EK_N_T_APPLY);
    EK_CHECK_STR(t->u.t_apply.head->u.path.data[0], "geolib");
    EK_CHECK_STR(t->u.t_apply.head->u.path.data[1], "Value");
    EK_CHECK_U64(t->u.t_apply.args.count, 1);
    pfx_end(&f);

    /* A composite argument without parentheses is rejected. */
    t = ann_of(&f, "Option [Value Int Float]");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_end(&f);
}

/* Application depth: parenthesized composite arguments nest. */
static void apply_depth(void)
{
    parse_fixture f;
    const ek_node *t;

    t = ann_of(&f, "Value (Option Int) (Pair Int Bool)");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_apply.args.count, 2);
    EK_CHECK_U64(t->u.t_apply.args.data[0]->kind, EK_N_T_PAREN);
    EK_CHECK_U64(t->u.t_apply.args.data[1]->u.wrap.inner->kind, EK_N_T_APPLY);
    pfx_end(&f);
}

static void generic_application(void)
{
    apply_positions();
    apply_heads();
    apply_depth();
}

static void nested_signatures(void)
{
    parse_fixture f;
    const ek_node *t;

    /* A function-typed parameter. */
    t = ann_of(&f, "(f: (Int) -> Int) -> Int");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(t->u.t_fn.params.data[0]->u.t_field.type->kind, EK_N_T_FN);
    pfx_end(&f);

    /* The extern signature shape. */
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
