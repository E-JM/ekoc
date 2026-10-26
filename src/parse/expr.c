#include "parse/internal.h"

/* The expression ladder: one function per precedence level, loosest at
   the top. Comparison and range are non-associative: a chain is
   diagnosed once and then folded left so the rest of the module still
   parses. The tightest level, the operand layer, is postfix.c. */

static ek_node *mul(p_ctx *c);
static ek_node *unary(p_ctx *c);
static ek_node *range(p_ctx *c);

static ek_node *bin(p_ctx *c, ek_binop op, ek_node *lhs, ek_node *rhs)
{
    ek_node *n;

    if (!lhs || !rhs) {
        return NULL;
    }
    n = p_node(c, EK_N_BINARY, lhs->line, lhs->col);
    if (n) {
        n->u.binary.op = op;
        n->u.binary.lhs = lhs;
        n->u.binary.rhs = rhs;
    }
    return n;
}

static ek_binop add_op(ek_token_kind k)
{
    switch (k) {
    case EK_TOK_PLUS: return EK_BIN_ADD;
    case EK_TOK_MINUS: return EK_BIN_SUB;
    default: return EK_BIN_CONCAT;
    }
}

static ek_binop mul_op(ek_token_kind k)
{
    switch (k) {
    case EK_TOK_STAR: return EK_BIN_MUL;
    case EK_TOK_SLASH: return EK_BIN_DIV;
    default: return EK_BIN_REM;
    }
}

static ek_binop cmp_op(ek_token_kind k)
{
    switch (k) {
    case EK_TOK_EQEQ: return EK_BIN_EQ;
    case EK_TOK_BANGEQ: return EK_BIN_NE;
    case EK_TOK_LT: return EK_BIN_LT;
    case EK_TOK_LTEQ: return EK_BIN_LE;
    case EK_TOK_GT: return EK_BIN_GT;
    default: return EK_BIN_GE;
    }
}

static int is_cmp(ek_token_kind k)
{
    return k == EK_TOK_EQEQ || k == EK_TOK_BANGEQ || k == EK_TOK_LT
        || k == EK_TOK_LTEQ || k == EK_TOK_GT || k == EK_TOK_GTEQ;
}

static int is_range(ek_token_kind k)
{
    return k == EK_TOK_DOTDOT || k == EK_TOK_DOTDOTEQ;
}

/* A `case` or `if` used as an operand is parenthesized; the whole
   right-hand side or the whole argument escapes the check because
   those positions never pass through here. */
static ek_node *operand(p_ctx *c, ek_node *e)
{
    if (e && (e->kind == EK_N_IF || e->kind == EK_N_CASE)) {
        p_error(c, "E-operand-block", e->line, e->col,
                "a `case` or `if` operand is parenthesized (Clause 6.5.1)");
    }
    return e;
}

static ek_node *add(p_ctx *c)
{
    ek_node *e = mul(c);

    while (e && !c->oom) {
        ek_token_kind k = p_src_kind(p_peek(c));

        if (k != EK_TOK_PLUS && k != EK_TOK_MINUS
            && k != EK_TOK_PLUSPLUS) {
            return e;
        }
        p_take(c);
        e = bin(c, add_op(k), e, operand(c, mul(c)));
    }
    return e;
}

static ek_node *mul(p_ctx *c)
{
    ek_node *e = unary(c);

    while (e && !c->oom) {
        ek_token_kind k = p_src_kind(p_peek(c));

        if (k != EK_TOK_STAR && k != EK_TOK_SLASH
            && k != EK_TOK_PERCENT) {
            return e;
        }
        p_take(c);
        e = bin(c, mul_op(k), e, operand(c, unary(c)));
    }
    return e;
}

/* Unary: an optional '-' over a postfix expression; no unary `+`, and
   `-` never chains. */
static ek_node *unary(p_ctx *c)
{
    const ek_layout_token *minus = p_peek(c);
    ek_node *val;
    ek_node *n;

    if (p_src_kind(minus) != EK_TOK_MINUS) {
        return p_postfix(c);
    }
    p_take(c);
    val = operand(c, p_postfix(c));
    if (!val) {
        return NULL;
    }
    n = p_node(c, EK_N_UNARY, minus->line, minus->col);
    if (n) {
        n->u.wrap.inner = val;
    }
    return n;
}

/* Level 6: one comparison; `a < b < c` is an error. The error fires
   on the second fold, whatever the left operand is. */
static ek_node *comparison(p_ctx *c)
{
    ek_node *e = range(c);
    int folded = 0;

    while (e && !c->oom && is_cmp(p_src_kind(p_peek(c)))) {
        ek_binop op = cmp_op(p_take(c)->tok.kind);
        ek_node *rhs = operand(c, range(c));

        if (!rhs) {
            return NULL;
        }
        if (folded) {
            p_error(c, "E-chained-comparison", e->line, e->col,
                    "comparison operators do not associate; write "
                    "`a < b && b < c` (Clause 6.5.5)");
        }
        folded = 1;
        e = bin(c, op, e, rhs);
    }
    return e;
}

/* Level 5: one range operator; a chain over a range requires
   parentheses. */
static ek_node *range(p_ctx *c)
{
    ek_node *e = add(c);
    int folded = 0;

    while (e && !c->oom && is_range(p_src_kind(p_peek(c)))) {
        ek_binop op = p_take(c)->tok.kind == EK_TOK_DOTDOT
            ? EK_BIN_RANGE_EXC : EK_BIN_RANGE_INC;
        ek_node *rhs = operand(c, add(c));

        if (!rhs) {
            return NULL;
        }
        if (folded) {
            p_error(c, "E-chained-range", e->line, e->col,
                    "range operators do not associate; parenthesize the "
                    "range (Clause 6.5.5)");
        }
        folded = 1;
        e = bin(c, op, e, rhs);
    }
    return e;
}

static ek_node *and_expr(p_ctx *c)
{
    ek_node *e = comparison(c);

    while (e && !c->oom && p_got(c, EK_TOK_ANDAND)) {
        e = bin(c, EK_BIN_AND, e, operand(c, comparison(c)));
    }
    return e;
}

static ek_node *or_expr(p_ctx *c)
{
    ek_node *e = and_expr(c);

    while (e && !c->oom && p_got(c, EK_TOK_OROR)) {
        e = bin(c, EK_BIN_OR, e, operand(c, and_expr(c)));
    }
    return e;
}

ek_node *p_expr(p_ctx *c)
{
    ek_node *e;

    if (c->oom) {
        return NULL;
    }
    if (!p_depth_enter(c)) {
        return NULL;
    }
    e = or_expr(c);
    p_depth_leave(c);
    return e;
}
