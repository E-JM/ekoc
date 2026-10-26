#include <string.h>

#include "parse/internal.h"

/* Names carry their position so case diagnostics land on the
   identifier. */
void p_check_case(p_ctx *c, const char *name, const ek_layout_token *t,
                  int want_pascal, const char *what)
{
    int pascal = p_is_pascal(name);

    if (want_pascal && !pascal) {
        p_error(c, "E-name-case", t->line, t->col,
                "%s name `%s` is not PascalCase (Clause 5.5.2)", what, name);
    }
    if (!want_pascal && pascal) {
        p_error(c, "E-name-case", t->line, t->col,
                "%s name `%s` is not snake_case (Clause 5.5.2)", what, name);
    }
}

/* The cooked bytes of a string literal that must be plain: an extern
   symbol carries no interpolation. */
static const char *lit_text(p_ctx *c, const ek_layout_token *t)
{
    size_t i, len = 0;
    char *out;

    for (i = 0; i < t->tok.part_count; i++) {
        if (!t->tok.parts[i].tokens) {
            len += t->tok.parts[i].text_len;
        }
    }
    out = p_alloc(c, len + 1);
    if (!out) {
        return NULL;
    }
    len = 0;
    for (i = 0; i < t->tok.part_count; i++) {
        if (!t->tok.parts[i].tokens) {
            memcpy(out + len, t->tok.parts[i].text, t->tok.parts[i].text_len);
            len += t->tok.parts[i].text_len;
        } else {
            p_error(c, "E-syntax", t->line, t->col,
                    "an extern symbol is a plain string (Clause 6.3.8)");
        }
    }
    out[len] = '\0';
    return out;
}

/* A variant: a Pascal name with an optional type atom payload. */
ek_node *p_variant(p_ctx *c)
{
    const ek_layout_token *head = p_expect(c, EK_TOK_IDENT, "a variant name");
    ek_node *v;

    if (!head) {
        p_sync(c);
        return NULL;
    }
    p_check_case(c, head->tok.text, head, 1, "variant");
    v = p_node(c, EK_N_VARIANT, head->line, head->col);
    if (!v) {
        return NULL;
    }
    v->u.variant.name = head->tok.text;
    v->u.variant.payload = NULL;
    if (p_atom_start(p_src_kind(p_peek(c)))) {
        v->u.variant.payload = p_type_atom(c);
    }
    return v;
}

/* NEWLINE INDENT VariantLine { VariantLine } DEDENT; every line leads
   with `|`. */
static void variant_lines(p_ctx *c, ek_node *n)
{
    while (!c->oom) {
        size_t before = c->pos;
        ek_node *v;

        if (p_got_layout(c, EK_LTOK_DEDENT)
            || p_at_layout(c, EK_LTOK_EOF)) {
            return;
        }
        if (p_got_layout(c, EK_LTOK_NEWLINE)) {
            continue;
        }
        if (!p_at(c, EK_TOK_PIPE)) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "a vertical variant line begins with `|` "
                        "(Clause 6.3.5)");
            p_sync(c);
            p_got_layout(c, EK_LTOK_NEWLINE);
            continue;
        }
        p_take(c);
        v = p_variant(c);
        p_list_add(c, &n->u.type_decl.variants, v);
        if (c->pos == before) {
            p_take(c);
        }
    }
}

/* VariantList := [ '|' ] Variant { '|' Variant } on one line. */
static void variant_list(p_ctx *c, ek_node *n)
{
    p_got(c, EK_TOK_PIPE);
    for (;;) {
        ek_node *v = p_variant(c);

        p_list_add(c, &n->u.type_decl.variants, v);
        if (!p_got(c, EK_TOK_PIPE)) {
            return;
        }
        if (!p_at(c, EK_TOK_IDENT)) {
            p_error_tok(c, "E-syntax", p_peek(c), "expected a variant");
            p_sync(c);
            return;
        }
    }
}

/* A type right-hand side: a variant list, a block of variant lines, a
   record, a tuple, or nothing. A bracket form that is neither a record
   nor a tuple is an alias, which does not exist. */
