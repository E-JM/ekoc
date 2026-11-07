#include "parse/internal.h"

static ek_node *fn_params(p_ctx *c, ek_node *n);
static ek_node *fn_arrow(p_ctx *c, ek_node *n);

/* A parameter group followed by an arrow and a result type, by a bare
   '!' or '?', or by nothing. The elided forms carry result NULL,
   meaning `_`. */
ek_node *p_fn_type(p_ctx *c, const ek_layout_token *open)
{
    ek_node *n = p_node(c, EK_N_T_FN, open->line, open->col);

    if (!n) {
        return NULL;
    }
    p_list_init(&n->u.t_fn.params);
    n->u.t_fn.result = NULL;
    n->u.t_fn.arrow = EK_ARROW_PURE;
    p_take(c); /* '(' */
    if (!fn_params(c, n)) {
        return n;
    }
    return fn_arrow(c, n);
}

/* [ Parameter { ',' Parameter } ] then ')'; breaks are free. */
static ek_node *fn_params(p_ctx *c, ek_node *n)
{
    p_skip_newlines(c);
    if (p_at(c, EK_TOK_RPAREN)) {
        p_take(c);
        return n;
    }
    for (;;) {
        const ek_layout_token *t = p_peek(c);
        const char *label = NULL;
        ek_node *ty;
        ek_node *param;

        if (p_src_kind(t) == EK_TOK_IDENT
            && p_src_kind(p_peek_at(c, 1)) == EK_TOK_COLON) {
            label = p_take(c)->tok.text;
            p_take(c);
        }
        ty = p_type(c);
        if (!ty) {
            return NULL;
        }
        param = p_node(c, EK_N_T_PARAM, t->line, t->col);
        if (param) {
            param->u.t_field.name = label;
            param->u.t_field.type = p_adopt(c, param, ty);
            if (!param->u.t_field.type) {
                return NULL;
            }
        }
        p_list_add(c, n, &n->u.t_fn.params, param);
        p_skip_newlines(c);
        if (p_got(c, EK_TOK_COMMA)) {
            p_skip_newlines(c);
            continue;
        }
        break;
    }
    if (!p_expect(c, EK_TOK_RPAREN, "')'")) {
        p_sync(c);
        return NULL;
    }
    return n;
}

/* The arrow and suffix forms; elided forms carry result NULL. */
static ek_node *fn_arrow(p_ctx *c, ek_node *n)
{
    if (p_got(c, EK_TOK_ARROW)) {
        n->u.t_fn.result = p_adopt(c, n, p_type(c));
    } else if (p_got(c, EK_TOK_BANGARROW)) {
        n->u.t_fn.arrow = EK_ARROW_WORLD;
        n->u.t_fn.result = p_adopt(c, n, p_type(c));
    } else if (p_got(c, EK_TOK_QUESTIONARROW)) {
        n->u.t_fn.arrow = EK_ARROW_OPEN;
        n->u.t_fn.result = p_adopt(c, n, p_type(c));
    } else if (p_got(c, EK_TOK_BANG)) {
        n->u.t_fn.arrow = EK_ARROW_WORLD;
    } else if (p_got(c, EK_TOK_QUESTION)) {
        n->u.t_fn.arrow = EK_ARROW_OPEN;
    }
    return n;
}
