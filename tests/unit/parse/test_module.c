#include "common.h"

/* Compilation units (Clause 6.2.1): imports, member dispatch, and the
   semicolon suites of Clause 5.3.2. Recovery is exercised by modules
   that yield several diagnostics without aborting. */

static void imports(void)
{
    parse_fixture f;
    const ek_node *imp;

    pfx_begin(&f, "import person\nimport csv/lex\n\nmain: Int = 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(f.ast.module->u.kids.count, 3);
    imp = pfx_item(&f, 0);
    EK_CHECK_U64(imp->kind, EK_N_IMPORT);
    EK_CHECK_U64(imp->u.path.count, 1);
    EK_CHECK_STR(imp->u.path.data[0], "person");
    imp = pfx_item(&f, 1);
    EK_CHECK_U64(imp->u.path.count, 2);
    EK_CHECK_STR(imp->u.path.data[1], "lex");
    pfx_end(&f);

    /* An import after a binding is an error (Clause 6.2.1). */
    pfx_begin(&f, "x: Int = 1\nimport person\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-import-order"));
    EK_CHECK(pfx_msg_has(&f, "6.2.1"));
    pfx_end(&f);

    /* An import is not a block member. */
    pfx_begin(&f, "f: Int =\n    import person\n    1\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-import-order"));
    pfx_end(&f);
}

static void top_level_items(void)
{
    parse_fixture f;

    /* An expression is not a top-level item (A.3.1). */
    pfx_begin(&f, "1 + 2\n");
    EK_CHECK(pfx_errors(&f) > 0);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-top-item"));
    pfx_end(&f);

    /* A file with no bindings is legal (Clause 6.2.1). */
    pfx_begin(&f, "import person\n// only a comment\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(f.ast.module->u.kids.count, 1);
    pfx_end(&f);

    pfx_begin(&f, "");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK(f.ast.module != NULL);
    EK_CHECK_U64(f.ast.module->u.kids.count, 0);
    pfx_end(&f);
}

static void semicolon_suites(void)
{
    parse_fixture f;
    const ek_node *body;

    /* `;` is a line-terminator equivalent (Clause 5.3.2). */
    pfx_begin(&f,
        "f: Int =\n"
        "    a: Int = 1; b: Int = 2\n"
        "    a + b\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    body = pfx_find(f.ast.module, EK_N_BLOCK);
    EK_CHECK(body != NULL);
    EK_CHECK_U64(body->u.kids.count, 3);
    EK_CHECK_U64(body->u.kids.data[0]->kind, EK_N_LET);
    EK_CHECK_U64(body->u.kids.data[1]->kind, EK_N_LET);
    EK_CHECK_U64(body->u.kids.data[2]->kind, EK_N_BINARY);
    pfx_end(&f);

    /* A lambda body may be a `;` suite (6.7.5). */
    pfx_begin(&f,
        "g: (out: Out) -> (Int) !-> Int =\n"
        "    (n): out.println(show(n)); n + 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    {
        const ek_node *lam = pfx_find(f.ast.module, EK_N_LAMBDA);

        EK_CHECK(lam != NULL);
        EK_CHECK_U64(lam->u.pair.b->kind, EK_N_BLOCK);
        EK_CHECK_U64(lam->u.pair.b->u.kids.count, 2);
    }
    pfx_end(&f);
}

static void recovery(void)
{
    parse_fixture f;

    /* One module, many diagnostics: the parse drains (ARCHITECTURE
       rule 4) and still returns an AST. */
    pfx_begin(&f,
        "a: Int = 1\n"
        "b: Int = [1, 2]\n"
        "c: Int = 3\n"
        "d: Int = xs[0]\n"
        "e: Int = 5\n");
    EK_CHECK(pfx_errors(&f) >= 2);
    pfx_collect(&f);
    EK_CHECK(pfx_has_code(&f, "E-value-bracket"));
    EK_CHECK(pfx_has_code(&f, "E-index-syntax"));
    EK_CHECK(f.ast.module != NULL);
    /* `[1, 2]` yields no item; `xs[0]` recovers to its receiver; the
       other three lines survive untouched. */
    EK_CHECK_U64(f.ast.module->u.kids.count, 4);
    pfx_end(&f);

    /* A broken arm does not take the rest of the case with it. */
    pfx_begin(&f,
        "f: (r: Result Int String) -> Int =\n"
        "    case r:\n"
        "        Ok v -> v\n"
        "        panic(\"boom\")\n"
        "        Err e -> e\n");
    EK_CHECK(pfx_errors(&f) >= 1);
    EK_CHECK(f.ast.module != NULL);
    pfx_end(&f);
}

static void member_shapes(void)
{
    parse_fixture f;

    /* Nested blocks: a test body with bindings and a nested if. */
    pfx_begin(&f,
        "t: test =\n"
        "    n: Int = 2\n"
        "    if n > 1:\n"
        "        assert(True)\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK(pfx_find(f.ast.module, EK_N_TEST_DECL) != NULL);
    EK_CHECK(pfx_find(f.ast.module, EK_N_IF) != NULL);
    pfx_end(&f);

    /* An inverted chain of Clause 6.3.4 nests in sequence. */
    pfx_begin(&f,
        "f: (out: Out) ! =\n"
        "    a <- f1()\n"
        "    b <- f2(a)\n"
        "    out.println(a + b)\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_count_kind(f.ast.module, EK_N_INVERT), 2);
    pfx_end(&f);
}

int main(void)
{
    imports();
    top_level_items();
    semicolon_suites();
    recovery();
    member_shapes();
    return ek_test_report("parse/module");
}