static void type_rhs(p_ctx *c, ek_node *n)
{
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        p_take(c);
        if (!p_got_layout(c, EK_LTOK_INDENT)) {
            n->u.type_decl.rhs = EK_TYPE_RHS_EMPTY; /* zero-variant */
            return;
        }
        n->u.type_decl.rhs = EK_TYPE_RHS_VARIANTS;
        p_list_init(&n->u.type_decl.variants);
        variant_lines(c, n);
        return;
    }
    if (p_at(c, EK_TOK_LBRACKET)) {
        ek_node *t = p_type(c);

        p_list_init(&n->u.type_decl.variants);
        if (!t) {
            p_sync(c);
            n->u.type_decl.rhs = EK_TYPE_RHS_EMPTY;
            return;
        }
        if (t->kind == EK_N_T_RECORD) {
            n->u.type_decl.rhs = EK_TYPE_RHS_RECORD;
            n->u.type_decl.type = t;
            return;
        }
        if (t->kind == EK_N_T_TUPLE) {
            n->u.type_decl.rhs = EK_TYPE_RHS_TUPLE;
            n->u.type_decl.type = t;
            return;
        }
        p_error(c, "E-type-decl-form", t->line, t->col,
                "the right-hand side of `type` is a declaration form, not a "
                "type expression (Clause 6.9.5)");
        n->u.type_decl.rhs = EK_TYPE_RHS_EMPTY;
        return;
    }
    n->u.type_decl.rhs = EK_TYPE_RHS_VARIANTS;
    p_list_init(&n->u.type_decl.variants);
    variant_list(c, n);
}

/* A type declaration: 'type', optional type variable binders, '=', and
   the right-hand side; the colon and the cursor before `type` are the
   caller's. */
ek_node *p_type_decl(p_ctx *c, const char *name, const ek_layout_token *head)
{
    ek_node *n = p_node(c, EK_N_TYPE_DECL, head->line, head->col);

    p_check_case(c, name, head, 1, "type declaration");
    if (!n) {
        return NULL;
    }
    p_take(c); /* the word `type` */
    n->u.type_decl.name = name;
    n->u.type_decl.type = NULL;
    p_path_init(&n->u.type_decl.params);
    while (p_at(c, EK_TOK_IDENT)) {
        const ek_layout_token *var = p_peek(c);

        p_check_case(c, var->tok.text, var, 0, "type variable");
        p_path_add(c, &n->u.type_decl.params, p_take(c)->tok.text);
    }
    if (!p_got(c, EK_TOK_EQ)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `=` before the type right-hand side");
        p_sync(c);
        n->u.type_decl.rhs = EK_TYPE_RHS_EMPTY;
        return n;
    }
    type_rhs(c, n);
    return n;
}

/* A ctype declaration: 'ctype', '=', and a record type. */
ek_node *p_ctype_decl(p_ctx *c, const char *name, const ek_layout_token *head)
{
    ek_node *n = p_node(c, EK_N_CTYPE_DECL, head->line, head->col);

    p_check_case(c, name, head, 1, "ctype declaration");
    if (!n) {
        return NULL;
    }
    p_take(c); /* the word `ctype` */
    n->u.decl.name = name;
    n->u.decl.body = NULL;
    if (!p_got(c, EK_TOK_EQ)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `=` before the ctype record");
        p_sync(c);
        return n;
    }
    {
        ek_node *t = p_type(c);

        if (!t) {
            p_sync(c);
            return n;
        }
        if (t->kind != EK_N_T_RECORD) {
            p_error(c, "E-syntax", t->line, t->col,
                    "a ctype right-hand side is a record type (Clause 6.3.6)");
        }
        n->u.decl.body = t;
    }
    return n;
}

/* A test declaration: 'test', '=', and an expression or block body. */
ek_node *p_test_decl(p_ctx *c, const char *name, const ek_layout_token *head)
{
    ek_node *n = p_node(c, EK_N_TEST_DECL, head->line, head->col);

    p_check_case(c, name, head, 0, "test declaration");
    if (!n) {
        return NULL;
    }
    p_take(c); /* the word `test` */
    n->u.decl.name = name;
    if (!p_got(c, EK_TOK_EQ)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `=` before the test body");
        p_sync(c);
        n->u.decl.body = NULL;
        return n;
    }
    n->u.decl.body = p_binding_rhs(c);
    return n;
}

/* Extern declaration tail: the word `extern`, the native symbol, and
   the optional `Pure`. The signature comes in as the annotation. */
ek_node *p_extern_tail(p_ctx *c, const char *name,
                       const ek_layout_token *head, ek_node *sig)
{
    ek_node *n = p_node(c, EK_N_EXTERN_DECL, head->line, head->col);

    p_check_case(c, name, head, 0, "extern declaration");
    if (!n) {
        return NULL;
    }
    if (sig->kind != EK_N_T_FN) {
        p_error(c, "E-syntax", head->line, head->col,
                "an extern declaration signs with a function type "
                "(Clause 6.3.8)");
    }
    n->u.extern_decl.name = name;
    n->u.extern_decl.type = sig;
    n->u.extern_decl.pure = 0;
    p_take(c); /* the word `extern` */
    n->u.extern_decl.symbol = NULL;
    {
        const ek_layout_token *str = p_expect(c, EK_TOK_STRING,
                                              "a native symbol string");

        if (str) {
            n->u.extern_decl.symbol = lit_text(c, str);
        }
    }
    if (p_at(c, EK_TOK_IDENT) && strcmp(p_peek(c)->tok.text, "Pure") == 0) {
        p_take(c);
        n->u.extern_decl.pure = 1;
    }
    return n;
}
