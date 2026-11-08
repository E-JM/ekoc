#include "parse/internal.h"

/* Recovers from a malformed selective list: consume through the
   closing brace, or to the end of the line when no brace closes it. */
static void p_import_skip(p_ctx *c)
{
    for (;;) {
        const ek_layout_token *t = p_peek(c);

        if (t->kind != EK_LTOK_SOURCE) {
            return;
        }
        if (t->tok.kind == EK_TOK_RBRACE) {
            p_take(c);
            return;
        }
        if (t->tok.kind == EK_TOK_SEMI || t->tok.kind == EK_TOK_EOF) {
            return;
        }
        p_take(c);
    }
}

/* The selective list after the module path: '.' '{' name {',' name}
   '}'. A list names at least one free name and does not end in a
   comma; the cursor sits on the dot. */
static void p_import_names(p_ctx *c, ek_node *n)
{
    p_take(c); /* the dot */
    p_take(c); /* the brace */
    if (p_at(c, EK_TOK_RBRACE)) {
        p_error_tok(c, "E-syntax", p_take(c),
                    "an import list names at least one free name");
        return;
    }
    for (;;) {
        const ek_layout_token *id =
            p_expect(c, EK_TOK_IDENT, "a free name in the import list");

        if (!id) {
            p_import_skip(c);
            return;
        }
        p_path_add(c, &n->u.imports.names, id->tok.text);
        if (!p_got(c, EK_TOK_COMMA)) {
            break;
        }
        if (p_at(c, EK_TOK_RBRACE)) {
            p_error_tok(c, "E-syntax", p_take(c),
                        "a trailing comma does not end an import list");
            return;
        }
    }
    if (!p_got(c, EK_TOK_RBRACE)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `}` after an import name");
        p_import_skip(c);
    }
}

/* An import line: 'import' followed by a '/'-separated module path, an
   optional selective list, and the end of the line. */
ek_node *p_import(p_ctx *c)
{
    const ek_layout_token *kw = p_take(c);
    ek_node *n = p_node(c, EK_N_IMPORT, kw->line, kw->col);

    if (!n) {
        return NULL;
    }
    p_path_init(&n->u.imports.path);
    p_path_init(&n->u.imports.names);
    for (;;) {
        const ek_layout_token *seg =
            p_expect(c, EK_TOK_IDENT, "a module path segment");

        if (!seg) {
            p_sync(c);
            return NULL;
        }
        p_path_add(c, &n->u.imports.path, seg->tok.text);
        if (!p_got(c, EK_TOK_SLASH)) {
            break;
        }
    }
    if (p_at(c, EK_TOK_DOT) && p_peek_at(c, 1)->kind == EK_LTOK_SOURCE
        && p_peek_at(c, 1)->tok.kind == EK_TOK_LBRACE) {
        p_import_names(c, n);
    }
    if (!p_at_layout(c, EK_LTOK_NEWLINE) && !p_at_layout(c, EK_LTOK_EOF)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `/` or the line's end after the import path; "
                    "a path segment is an identifier (Clause 7.2.2)");
        p_sync(c);
    }
    p_got_layout(c, EK_LTOK_NEWLINE);
    return n;
}
