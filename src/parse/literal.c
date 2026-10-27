#include "parse/internal.h"

/* Literals, lambda bodies, composite literals, and string
   interpolation. */

static ek_node *lambda(p_ctx *c);
static ek_node *lambda_param(p_ctx *c);
static ek_node *string_lit(p_ctx *c, const ek_layout_token *t);
static ek_node *composite_item(p_ctx *c);

ek_node *p_literal(p_ctx *c, const ek_layout_token *t)
{
    ek_node *n;

    switch (t->tok.kind) {
    case EK_TOK_INT:
        n = p_node(c, EK_N_INT_LIT, t->line, t->col);
        if (n) {
            n->u.int_lit.text = t->tok.text;
            n->u.int_lit.magnitude = t->tok.magnitude;
            n->u.int_lit.overflow = t->tok.overflow;
            n->u.int_lit.base = t->tok.base;
        }
        return n;
    case EK_TOK_FLOAT:
        n = p_node(c, EK_N_FLOAT_LIT, t->line, t->col);
        if (n) {
            n->u.float_lit.text = t->tok.text;
            n->u.float_lit.value = t->tok.fvalue;
        }
        return n;
    case EK_TOK_CHAR:
        n = p_node(c, EK_N_CHAR_LIT, t->line, t->col);
        if (n) {
            n->u.char_lit.scalar = t->tok.scalar;
        }
        return n;
    default:
        return string_lit(c, t);
    }
}

/* EK_N_STRING_LIT: one segment per part, slots parsed with the
   expression grammar. */
static ek_node *string_lit(p_ctx *c, const ek_layout_token *t)
{
    ek_node *n = p_node(c, EK_N_STRING_LIT, t->line, t->col);
    size_t i;

    if (!n) {
        return NULL;
    }
    n->u.str.count = t->tok.part_count;
    n->u.str.segs = NULL;
    if (t->tok.part_count > 0) {
        n->u.str.segs = p_alloc(c, t->tok.part_count * sizeof(ek_str_seg));
        if (!n->u.str.segs) {
            return NULL;
        }
    }
    for (i = 0; i < t->tok.part_count; i++) {
        const ek_string_part *p = &t->tok.parts[i];
        ek_str_seg *seg = &n->u.str.segs[i];

        seg->text = NULL;
        seg->text_len = 0;
        seg->expr = NULL;
        if (p->tokens) {
            seg->expr = p_adopt(c, n, p_slot_expr(c, p->tokens,
                                                p->token_count));
        } else {
            seg->text = p->text;
            seg->text_len = p->text_len;
        }
    }
    return n;
}

/* One interpolation slot: the raw token run is re-wrapped as a stream
   and parsed by the same expression entry, so `{n + 1}` and a nested
   string slot are ordinary expressions. */
ek_node *p_slot_expr(p_ctx *c, const ek_token *toks, size_t n)
{
    ek_layout_token *lay = p_alloc(c, (n + 1) * sizeof *lay);
    p_ctx sub = *c;
    ek_node *e;
    size_t i;

    if (!lay) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        lay[i].kind = EK_LTOK_SOURCE;
        lay[i].line = toks[i].line;
        lay[i].col = toks[i].col;
        lay[i].tok = toks[i];
    }
    lay[n].kind = EK_LTOK_EOF;
    lay[n].line = n > 0 ? toks[n - 1].line : 1;
    lay[n].col = n > 0 ? toks[n - 1].col : 1;
    sub.toks = lay;
    sub.count = n + 1;
    sub.pos = 0;
    sub.oom = c->oom;
    sub.match = NULL;
    sub.match_stack = NULL;
    e = p_expr(&sub);
    if (!sub.oom && !sub.fatal && e
        && p_peek(&sub)->kind == EK_LTOK_SOURCE) {
        p_error(c, "E-slot-expr", p_peek(&sub)->line, p_peek(&sub)->col,
                "an interpolation slot holds one expression (Clause 5.9.3)");
    }
    if (sub.oom) {
        c->oom = 1;
    }
    if (sub.fatal) {
        c->fatal = 1;
        c->depth_hit |= sub.depth_hit;
    }
    return e;
}

/* A composite literal: '{' items '}', with breaks inside braces free.
   Keyed entries and elements share the node; the expected type decides
   the family. */
ek_node *p_composite(p_ctx *c)
{
    const ek_layout_token *open = p_take(c);
    ek_node *n = p_node(c, EK_N_COMPOSITE, open->line, open->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.kids);
    p_skip_newlines(c);
    if (p_got(c, EK_TOK_RBRACE)) {
        return n;
    }
    for (;;) {
        ek_node *item = composite_item(c);

        p_list_add(c, n, &n->u.kids, item);
        if (c->oom) {
            return n;
        }
        p_skip_newlines(c);
        if (p_got(c, EK_TOK_COMMA)) {
            p_skip_newlines(c);
            continue;
        }
        if (p_got(c, EK_TOK_RBRACE)) {
            return n;
        }
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `}` in the composite literal");
        p_sync(c);
        p_got(c, EK_TOK_RBRACE);
        return n;
    }
}

/* CompositeItem := Expression [ ':' Expression ]: the first expression
   is the value of an element or the key of a keyed entry. */
static ek_node *composite_item(p_ctx *c)
{
    ek_node *first = p_expr(c);
    ek_node *item;

    if (!first) {
        return NULL;
    }
    item = p_node(c, EK_N_COMPOSITE_ITEM, first->line, first->col);
    if (!item) {
        return NULL;
    }
    item->u.pair.a = NULL;
    item->u.pair.b = p_adopt(c, item, first);
    if (p_got(c, EK_TOK_COLON)) {
        item->u.pair.a = p_adopt(c, item, first);
        item->u.pair.b = p_adopt(c, item, p_expr(c));
    }
    if (!item->u.pair.b) {
        return NULL;
    }
    return item;
}

