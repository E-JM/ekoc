#include "common.h"

/* Documents and bindings: the document before a binding is consumed
   there, consecutive lines form one document, and only a line that
   begins with the deprecation convention sets the flag. */

static void value_binding(void)
{
    parse_fixture f;

    pfx_begin(&f, "/// @deprecated\nold = 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->kind, EK_N_LET);
    EK_CHECK_U64(pfx_item(&f, 0)->deprecated, 1);
    pfx_end(&f);

    pfx_begin(&f, "/// ordinary text\nold = 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->deprecated, 0);
    pfx_end(&f);

    pfx_begin(&f, "///   @deprecated\nold = 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->deprecated, 1);
    pfx_end(&f);
}

static void consecutive_lines(void)
{
    parse_fixture f;

    pfx_begin(&f, "/// text\n/// @deprecated\nold = 1\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->deprecated, 1);
    pfx_end(&f);

    pfx_begin(&f,
              "/// @deprecated\n"
              "/// more text\n"
              "old = 1\n"
              "next = 2\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->deprecated, 1);
    EK_CHECK_U64(pfx_item(&f, 1)->deprecated, 0);
    pfx_end(&f);
}

static void block_binding(void)
{
    parse_fixture f;
    const ek_node *fn;
    const ek_node *body;
    const ek_node *let;

    pfx_begin(&f,
              "f =\n"
              "    /// @deprecated\n"
              "    x = 1\n"
              "    x\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    fn = pfx_item(&f, 0);
    EK_CHECK_U64(fn->kind, EK_N_LET);
    EK_CHECK_U64(fn->deprecated, 0);
    body = fn->u.bind.value;
    EK_CHECK_U64(body->kind, EK_N_BLOCK);
    let = body->u.kids.data[0];
    EK_CHECK_U64(let->kind, EK_N_LET);
    EK_CHECK_U64(let->deprecated, 1);
    pfx_end(&f);
}

static void declarations(void)
{
    parse_fixture f;

    pfx_begin(&f,
              "/// @deprecated\n"
              "Shape: type = Circle Float\n"
              "/// @deprecated\n"
              "Point: ctype = [x: Float]\n"
              "/// @deprecated\n"
              "smoke: test = 1\n"
              "/// @deprecated\n"
              "sqrt: (F64) -> F64 = extern \"sqrt\"\n"
              "/// @deprecated\n"
              "Shape.area: (s: Shape) -> Float = 1.0\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_item(&f, 0)->kind, EK_N_TYPE_DECL);
    EK_CHECK_U64(pfx_item(&f, 0)->deprecated, 1);
    EK_CHECK_U64(pfx_item(&f, 1)->kind, EK_N_CTYPE_DECL);
    EK_CHECK_U64(pfx_item(&f, 1)->deprecated, 1);
    EK_CHECK_U64(pfx_item(&f, 2)->kind, EK_N_TEST_DECL);
    EK_CHECK_U64(pfx_item(&f, 2)->deprecated, 1);
    EK_CHECK_U64(pfx_item(&f, 3)->kind, EK_N_EXTERN_DECL);
    EK_CHECK_U64(pfx_item(&f, 3)->deprecated, 1);
    EK_CHECK_U64(pfx_item(&f, 4)->kind, EK_N_NAMESPACE);
    EK_CHECK_U64(pfx_item(&f, 4)->deprecated, 1);
    pfx_end(&f);
}

static void unattached_document(void)
{
    parse_fixture f;

    /* A document at the end of the file describes nothing. */
    pfx_begin(&f, "/// @deprecated\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(f.ast.module->u.kids.count, 0);
    pfx_end(&f);

    /* A non-binding member is not described either. */
    pfx_begin(&f,
              "f =\n"
              "    /// @deprecated\n"
              "    if c: 1\n"
              "    2\n");
    EK_CHECK_U64(pfx_errors(&f), 0);
    EK_CHECK_U64(pfx_find(f.ast.module, EK_N_IF)->deprecated, 0);
    pfx_end(&f);
}

int main(void)
{
    value_binding();
    consecutive_lines();
    block_binding();
    declarations();
    unattached_document();
    return ek_test_report("parse/docs");
}
