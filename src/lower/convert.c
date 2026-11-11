#include <string.h>

#include "lower/internal.h"

/* The conversion member's lowering: on a newtype receiver, the payload
   type's own name reads the sole constructor's payload. The operand
   lowers to a temp, the payload reads out of the constructor family's
   single slot as an owned keep, and the temp dies. Three shapes reach
   here: the receiver form's operand is the receiver node; the
   type-namespace form's is the one written argument; and a qualified
   chain's is the value the chain's module and type segments peel down
   to. */

/* The sole constructor one newtype declaration owns, in its declaring
   module's surface; NULL when the surface names none. */
static const ek_symbol *sole_ctor(lower_ctx *c, const ek_type *ot)
{
    const ek_symbol *tsym = ot && ot->kind == EK_T_NOMINAL
                                ? ot->u.nominal.sym : NULL;
    size_t k, i;

    if (!tsym || !tsym->module || !tsym->name) {
        return NULL;
    }
    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, tsym->module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, tsym->name) == 0) {
                return m; /* the sole-constructor rule accepted it */
            }
        }
    }
    return NULL;
}

lower_val lower_convert_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const call_form *form, const ek_type *rt)
{
    const ek_type *ot = rt;
    const ek_node *operand = callee->u.field.recv;
    const ek_symbol *cs;
    lower_gtype *g;
    lower_val a;
    const char *at, *r;

    /* An impossible arity is check's report; the balance still
       evaluates and dies, one defect one diagnostic. */
    if (form->extra) {
        return lower_discard_call(c, scope, e, form);
    }
    if (rt) {
        if (e->u.call.args.count != 0) {
            return lower_discard_call(c, scope, e, form);
        }
    } else if (e->u.call.args.count == 1) {
        operand = e->u.call.args.data[0];
        ot = lower_row_type(c, operand);
    } else if (e->u.call.args.count == 0) {
        /* A qualified chain x.Mod.Type.P(): the value sits beneath the
           module and type segments. */
        operand = module_path_recv(c, callee->u.field.recv);
        if (!operand) {
            return lower_discard_call(c, scope, e, form);
        }
        ot = lower_row_type(c, operand);
    } else {
        return lower_discard_call(c, scope, e, form);
    }
    cs = sole_ctor(c, ot);
    g = cs ? lower_ctor_of_symbol(c, e, cs, 0) : NULL;
    if (!g || !g->nf) {
        lower_error(c, e, "the conversion operand carries no payload");
        return lower_discard_call(c, scope, e, form);
    }
    a = lower_sub(c, scope, operand);
    at = lower_as_temp(c, scope, &a);
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_keep(((ekS_%s *)%s.as.ref)->p0);\n",
                r, g->stem, at);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", at);
    return (lower_val){ r, 1 };
}
