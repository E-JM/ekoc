#include <string.h>

#include "parse/internal.h"

ek_parse_result ek_parse(const ek_layout_token *tokens, size_t count,
                         const char *file, ek_arena *arena, ek_diag *diag)
{
    p_ctx ctx;
    ek_parse_result r;

    memset(&ctx, 0, sizeof ctx);
    ctx.file = file;
    ctx.arena = arena;
    ctx.diag = diag;
    ctx.toks = tokens;
    ctx.count = tokens && count > 0 ? count : 1;
    if (!tokens || count == 0) {
        /* No stream from layout (allocation failure upstream): the
           pipeline still drains, so hand back an empty module. */
        ek_diag_emit(diag, EK_SEV_ERROR, NULL, file, 0, 0,
                     "the layout stage produced no token stream; the "
                     "module is empty");
        r.module = p_node(&ctx, EK_N_MODULE, 1, 1);
        if (r.module) {
            p_list_init(&r.module->u.kids);
        }
        return r;
    }
    r.module = p_module(&ctx);
    return r;
}

/* A module: import lines first, then top-level items. */
ek_node *p_module(p_ctx *c)
{
    const ek_layout_token *first = p_peek(c);
    ek_node *mod = p_node(c, EK_N_MODULE, first->line, first->col);
    int seen_item = 0;

    if (!mod) {
        return NULL;
    }
    p_list_init(&mod->u.kids);
    while (!c->oom) {
        size_t before = c->pos;
        ek_node *item = NULL;

        if (p_at_layout(c, EK_LTOK_EOF)) {
            break;
        }
        if (p_got_layout(c, EK_LTOK_NEWLINE) || p_got(c, EK_TOK_SEMI)) {
            continue;
        }
        if (p_at(c, EK_TOK_KW_IMPORT)) {
            if (seen_item) {
                p_error_tok(c, "E-import-order", p_peek(c),
                            "an import line must precede every top-level "
                            "binding (Clause 6.2.1)");
            }
            item = p_member(c, 1);
        } else {
            item = p_member(c, 1);
            seen_item = item != NULL;
        }
        if (c->fatal) {
            break;
        }
        p_list_add(c, &mod->u.kids, item);
        if (c->pos == before) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "token cannot begin a top-level item; skipped");
            p_take(c);
        }
    }
    return mod;
}

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
static ek_node *p_import(p_ctx *c)
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
                    "expected end of line after the import path");
        p_sync(c);
    }
    p_got_layout(c, EK_LTOK_NEWLINE);
    return n;
}

/* One member line: a value binding or an expression. */
ek_node *p_member(p_ctx *c, int top_level)
{
    if (c->oom) {
        return NULL;
    }
    if (p_at(c, EK_TOK_KW_IMPORT)) {
        if (top_level) {
            return p_import(c);
        }
        p_error_tok(c, "E-import-order", p_peek(c),
                    "import is a compilation-unit item, not a member "
                    "(Clause 6.2.1)");
        p_sync(c);
        return NULL;
    }
    if (p_at(c, EK_TOK_KW_CASE)) {
        return p_case_expr(c);
    }
    if (p_at(c, EK_TOK_KW_IF)) {
        return p_if_expr(c);
    }
    if (p_at(c, EK_TOK_LARROW)) {
        return p_invert_bare(c);
    }
    if (p_binding_op_ahead(c)) {
        return p_binding_line(c, top_level);
    }
    {
        ek_node *e = p_expr(c);

        if (!e) {
            p_sync(c);
            return NULL;
        }
        if (top_level) {
            p_error(c, "E-top-item", e->line, e->col,
                    "a top-level item is a binding or declaration, not "
                    "an expression (Clause 6.2.1)");
            return NULL;
        }
        return e;
    }
}

/* The zero-pattern form `<- Call`. */
ek_node *p_invert_bare(p_ctx *c)
{
    const ek_layout_token *arrow = p_take(c);
    ek_node *call = p_expr(c);
    ek_node *n;

    if (!call) {
        return NULL;
    }
    if (call->kind != EK_N_CALL) {
        p_error(c, "E-invert-call", call->line, call->col,
                "the right-hand side of `<-` is a call (Clause 6.3.1)");
    }
    n = p_node(c, EK_N_INVERT, arrow->line, arrow->col);
    if (n) {
        n->u.bind.pattern = NULL;
        n->u.bind.type = NULL;
        n->u.bind.value = call;
    }
    return n;
}