/* '(' in primary position: a parenthesized expression, or a lambda
   when the matching ')' is followed by ':' and the position is not
   an operator operand (the colon of an enclosing header wins
   there). */
ek_node *p_paren_primary(p_ctx *c)
{
    size_t close = p_match_bracket(c, c->pos);
    const ek_layout_token *after = p_peek_at(c, close - c->pos + 1);

    if (!c->operand_pos && !c->head_colon && after->kind == EK_LTOK_SOURCE
        && after->tok.kind == EK_TOK_COLON) {
        return lambda(c);
    }
    {
        const ek_layout_token *open = p_take(c);
        ek_node *inner = p_expr(c);
        ek_node *n;

        if (!inner) {
            return NULL;
        }
        if (!p_expect(c, EK_TOK_RPAREN, "')'")) {
            return NULL;
        }
        n = p_node(c, EK_N_PAREN, open->line, open->col);
        if (!n) {
            return NULL;
        }
        n->u.wrap.inner = p_adopt(c, n, inner);
        if (!n->u.wrap.inner) {
            return NULL;
        }
        return n;
    }
}

/* Index of the bracket matching the opener at from, counting all three
   bracket pairs; markers never carry brackets. */
size_t p_match_bracket(p_ctx *c, size_t from)
{
    size_t i;
    size_t top = 0;
    size_t last = c->count > 0 ? c->count - 1 : 0;

    if (from >= c->count) {
        return last;
    }
    if (!c->match) {
        c->match = p_alloc(c, c->count * sizeof *c->match);
        c->match_stack = p_alloc(c, c->count * sizeof *c->match_stack);
        if (!c->match || !c->match_stack) {
            return last;
        }
        for (i = 0; i < c->count; i++) {
            c->match[i] = (size_t)-1;
        }
    }
    if (c->match[from] != (size_t)-1) {
        return c->match[from];
    }
    for (i = from; i < c->count; i++) {
        const ek_layout_token *t = &c->toks[i];

        if (t->kind != EK_LTOK_SOURCE) {
            continue;
        }
        switch (t->tok.kind) {
        case EK_TOK_LPAREN:
        case EK_TOK_LBRACKET:
        case EK_TOK_LBRACE:
            c->match_stack[top++] = i;
            break;
        case EK_TOK_RPAREN:
        case EK_TOK_RBRACKET:
        case EK_TOK_RBRACE:
            if (top > 0) {
                size_t open = c->match_stack[--top];

                c->match[open] = i;
                c->match[i] = open;
                if (open == from) {
                    return i;
                }
            }
            break;
        default:
            break;
        }
    }
    return last;
}

/* A lambda: '(' parameters ')' ':' body. */
static ek_node *lambda(p_ctx *c)
{
    const ek_layout_token *open = p_take(c); /* '(' */
    ek_node *params = p_node(c, EK_N_LAMBDA_PARAMS, open->line, open->col);
    ek_node *body;
    ek_node *n;

    if (!params) {
        return NULL;
    }
    p_list_init(&params->u.kids);
    p_skip_newlines(c);
    if (!p_at(c, EK_TOK_RPAREN)) {
        for (;;) {
            ek_node *p = lambda_param(c);

            p_list_add(c, params, &params->u.kids, p);
            p_skip_newlines(c);
            if (!p_got(c, EK_TOK_COMMA)) {
                break;
            }
            p_skip_newlines(c);
        }
    }
    if (!p_expect(c, EK_TOK_RPAREN, "')'")) {
        return NULL;
    }
    if (!p_got(c, EK_TOK_COLON)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `:` after the lambda parameters");
        return NULL;
    }
    body = p_body_suite(c);
    if (!body) {
        return NULL;
    }
    n = p_node(c, EK_N_LAMBDA, open->line, open->col);
    if (!n) {
        return NULL;
    }
    n->u.pair.a = p_adopt(c, n, params);
    n->u.pair.b = p_adopt(c, n, body);
    if (!n->u.pair.a || !n->u.pair.b) {
        return NULL;
    }
    return n;
}

/* A lambda parameter: a name, '_', or a nested parameter group. The
   nested group is the recursion edge, so it carries the guard. */
static ek_node *lambda_param(p_ctx *c)
{
    const ek_layout_token *t = p_peek(c);

    if (p_src_kind(t) == EK_TOK_IDENT) {
        ek_node *n = p_node(c, EK_N_NAME, t->line, t->col);

        p_take(c);
        if (n) {
            p_path_init(&n->u.path);
            p_path_add(c, &n->u.path, t->tok.text);
        }
        return n;
    }
    if (p_src_kind(t) == EK_TOK_LPAREN) {
        ek_node *g = p_node(c, EK_N_LAMBDA_PARAMS, t->line, t->col);

        p_take(c);
        if (!g) {
            return NULL;
        }
        if (!p_depth_enter(c)) {
            return NULL;
        }
        p_list_init(&g->u.kids);
        p_skip_newlines(c);
        while (!p_at(c, EK_TOK_RPAREN) && !c->oom) {
            ek_node *p = lambda_param(c);

            p_list_add(c, g, &g->u.kids, p);
            p_skip_newlines(c);
            if (!p_got(c, EK_TOK_COMMA)) {
                break;
            }
            p_skip_newlines(c);
        }
        p_depth_leave(c);
        p_expect(c, EK_TOK_RPAREN, "')'");
        return g;
    }
    p_error_tok(c, "E-syntax", t, "expected a lambda parameter");
    return NULL;
}
