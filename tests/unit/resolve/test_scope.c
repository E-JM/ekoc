#include "common.h"

/* Scoping: blocks, lambdas, case arms with guards, shadowing and the
   binding line. */

static void block_nesting(void)
{
    rfx f;
    const ek_ref *r;

    /* An inner binding shadows an outer one. */
    rfx_begin(&f,
        "x: Int = 1\n"
        "f: Int =\n"
        "    x: Int = 2\n"
        "    x\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 4, "x");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    EK_CHECK(r->station == EK_ST_HEAD_LOCAL);
    rfx_end(&f);

    /* A local is visible from its binding line onward only; a
       block-level self-reference reaches nothing. */
    rfx_begin(&f,
        "f: Int =\n"
        "    y := x\n"
        "    x: Int = 1\n"
        "    x\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 2));
    rfx_end(&f);

    /* A rebinding sees the earlier binding, then shadows it. */
    rfx_begin(&f,
        "f: Int =\n"
        "    x: Int = 1\n"
        "    x := x + 1\n"
        "    x\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

/* A rebinding shadows the binding it rebinds: a use after the `:=`
   denotes the new binding, the rebind's own right-hand side the old
   one. */
static void rebinding_denotation(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: Int =\n"
        "    x: Int = 1\n"
        "    x := x + 1\n"
        "    x\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "x");
    EK_CHECK(r != NULL);
    EK_CHECK(r->symbol != NULL);
    EK_CHECK_U64((unsigned long long)(r->symbol ? r->symbol->line : 0), 2);
    r = rfx_ref_named(&f, 4, "x");
    EK_CHECK(r != NULL);
    EK_CHECK(r->symbol != NULL);
    EK_CHECK_U64((unsigned long long)(r->symbol ? r->symbol->line : 0), 3);
    rfx_end(&f);
}

static void top_level_visibility(void)
{
    rfx f;
    const ek_ref *r;

    /* Top-level bindings are mutually visible, including forward and
       self references. */
    rfx_begin(&f,
        "a: Int = b\n"
        "b: Int = a\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);

    rfx_begin(&f,
        "f: (n: Int) -> Int = f(n)\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 1, "f");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_HEAD_TOP);
    r = rfx_ref_named(&f, 1, "n");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_PARAM);
    EK_CHECK(r->station == EK_ST_HEAD_LOCAL);
    rfx_end(&f);
}

static void case_arms(void)
{
    rfx f;
    const ek_ref *r;

    /* A pattern binding is visible in its arm's guard and result, and
       in that arm alone. */
    rfx_begin(&f,
        "sign: (n: Int) -> String =\n"
        "    case n:\n"
        "        m if m < 0 -> \"neg\"\n"
        "        _ -> m\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 4));
    r = rfx_ref_named(&f, 3, "m");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    EK_CHECK_STR(r->symbol ? r->symbol->name : NULL, "m");
    rfx_end(&f);

    rfx_begin(&f,
        "sign: (n: Int) -> String =\n"
        "    case n:\n"
        "        m if m < 0 -> \"neg\"\n"
        "        _ -> \"pos\"\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

static void tuple_split_and_discard(void)
{
    rfx f;
    const ek_ref *r;

    /* A tuple split binds both names. */
    rfx_begin(&f,
        "pair: [String, Int] = {\"a\", 1}\n"
        "t: () -> Int =\n"
        "    (a, b) = pair\n"
        "    b\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 4, "b");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    EK_CHECK_STR(r->symbol ? r->symbol->name : NULL, "b");
    rfx_end(&f);

    /* `_` is a discard binding and the unit value; neither binds. */
    rfx_begin(&f,
        "t: (p: [Int, Int]) -> [Int, Int] =\n"
        "    _ := p\n"
        "    p\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

static void lambdas(void)
{
    rfx f;
    const ek_ref *r;

    /* A lambda parameter binds in its body. */
    rfx_begin(&f,
        "twice: (g: (Int) -> Int, v: Int) -> Int = g(g(v))\n"
        "f: Int = twice((n): n * 2, 1)\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "n");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_PARAM);
    EK_CHECK(r->station == EK_ST_HEAD_LOCAL);
    rfx_end(&f);

    /* A body closure sees the enclosing block's earlier bindings. */
    rfx_begin(&f,
        "t: () -> Int =\n"
        "    base: Int = 10\n"
        "    read: () -> Int = (): base\n"
        "    read()\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

static void rest_binding(void)
{
    rfx f;
    const ek_ref *r;

    /* A list rest binds its name; the wildcard rest binds nothing. */
    rfx_begin(&f,
        "head: (xs: []Int) -> Int =\n"
        "    case xs:\n"
        "        {h, ..t} -> h + t.length()\n"
        "        _ -> 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "t");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    rfx_end(&f);
}

int main(void)
{
    block_nesting();
    top_level_visibility();
    case_arms();
    tuple_split_and_discard();
    lambdas();
    rest_binding();
    rebinding_denotation();
    return ek_test_report("resolve/scope");
}
