#include "parse/internal.h"

/* case and if. An arm is always `pattern -> body`; a bare expression
   is not an arm and has a dedicated error. The single-line form
   separates arms with `;`. */

static ek_node *arm(p_ctx *c);
static void arm_lines(p_ctx *c, ek_node *n);
static void arm_inline(p_ctx *c, ek_node *n);
static ek_node *else_branch(p_ctx *c);

/* A case expression: 'case' scrutinees ':' then either an indented arm
   block or a `;`-separated inline arm run. */
ek_node *p_case_expr(p_ctx *c)
{
    const ek_layout_token *kw = p_take(c);
    ek_node *n = p_node(c, EK_N_CASE, kw->line, kw->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.caseexpr.scrutinees);
    p_list_init(&n->u.caseexpr.arms);
    for (;;) {
        ek_node *e = p_expr(c);

        p_list_add(c, n, &n->u.caseexpr.scrutinees, e);
        if (!e || c->oom) {
            return n;
        }
        if (!p_got(c, EK_TOK_COMMA)) {
            break;
        }
    }
    if (!p_got(c, EK_TOK_COLON)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `:` before the case arms (Clause 6.7.1)");
        p_sync(c);
        return n;
    }
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        p_take(c);
        if (!p_got_layout(c, EK_LTOK_INDENT)) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "expected an indented arm block (Clause 6.7.1)");
            return n;
        }
        arm_lines(c, n);
        return n;
    }
    arm_inline(c, n);
    return n;
}

/* The indented arm block: each arm ends at NEWLINE, `;` carries arms
   on one line, DEDENT closes. */
static void arm_lines(p_ctx *c, ek_node *n)
{
    while (!c->oom) {
        size_t before = c->pos;
        ek_node *a;

        if (p_got_layout(c, EK_LTOK_DEDENT)
            || p_at_layout(c, EK_LTOK_EOF)) {
            return;
        }
        if (p_got_layout(c, EK_LTOK_NEWLINE) || p_got(c, EK_TOK_SEMI)) {
            continue;
        }
        a = arm(c);
        p_list_add(c, n, &n->u.caseexpr.arms, a);
        if (c->pos == before) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "expected a case arm; skipped");
            p_take(c);
        }
    }
}

/* The inline arm run after ':': `;` separates arms, and they run to
   the end of the member. */
static void arm_inline(p_ctx *c, ek_node *n)
{
    while (!c->oom) {
        ek_node *a = arm(c);

        p_list_add(c, n, &n->u.caseexpr.arms, a);
        if (!a || c->oom) {
            return;
        }
        if (!p_got(c, EK_TOK_SEMI)) {
            return;
        }
    }
}

/* One arm: one or more patterns (several for a multi-scrutinee arm),
   an optional `if` guard, '->', and the body. A body on the arm's own
   line is an expression; a body on the following lines is the indented
   member block. */
static ek_node *arm(p_ctx *c)
{
    const ek_layout_token *first = p_peek(c);
    ek_node *n = p_node(c, EK_N_ARM, first->line, first->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.arm.patterns);
    for (;;) {
        ek_node *p = p_pattern(c);

        p_list_add(c, n, &n->u.arm.patterns, p);
        if (!p || c->oom) {
            return n;
        }
        if (!p_got(c, EK_TOK_COMMA)) {
            break;
        }
        if (!p_pattern_start(p_src_kind(p_peek(c)))) {
            p_error_tok(c, "E-syntax", p_peek(c),
                        "expected a pattern after `,`");
            return n;
        }
    }
    n->u.arm.guard = NULL;
    n->u.arm.body = NULL;
    if (p_got(c, EK_TOK_KW_IF)) {
        n->u.arm.guard = p_adopt(c, n, p_expr(c));
        if (!n->u.arm.guard) {
            return n;
        }
    }
    if (!p_at(c, EK_TOK_ARROW)) {
        p_error_tok(c, "E-bare-case-arm", p_peek(c),
                    "an arm is `pattern -> expression`; a bare expression "
                    "is not an arm (Clause 6.7.1)");
        p_sync(c);
        return n;
    }
    p_take(c);
    if (p_at_layout(c, EK_LTOK_NEWLINE)) {
        n->u.arm.body = p_adopt(c, n, p_block_suite(c));
    } else {
        n->u.arm.body = p_adopt(c, n, p_expr(c));
    }
    return n;
}

/* An if expression: 'if' condition ':' suite, with an optional `else`
   suite. Layout emits the DEDENTs owed by the stack before a matching
   `else` line, so the parser reattaches it across NEWLINE and DEDENT
   markers. */
ek_node *p_if_expr(p_ctx *c)
{
    const ek_layout_token *kw = p_take(c);
    ek_node *cond;
    ek_node *then_body;
    ek_node *n;

    if (c->oom) {
        return NULL;
    }
    cond = p_expr(c);
    if (!cond) {
        p_sync(c);
        return NULL;
    }
    if (!p_got(c, EK_TOK_COLON)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `:` after the condition (Clause 6.7.4)");
        p_sync(c);
        return NULL;
    }
    then_body = p_body_suite(c);
    if (!then_body) {
        return NULL;
    }
    n = p_node(c, EK_N_IF, kw->line, kw->col);
    if (!n) {
        return NULL;
    }
    n->u.ifexpr.cond = p_adopt(c, n, cond);
    n->u.ifexpr.then = p_adopt(c, n, then_body);
    n->u.ifexpr.els = p_adopt(c, n, else_branch(c));
    if (!n->u.ifexpr.cond || !n->u.ifexpr.then) {
        return NULL;
    }
    return n;
}

/* 'else' Suite when present; the lookahead restores the cursor when no
   else belongs to this if. */
static ek_node *else_branch(p_ctx *c)
{
    size_t save = c->pos;
    ek_node *body;

    for (;;) {
        const ek_layout_token *t = p_peek(c);

        if (t->kind == EK_LTOK_DEDENT || t->kind == EK_LTOK_NEWLINE) {
            p_take(c);
            continue;
        }
        break;
    }
    if (!p_at(c, EK_TOK_KW_ELSE)) {
        c->pos = save;
        return NULL;
    }
    p_take(c);
    if (!p_got(c, EK_TOK_COLON)) {
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `:` after `else` (Clause 6.7.4)");
        p_sync(c);
        return NULL;
    }
    body = p_body_suite(c);
    return body;
}
