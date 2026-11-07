#include "parse/internal.h"

/* Member bodies. A block body is NEWLINE INDENT { Member } DEDENT; a
   `;`-separated single-line suite is the same member sequence on one
   line, so both produce EK_N_BLOCK. */

ek_node *p_member_list(p_ctx *c)
{
    const ek_layout_token *first = p_peek(c);
    ek_node *block = p_node(c, EK_N_BLOCK, first->line, first->col);

    if (!block) {
        return NULL;
    }
    p_list_init(&block->u.kids);
    while (!c->oom) {
        size_t before = c->pos;
        ek_node *m;

        if (p_got_layout(c, EK_LTOK_DEDENT)
            || p_at_layout(c, EK_LTOK_EOF)) {
            break;
        }
        if (p_got_layout(c, EK_LTOK_NEWLINE) || p_got(c, EK_TOK_SEMI)) {
            continue;
        }
        m = p_member(c, 0);
        if (m) {
            p_list_add(c, block, &block->u.kids, m);
        }
        if (c->pos == before) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "member is malformed; skipped");
            p_take(c);
        }
    }
    return block;
}

ek_node *p_block_suite(p_ctx *c)
{
    if (!p_got_layout(c, EK_LTOK_NEWLINE)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected a block body (Clause 6.2.2)");
        return NULL;
    }
    if (!p_got_layout(c, EK_LTOK_INDENT)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected an indented block");
        return NULL;
    }
    return p_member_list(c);
}

ek_node *p_inline_suite(p_ctx *c)
{
    ek_node *first = p_expr(c);
    ek_node *block;

    if (!first) {
        return NULL;
    }
    if (!p_at(c, EK_TOK_SEMI)) {
        return first;
    }
    block = p_node(c, EK_N_BLOCK, first->line, first->col);
    if (!block) {
        return NULL;
    }
    p_list_init(&block->u.kids);
    p_list_add(c, block, &block->u.kids, first);
    while (p_got(c, EK_TOK_SEMI)) {
        ek_node *e = p_expr(c);

        if (!e) {
            break;
        }
        p_list_add(c, block, &block->u.kids, e);
    }
    return block;
}

/* The body after a lambda, `if`, or `else` header. */
ek_node *p_body_suite(p_ctx *c)
{
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        return p_block_suite(c);
    }
    return p_inline_suite(c);
}

/* A binding right-hand side: one expression, or a block on the
   following lines. */
ek_node *p_binding_rhs(p_ctx *c)
{
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        return p_block_suite(c);
    }
    return p_expr(c);
}
