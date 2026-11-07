#include "parse/internal.h"

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

/* A lambda: '(' parameters ')' ':' body. */
ek_node *p_lambda(p_ctx *c)
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
