#include "parse/internal.h"

/* Expression := OrExpression and the ladder of A.3.5, which fixes the
   precedence of Clause 6.5.5. Comparison and range are non-associative:
   a chain is diagnosed once and then folded left so the rest of the
   module still parses. */

static ek_node *mul(p_ctx *c);
static ek_node *unary(p_ctx *c);
static ek_node *range(p_ctx *c);
static ek_node *postfix(p_ctx *c);
static ek_node *primary(p_ctx *c);
static ek_node *apply_ctor(p_ctx *c, ek_node *head);
static ek_node *dot_link(p_ctx *c, ek_node *e);

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
   those positions never pass through here (Clause 6.5.1). */
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

/* UnaryExpression := [ '-' ] PostfixExpression: no unary `+`, and `-`
   never chains (Clause 6.5.4). */
static ek_node *unary(p_ctx *c)
{
    const ek_layout_token *minus = p_peek(c);
    ek_node *val;
    ek_node *n;

    if (p_src_kind(minus) != EK_TOK_MINUS) {
        return postfix(c);
    }
    p_take(c);
    val = operand(c, postfix(c));
    if (!val) {
        return NULL;
    }
    n = p_node(c, EK_N_UNARY, minus->line, minus->col);
    if (n) {
        n->u.wrap.inner = val;
    }
    return n;
}

/* Level 6: one comparison; `a < b < c` is an error (Clause 6.5.5).
   The error fires on the second fold, whatever the left operand is. */
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
   parentheses (Clause 6.5.6). */
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
    if (c->oom) {
        return NULL;
    }
    return or_expr(c);
}

/* Is e a name chain that can head a constructor application: every
   segment PascalCase, no call or application yet (Clause 6.5.2). */
int p_cons_head(const ek_node *e)
{
    switch (e->kind) {
    case EK_N_NAME:
        return e->u.path.count == 1 && p_is_pascal(e->u.path.data[0]);
    case EK_N_FIELD:
        return p_is_pascal(e->u.field.name) && p_cons_head(e->u.field.recv);
    default:
        return 0;
    }
}

/* ConstructorExpression := PascalName PostfixExpression; the argument
   is one postfix expression, so juxtaposition binds tighter than every
   operator (Clause 6.5.2, 6.5.5). */
static ek_node *apply_ctor(p_ctx *c, ek_node *head)
{
    ek_node *arg = postfix(c);
    ek_node *n;

    if (!arg) {
        return NULL;
    }
    n = p_node(c, EK_N_APPLY, head->line, head->col);
    if (n) {
        n->u.pair.a = head;
        n->u.pair.b = arg;
    }
    return n;
}

/* Postfix := '.' Identifier [ '(' args ')' ], level 1 of Clause 6.5.5.
   Index syntax does not exist (Clause 6.10). Dotted references and
   receiver chains share FIELD and CALL; Clause 7.4.4 classifies. */
static ek_node *postfix(p_ctx *c)
{
    ek_node *e = primary(c);

    while (e && !c->oom) {
        const ek_layout_token *t = p_peek(c);

        if (p_src_kind(t) == EK_TOK_LBRACKET) {
            p_error(c, "E-index-syntax", t->line, t->col,
                    "index syntax `x[i]` is not a form; use `get` "
                    "(Clause 6.10)");
            return e;
        }
        if (p_src_kind(t) != EK_TOK_DOT) {
            if (p_src_kind(t) == EK_TOK_LPAREN && !p_cons_head(e)
                && (e->kind == EK_N_NAME || e->kind == EK_N_FIELD)) {
                /* Callee '(' args ')' — prefix or receiver call (A.3.5) */
                ek_node *call = p_node(c, EK_N_CALL, e->line, e->col);

                if (!call) {
                    return NULL;
                }
                call->u.call.callee = e;
                p_call_args(c, &call->u.call.args);
                e = call;
                continue;
            }
            if (p_apply_start(p_src_kind(t)) && p_cons_head(e)) {
                e = apply_ctor(c, e);
                continue;
            }
            return e;
        }
        e = dot_link(c, e);
    }
    return e;
}

/* Postfix := '.' Identifier [ '(' args ')' ]: the link always builds
   the FIELD, and the call wraps it (A.3.5). */
static ek_node *dot_link(p_ctx *c, ek_node *e)
{
    const ek_layout_token *name;
    ek_node *field;

    p_take(c); /* the dot */
    name = p_expect(c, EK_TOK_IDENT, "a member name");
    if (!name) {
        return NULL;
    }
    field = p_node(c, EK_N_FIELD, e->line, e->col);
    if (!field) {
        return NULL;
    }
    field->u.field.recv = e;
    field->u.field.name = name->tok.text;
    if (!p_at(c, EK_TOK_LPAREN)) {
        return field;
    }
    {
        ek_node *call = p_node(c, EK_N_CALL, e->line, e->col);

        if (!call) {
            return NULL;
        }
        call->u.call.callee = field;
        p_call_args(c, &call->u.call.args);
        return call;
    }
}

/* PrimaryExpression (A.3.5): literals, names, calls, parenthesized
   expressions, lambdas, case, if, and composite literals. */
static ek_node *primary(p_ctx *c)
{
    const ek_layout_token *t = p_peek(c);

    switch (p_src_kind(t)) {
    case EK_TOK_INT:
    case EK_TOK_FLOAT:
    case EK_TOK_CHAR:
    case EK_TOK_STRING:
        return p_literal(c, p_take(c));
    case EK_TOK_IDENT: {
        ek_node *n = p_node(c, EK_N_NAME, t->line, t->col);

        p_take(c);
        if (n) {
            p_path_init(&n->u.path);
            p_path_add(c, &n->u.path, t->tok.text);
        }
        return n;
    }
    case EK_TOK_LPAREN:
        return p_paren_primary(c);
    case EK_TOK_LBRACE:
        return p_composite(c);
    case EK_TOK_KW_CASE:
        return p_case_expr(c);
    case EK_TOK_KW_IF:
        return p_if_expr(c);
    case EK_TOK_LBRACKET:
        p_error(c, "E-value-bracket", t->line, t->col,
                "value position has no `[...]`; braces are the only value "
                "brackets (Clause 6.10)");
        return NULL;
    default:
        p_error_tok(c, "E-syntax", t, "expected an expression");
        return NULL;
    }
}

/* '(' [ Argument { ',' Argument } ] ')' (A.3.5); breaks inside brackets
   are free formatting (Clause 6.2.4). The '(' is consumed. */
void p_call_args(p_ctx *c, ek_nlist *args)
{
    p_list_init(args);
    p_take(c); /* '(' */
    p_skip_newlines(c);
    if (p_got(c, EK_TOK_RPAREN)) {
        return;
    }
    for (;;) {
        ek_node *arg = p_expr(c);

        p_list_add(c, args, arg);
        if (c->oom) {
            return;
        }
        p_skip_newlines(c);
        if (p_got(c, EK_TOK_COMMA)) {
            p_skip_newlines(c);
            if (p_at(c, EK_TOK_RPAREN)) {
                p_error_tok(c, "E-syntax", p_peek(c),
                            "no argument after `,`");
            }
            continue;
        }
        if (p_got(c, EK_TOK_RPAREN)) {
            return;
        }
        p_error_tok(c, "E-syntax", p_peek(c),
                    "expected `,` or `)` in the argument list");
        p_sync(c);
        p_got(c, EK_TOK_RPAREN);
        return;
    }
}