/* Finds the first binding operator at bracket depth zero before the
   line ends; the member dispatch hangs on it. */
int p_binding_op_ahead(p_ctx *c)
{
    size_t i;
    int depth = 0;

    for (i = c->pos; i < c->count; i++) {
        const ek_layout_token *t = &c->toks[i];

        if (t->kind == EK_LTOK_EOF || t->kind == EK_LTOK_NEWLINE
            || t->kind == EK_LTOK_DEDENT) {
            return 0;
        }
        if (t->kind != EK_LTOK_SOURCE) {
            continue;
        }
        switch (t->tok.kind) {
        case EK_TOK_LPAREN:
        case EK_TOK_LBRACKET:
        case EK_TOK_LBRACE:
            depth++;
            break;
        case EK_TOK_RPAREN:
        case EK_TOK_RBRACKET:
        case EK_TOK_RBRACE:
            if (depth > 0) {
                depth--;
            }
            break;
        case EK_TOK_COLONEQ:
            return 1;
        case EK_TOK_LARROW:
            return 2;
        case EK_TOK_EQ:
            if (depth == 0) {
                return 3;
            }
            break;
        case EK_TOK_SEMI:
            return 0;
        default:
            break;
        }
    }
    return 0;
}

/* A dotted name path; head receives the first token. */
void p_name_path(p_ctx *c, ek_slist *path, const ek_layout_token **head)
{
    p_path_init(path);
    *head = NULL;
    if (!p_at(c, EK_TOK_IDENT)) {
        return;
    }
    *head = p_take(c);
    p_path_add(c, path, (*head)->tok.text);
    while (p_at(c, EK_TOK_DOT)
           && p_peek_at(c, 1)->kind == EK_LTOK_SOURCE
           && p_peek_at(c, 1)->tok.kind == EK_TOK_IDENT) {
        const ek_layout_token *seg;

        p_take(c); /* the dot */
        seg = p_take(c);
        p_path_add(c, path, seg->tok.text);
    }
}

/* The single-name patterns admitted on a binding line: a variable, the
   wildcard, or — by spelling — a constructor. */
ek_node *p_path_pattern(p_ctx *c, const ek_slist *path,
                        const ek_layout_token *head)
{
    ek_node *n;

    if (path->count != 1) {
        p_error_tok(c, "E-syntax", head,
                    "a binding pattern is one name; write a tuple group to "
                    "destructure (Clause 6.8.1)");
        return NULL;
    }
    if (strcmp(path->data[0], "_") == 0) {
        return p_node(c, EK_N_P_WILDCARD, head->line, head->col);
    }
    if (p_is_pascal(path->data[0])) {
        n = p_node(c, EK_N_P_CONS, head->line, head->col);
        if (n) {
            p_path_init(&n->u.cons.name);
            p_path_add(c, &n->u.cons.name, path->data[0]);
        }
        return n;
    }
    n = p_node(c, EK_N_P_VAR, head->line, head->col);
    if (n) {
        p_path_init(&n->u.path);
        p_path_add(c, &n->u.path, path->data[0]);
    }
    return n;
}

/* `<-` admits exactly the lambda parameter shapes. */
void p_check_invert_pattern(p_ctx *c, const ek_node *pat)
{
    size_t i;

    switch (pat->kind) {
    case EK_N_P_WILDCARD:
        return;
    case EK_N_P_VAR:
        if (!p_is_snake(pat->u.path.data[0])) {
            p_error(c, "E-invert-pattern", pat->line, pat->col,
                    "`<-` binds a snake_case name (Clause 6.3.4)");
        }
        return;
    case EK_N_P_PAREN:
        for (i = 0; i < pat->u.kids.count; i++) {
            p_check_invert_pattern(c, pat->u.kids.data[i]);
        }
        return;
    default:
        p_error(c, "E-invert-pattern", pat->line, pat->col,
                "`<-` admits only a name, `_`, or a parameter group "
                "(Clause 6.3.4)");
    }
}
