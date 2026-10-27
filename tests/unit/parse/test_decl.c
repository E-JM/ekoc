#include "common.h"

/* Every declaration form: value bindings, rebinding, inverted
   bindings, namespace bindings, and the type, ctype, test, and extern
   declarations. */

static void value_bindings(void)
{
    parse_fixture f;
    const ek_node *let;

    pfx_begin(&f, "age: UInt = 36\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(f.ast.module->u.kids.count, 1);
    let = pfx_item(&f, 0);
    EK_CHECK_U64(let->kind, EK_N_LET);
    EK_CHECK_U64(let->u.bind.pattern->kind, EK_N_P_VAR);
    EK_CHECK_U64(let->u.bind.type->kind, EK_N_T_NAME);
    EK_CHECK_U64(let->u.bind.value->kind, EK_N_INT_LIT);
    pfx_end(&f);

    pfx_begin(&f, "x = 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->u.bind.type, NULL);
    pfx_end(&f);

    /* Tuple split of a binding. */
    pfx_begin(&f, "f: Int =\n    (a, b) = row\n    a\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    let = pfx_item(&f, 0)->u.bind.value->u.kids.data[0];
    EK_CHECK_U64(let->kind, EK_N_LET);
    EK_CHECK_U64(let->u.bind.pattern->kind, EK_N_P_PAREN);
    EK_CHECK_U64(let->u.bind.pattern->u.kids.count, 2);
    pfx_end(&f);
}

static void rebind_and_discard(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f, "f: Int =\n    count := count + 1\n    count\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_find(f.ast.module, EK_N_REBIND);
    EK_CHECK(n != NULL);
    EK_CHECK_U64(n->u.bind.pattern->kind, EK_N_P_VAR);
    EK_CHECK_U64(n->u.bind.value->kind, EK_N_BINARY);
    pfx_end(&f);

    pfx_begin(&f, "f: Int =\n    _ := side()\n    0\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_find(f.ast.module, EK_N_REBIND);
    EK_CHECK(n != NULL);
    EK_CHECK_U64(n->u.bind.pattern->kind, EK_N_P_WILDCARD);
    pfx_end(&f);
}

static void inverted_bindings(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f,
        "f: Int =\n"
        "    (p, i) <- persons.each_with_index()\n"
        "    0\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_find(f.ast.module, EK_N_INVERT);
    EK_CHECK(n != NULL);
    EK_CHECK_U64(n->u.bind.pattern->kind, EK_N_P_PAREN);
    EK_CHECK_U64(n->u.bind.value->kind, EK_N_CALL);
    pfx_end(&f);

    /* The zero-argument callback. */
    pfx_begin(&f, "f: Int =\n    <- scope.spawn()\n    0\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_find(f.ast.module, EK_N_INVERT);
    EK_CHECK(n != NULL);
    EK_CHECK_U64(n->u.bind.pattern, NULL);
    pfx_end(&f);

    /* A right-hand side that is not a call is rejected. */
    pfx_begin(&f, "f: Int =\n    x <- 3 + 4\n    0\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-invert-call"));
    EK_CHECK(pfx_msg_has(&f, "must be a call"));
    pfx_end(&f);
}

static void namespace_binding(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f,
        "Money.show: (m: Money) -> String = \"x\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(f.ast.module->u.kids.count, 1);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->kind, EK_N_NAMESPACE);
    EK_CHECK_U64(n->u.namespace.owner.count, 1);
    EK_CHECK_STR(n->u.namespace.owner.data[0], "Money");
    EK_CHECK_STR(n->u.namespace.name, "show");
    EK_CHECK_U64(n->u.namespace.type->kind, EK_N_T_FN);
    pfx_end(&f);

    pfx_begin(&f, "Money._hide: (m: Money) -> String = \"x\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_STR(pfx_item(&f, 0)->u.namespace.name, "_hide");
    pfx_end(&f);
}

static void type_decls(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f, "Sex: type = Man | Woman\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->kind, EK_N_TYPE_DECL);
    EK_CHECK_U64(n->u.type_decl.rhs, EK_TYPE_RHS_VARIANTS);
    EK_CHECK_U64(n->u.type_decl.variants.count, 2);
    EK_CHECK_STR(n->u.type_decl.variants.data[0]->u.variant.name, "Man");
    EK_CHECK_U64(n->u.type_decl.variants.data[0]->u.variant.payload, NULL);
    pfx_end(&f);

    /* A leading bar is admitted. */
    pfx_begin(&f, "T: type = | A | B\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->u.type_decl.variants.count, 2);
    pfx_end(&f);

    /* Generic binders and typed payloads. */
    pfx_begin(&f, "Result: type t e = Ok t | Err e\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->u.type_decl.params.count, 2);
    EK_CHECK_STR(n->u.type_decl.params.data[0], "t");
    EK_CHECK_U64(n->u.type_decl.variants.data[0]->u.variant.payload->kind,
                 EK_N_T_NAME);
    pfx_end(&f);

    /* The vertical form with a leading bar per line. */
    pfx_begin(&f, "T: type =\n    | A\n    | B\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->u.type_decl.rhs, EK_TYPE_RHS_VARIANTS);
    EK_CHECK_U64(n->u.type_decl.variants.count, 2);
    pfx_end(&f);

    /* Record and tuple right-hand sides. */
    pfx_begin(&f, "Person: type = [name: String, age: UInt]\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->u.type_decl.rhs, EK_TYPE_RHS_RECORD);
    EK_CHECK_U64(n->u.type_decl.type->u.kids.count, 2);
    pfx_end(&f);

    pfx_begin(&f, "Pair: type = [A, B]\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->u.type_decl.rhs, EK_TYPE_RHS_TUPLE);
    pfx_end(&f);

    /* An empty right-hand side is not a declaration form; the
       zero-variant type does not exist. */
    pfx_begin(&f, "Never: type =\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-type-decl-form"));
    pfx_end(&f);

    /* A composite expression is not a declaration form. */
    pfx_begin(&f, "Bad: type = []Int\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-type-decl-form"));
    pfx_end(&f);
}

static void ctype_decl(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f, "DivC: ctype = [quot: I32, rem: I32]\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->kind, EK_N_CTYPE_DECL);
    EK_CHECK_STR(n->u.decl.name, "DivC");
    EK_CHECK_U64(n->u.decl.body->kind, EK_N_T_RECORD);
    EK_CHECK_U64(n->u.decl.body->u.kids.count, 2);
    pfx_end(&f);
}

static void test_decl(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f,
        "t1: test =\n"
        "    x: Int = 1\n"
        "    assert_eq(x, 1)\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->kind, EK_N_TEST_DECL);
    EK_CHECK_STR(n->u.decl.name, "t1");
    EK_CHECK_U64(n->u.decl.body->kind, EK_N_BLOCK);
    EK_CHECK_U64(n->u.decl.body->u.kids.count, 2);
    pfx_end(&f);

    pfx_begin(&f, "t2: test = 1 + 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->u.decl.body->kind, EK_N_BINARY);
    pfx_end(&f);
}

static void extern_decl(void)
{
    parse_fixture f;
    const ek_node *n;

    pfx_begin(&f, "_sqrt: (F64) -> F64 = extern \"sqrt\" Pure\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->kind, EK_N_EXTERN_DECL);
    EK_CHECK_STR(n->u.extern_decl.symbol, "sqrt");
    EK_CHECK_U64(n->u.extern_decl.pure, 1);
    EK_CHECK_U64(n->u.extern_decl.type->kind, EK_N_T_FN);
    EK_CHECK_U64(n->u.extern_decl.type->u.t_fn.arrow, EK_ARROW_PURE);
    pfx_end(&f);

    pfx_begin(&f,
        "_qsort: (base: C.Ptr.Void, n: C.Size, size: C.Size,\n"
        "         compar: (C.Ptr.Void, C.Ptr.Void) -> Int) ! = "
        "extern \"qsort\"\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    n = pfx_item(&f, 0);
    EK_CHECK_U64(n->u.extern_decl.pure, 0);
    EK_CHECK_U64(n->u.extern_decl.type->u.t_fn.arrow, EK_ARROW_WORLD);
    EK_CHECK_U64(n->u.extern_decl.type->u.t_fn.result, NULL);
    pfx_end(&f);
}

static void name_case(void)
{
    parse_fixture f;

    pfx_begin(&f, "person: type = [x: Int]\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-name-case"));
    pfx_end(&f);

    pfx_begin(&f, "Foo: test = 1\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-name-case"));
    pfx_end(&f);
}

int main(void)
{
    value_bindings();
    rebind_and_discard();
    inverted_bindings();
    namespace_binding();
    type_decls();
    ctype_decl();
    test_decl();
    extern_decl();
    name_case();
    return ek_test_report("parse/decl");
}
