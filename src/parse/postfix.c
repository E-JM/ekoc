#include <string.h>

#include "parse/internal.h"

/* The level-1 operand layer of A.3.5: postfix member chains,
   constructor juxtaposition, primaries, and call argument lists. A
   `case` or `if` reaches this layer only as a whole argument or
   right-hand side (Clause 6.5.1); the operator ladder in expr.c checks
   its own operands. */

static ek_node *primary(p_ctx *c);
static ek_node *apply_ctor(p_ctx *c, ek_node *head);
static ek_node *dot_link(p_ctx *c, ek_node *e);
static void call_args(p_ctx *c, ek_nlist *args);

/* Is e a name chain that can head a constructor application: every
   segment PascalCase, no call or application yet (Clause 6.5.2). */
static int cons_head(const ek_node *e)
{
    switch (e->kind) {
    case EK_N_NAME:
        return e->u.path.count == 1 && p_is_pascal(e->u.path.data[0]);
    case EK_N_FIELD:
        return p_is_pascal(e->u.field.name) && cons_head(e->u.field.recv);
    default:
        return 0;
    }
}

/* ConstructorExpression := PascalName PostfixExpression; the argument
   is one postfix expression, so juxtaposition binds tighter than every
   operator (Clause 6.5.2, 6.5.5). */
static ek_node *apply_ctor(p_ctx *c, ek_node *head)
{
    ek_node *arg = p_postfix(c);
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
ek_node *p_postfix(p_ctx *c)
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
            if (p_src_kind(t) == EK_TOK_LPAREN && !cons_head(e)
                && (e->kind == EK_N_NAME || e->kind == EK_N_FIELD)) {
                /* Callee '(' args ')' — prefix or receiver call (A.3.5) */
                ek_node *call = p_node(c, EK_N_CALL, e->line, e->col);

                if (!call) {
                    return NULL;
                }
                call->u.call.callee = e;
                call_args(c, &call->u.call.args);
                e = call;
                continue;
            }
            if (p_apply_start(p_src_kind(t)) && cons_head(e)) {
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
        call_args(c, &call->u.call.args);
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
static void call_args(p_ctx *c, ek_nlist *args)
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
