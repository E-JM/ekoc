#include <string.h>

#include "parse/internal.h"

/* Patterns of Clause 6.8 / A.3.6. Variables and constructors are
   classified by spelling alone (Clause 5.5.2). Tuple, record, and list
   patterns share braces; the node records the items as written and the
   expected type decides the family (Clause 6.8.1). */

static ek_node *primary_pattern(p_ctx *c);
static ek_node *lit_pattern(p_ctx *c, ek_node *lit);
static ek_node *name_pattern(p_ctx *c, const ek_layout_token *t);
static ek_node *string_pattern(p_ctx *c, const ek_layout_token *t);
static ek_node *minus_pattern(p_ctx *c, const ek_layout_token *t);
static ek_node *brace_pattern(p_ctx *c, const ek_layout_token *open);
static ek_node *paren_pattern(p_ctx *c, const ek_layout_token *open);
static void brace_items(p_ctx *c, ek_node *n);
static ek_node *brace_item(p_ctx *c);

/* Pattern := OrPattern: P | Q matches either alternative (6.8.3). */
ek_node *p_pattern(p_ctx *c)
{
    ek_node *first = primary_pattern(c);
    ek_node *n;

    if (!first || !p_at(c, EK_TOK_PIPE)) {
        return first;
    }
    n = p_node(c, EK_N_P_OR, first->line, first->col);
    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.kids);
    p_list_add(c, &n->u.kids, first);
    while (p_got(c, EK_TOK_PIPE)) {
        ek_node *alt = primary_pattern(c);

        if (!alt) {
            return NULL;
        }
        p_list_add(c, &n->u.kids, alt);
    }
    return n;
}

static void dotted_name(p_ctx *c, const ek_layout_token *first,
                        ek_slist *path)
{
    p_path_init(path);
    p_path_add(c, path, first->tok.text);
    while (p_at(c, EK_TOK_DOT)
           && p_peek_at(c, 1)->kind == EK_LTOK_SOURCE
           && p_peek_at(c, 1)->tok.kind == EK_TOK_IDENT) {
        const ek_layout_token *seg;

        p_take(c);
        seg = p_take(c);
        p_path_add(c, path, seg->tok.text);
    }
}

/* PrimaryPattern of A.3.6. */
static ek_node *primary_pattern(p_ctx *c)
{
    const ek_layout_token *t = p_peek(c);

    switch (p_src_kind(t)) {
    case EK_TOK_IDENT:
        return name_pattern(c, t);
    case EK_TOK_INT:
    case EK_TOK_FLOAT:
    case EK_TOK_CHAR:
        return lit_pattern(c, p_literal(c, p_take(c)));
    case EK_TOK_STRING:
        return string_pattern(c, t);
    case EK_TOK_MINUS:
        return minus_pattern(c, t);
    case EK_TOK_LPAREN:
        return paren_pattern(c, t);
    case EK_TOK_LBRACE:
        return brace_pattern(c, t);
    default:
        p_error_tok(c, "E-syntax", t, "expected a pattern");
        return NULL;
    }
}

/* An identifier pattern: a (qualified) constructor or a variable,
   classified by spelling (Clause 5.5.2, A.3.6). */
static ek_node *name_pattern(p_ctx *c, const ek_layout_token *t)
{
    ek_slist path;

    p_take(c);
    dotted_name(c, t, &path);
    /* A dotted head is a qualified constructor `Mod.Name`
       (Clause 7.4.6); the module segment is snake_case. */
    if (path.count > 1 || p_is_pascal(path.data[0])) {
        ek_node *n = p_node(c, EK_N_P_CONS, t->line, t->col);

        if (!n) {
            return NULL;
        }
        n->u.cons.name = path;
        n->u.cons.payload = NULL;
        if (p_pattern_start(p_src_kind(p_peek(c)))) {
            n->u.cons.payload = primary_pattern(c);
        }
        return n;
    }
    if (strcmp(path.data[0], "_") == 0) {
        return p_node(c, EK_N_P_WILDCARD, t->line, t->col);
    }
    {
        ek_node *n = p_node(c, EK_N_P_VAR, t->line, t->col);

        if (n) {
            n->u.path = path;
        }
        return n;
    }
}

/* A string pattern carries no interpolation (Clause 6.8.5). */
static ek_node *string_pattern(p_ctx *c, const ek_layout_token *t)
{
    ek_node *lit = p_literal(c, p_take(c));

    if (lit && lit->kind == EK_N_STRING_LIT) {
        size_t i;

        for (i = 0; i < lit->u.str.count; i++) {
            if (lit->u.str.segs[i].expr) {
                p_error(c, "E-interp-pattern", t->line, t->col,
                        "an interpolated string is an expression, not "
                        "a literal pattern (Clause 6.8.5)");
            }
        }
    }
    return lit_pattern(c, lit);
}

