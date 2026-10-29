#include <string.h>

#include "lower/internal.h"

/* The self tail-call transform. A direct call to the function being
   emitted, standing in one of its tail positions (the body's final
   expression, if-branch tails, case-arm body tails, nested block
   tails), lowers to a frame-local jump: every argument evaluates into
   a temp first, then the abandoned references release, then each
   owned parameter takes its temp, and the body reenters at its retry
   label. The stack the call uses is constant. */

/* The reference of a call site, when it names the function being
   emitted directly. */
static int self_ref(lower_ctx *c, const ek_node *e, const ek_ref **out)
{
    const ek_ref *ref = lower_ref(c, e);

    if (!ref && e->u.call.callee) {
        ref = lower_ref(c, e->u.call.callee);
    }
    if (ref && ref->kind == EK_SYM_FUNC && ref->symbol
        && ref->symbol == c->tce_self) {
        *out = ref;
        return 1;
    }
    return 0;
}

/* Whether the expression holds a tail self-call. The walk mirrors the
   emitter's tail positions and never enters a lambda, an argument, or
   a binding's right-hand side. */
static int has_tail(lower_ctx *c, const ek_node *e)
{
    const ek_node *m;
    size_t i;

    if (!e) {
        return 0;
    }
    switch (e->kind) {
    case EK_N_BLOCK:
        if (e->u.kids.count == 0) {
            return 0;
        }
        m = e->u.kids.data[e->u.kids.count - 1];
        if (m->kind == EK_N_LET || m->kind == EK_N_REBIND
            || m->kind == EK_N_INVERT) {
            return 0;
        }
        return has_tail(c, m);
    case EK_N_PAREN:
        return has_tail(c, e->u.wrap.inner);
    case EK_N_BINARY:
        /* The logical operators expand to the if-form, and the right
           operand is a tail position; the left operand is the
           condition and never is. */
        return (e->u.binary.op == EK_BIN_AND
                || e->u.binary.op == EK_BIN_OR)
            && has_tail(c, e->u.binary.rhs);
    case EK_N_IF:
        return has_tail(c, e->u.ifexpr.then)
            || has_tail(c, e->u.ifexpr.els);
    case EK_N_CASE:
        for (i = 0; i < e->u.caseexpr.arms.count; i++) {
            if (has_tail(c, e->u.caseexpr.arms.data[i]->u.arm.body)) {
                return 1;
            }
        }
        return 0;
    case EK_N_CALL: {
        const ek_ref *ref;

        return self_ref(c, e, &ref);
    }
    default:
        return 0;
    }
}

/* Whether the function's body has a tail self-call, deciding the
   retry label. */
int lower_has_self_tail(lower_ctx *c, const ek_symbol *self,
                        const ek_node *body)
{
    const ek_symbol *saved = c->tce_self;
    int has;

    c->tce_self = self;
    has = has_tail(c, body);
    c->tce_self = saved;
    return has;
}

/* The owned locals of the scope chain up to the parameters, released
   innermost first. */
static void release_scopes(lower_ctx *c, lower_scope *scope)
{
    const lower_scope *s;

    for (s = scope; s; s = s->parent) {
        size_t i;

        if (s == c->tce_params) {
            break;
        }
        i = s->nloc;
        while (i > 0) {
            i--;
            if (!s->locs[i].is_param) {
                lower_pad(c);
                lower_emitf(c, "ek_release(%s);\n", s->locs[i].cname);
            }
        }
    }
}

/* The transform at one call site. The arguments evaluate before any
   old parameter releases, since an argument may read one. */
void lower_emit_self_tail(lower_ctx *c, lower_scope *scope,
                          const ek_node *e, const ek_ref *ref)
{
    const ek_node *ann = ref->symbol->type;
    const ek_nlist *args = &e->u.call.args;
    const char **argv = NULL;
    size_t cap = 0, n, i;

    n = ann && ann->kind == EK_N_T_FN ? ann->u.t_fn.params.count : 0;
    argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
    if (!argv && n) {
        c->oom = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        if (i < args->count) {
            int saved = c->in_tail;

            c->in_tail = 0;
            {
                lower_val a = lower_expr(c, scope, args->data[i]);

                c->in_tail = saved;
                argv[i] = lower_as_temp(c, scope, &a);
            }
        } else {
            lower_val a = lower_val_unit();

            argv[i] = lower_as_temp(c, scope, &a); /* a malformed arity */
        }
    }
    /* the case scrutinee temps and arm temps the jump abandons */
    lower_clean_release(c, 0);
    /* the owned locals of the enclosing scopes, innermost first */
    release_scopes(c, scope);
    /* the owned parameters take their temps, then the body reenters */
    for (i = 0; i < n && c->tce_params && i < c->tce_params->nloc; i++) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", c->tce_params->locs[i].cname);
        lower_pad(c);
        lower_emitf(c, "%s = %s;\n", c->tce_params->locs[i].cname, argv[i]);
    }
    lower_pad(c);
    lower_emitf(c, "goto %s;\n", c->tce_label);
}