/* `-` and a numeric literal (Clause 6.8.5). */
static ek_node *minus_pattern(p_ctx *c, const ek_layout_token *t)
{
    ek_token_kind k;
    ek_node *lit;
    ek_node *neg;

    p_take(c);
    k = p_src_kind(p_peek(c));
    if (k != EK_TOK_INT && k != EK_TOK_FLOAT) {
        p_error(c, "E-syntax", p_peek(c)->line, p_peek(c)->col,
                "a negative pattern is `-` and a numeric literal "
                "(Clause 6.8.5)");
        return NULL;
    }
    lit = p_literal(c, p_take(c));
    if (!lit) {
        return NULL;
    }
    neg = p_node(c, EK_N_UNARY, t->line, t->col);
    if (!neg) {
        return NULL;
    }
    neg->u.wrap.inner = lit;
    return lit_pattern(c, neg);
}

/* P_LIT wraps the literal expression, including the negative numeric
   form (Clause 6.8.5). */
static ek_node *lit_pattern(p_ctx *c, ek_node *lit)
{
    ek_node *n;

    if (!lit) {
        return NULL;
    }
    n = p_node(c, EK_N_P_LIT, lit->line, lit->col);
    if (n) {
        n->u.wrap.inner = lit;
    }
    return n;
}

/* '(' Pattern { ',' Pattern } ')': the tuple group of a binding line
   or a multi-scrutinee arm (A.3.6). */
static ek_node *paren_pattern(p_ctx *c, const ek_layout_token *open)
{
    ek_node *n = p_node(c, EK_N_P_PAREN, open->line, open->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.kids);
    p_take(c);
    for (;;) {
        ek_node *p = p_pattern(c);

        p_list_add(c, &n->u.kids, p);
        if (!p || c->oom) {
            return n;
        }
        if (p_got(c, EK_TOK_COMMA)) {
            continue;
        }
        if (p_got(c, EK_TOK_RPAREN)) {
            return n;
        }
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `)` in the pattern group");
        p_sync(c);
        p_got(c, EK_TOK_RPAREN);
        return n;
    }
}

/* '{' '}' | elements | elements ',' '..' [name] | '..' [name] ','
   elements | fields — the shared brace syntax of Clause 6.8. */
static ek_node *brace_pattern(p_ctx *c, const ek_layout_token *open)
{
    ek_node *n = p_node(c, EK_N_P_BRACE, open->line, open->col);

    if (!n) {
        return NULL;
    }
    n->u.p_brace.rest = EK_REST_NONE;
    n->u.p_brace.rest_name = NULL;
    p_list_init(&n->u.p_brace.items);
    p_take(c);
    if (p_got(c, EK_TOK_RBRACE)) {
        return n;
    }
    if (p_at(c, EK_TOK_DOTDOT)) {
        p_take(c);
        n->u.p_brace.rest = EK_REST_PREFIX;
        if (p_at(c, EK_TOK_IDENT)) {
            n->u.p_brace.rest_name = p_take(c)->tok.text;
        }
        if (!p_got(c, EK_TOK_COMMA)) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "a prefix rest is followed by `,` and the last "
                        "elements (Clause 6.8.4)");
            p_sync(c);
            p_got(c, EK_TOK_RBRACE);
            return n;
        }
        brace_items(c, n);
        return n;
    }
    brace_items(c, n);
    return n;
}

/* Items until '}'; a `..` after the items is the suffix rest of
   Clause 6.8.4. */
static void brace_items(p_ctx *c, ek_node *n)
{
    for (;;) {
        ek_node *item;

        if (p_got(c, EK_TOK_RBRACE)) {
            return;
        }
        if (p_at(c, EK_TOK_DOTDOT)) {
            p_take(c);
            n->u.p_brace.rest = EK_REST_SUFFIX;
            if (p_at(c, EK_TOK_IDENT)) {
                n->u.p_brace.rest_name = p_take(c)->tok.text;
            }
            p_got(c, EK_TOK_RBRACE);
            return;
        }
        item = brace_item(c);
        p_list_add(c, &n->u.p_brace.items, item);
        if (!item || c->oom) {
            return;
        }
        if (p_got(c, EK_TOK_COMMA)) {
            continue;
        }
        if (p_got(c, EK_TOK_RBRACE)) {
            return;
        }
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `}` in the pattern");
        p_sync(c);
        p_got(c, EK_TOK_RBRACE);
        return;
    }
}

/* One item: a field `name [: Pattern]` or an element pattern. A bare
   identifier stays an element; the record reading of Clause 6.8.2
   reinterprets it as the shorthand field. */
static ek_node *brace_item(p_ctx *c)
{
    const ek_layout_token *t = p_peek(c);
    const ek_layout_token *next = p_peek_at(c, 1);

    if (p_src_kind(t) == EK_TOK_IDENT && p_src_kind(next) == EK_TOK_COLON) {
        ek_node *pat;
        ek_node *n = p_node(c, EK_N_P_FIELD, t->line, t->col);

        p_take(c);
        p_take(c);
        if (!n) {
            return NULL;
        }
        n->u.p_field.name = t->tok.text;
        pat = p_pattern(c);
        if (!pat) {
            return NULL;
        }
        n->u.p_field.pattern = pat;
        return n;
    }
    return primary_pattern(c);
}
