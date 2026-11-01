#include <string.h>

#include "lower/internal.h"

/* The panicking observations of the Option and Result namespaces: the
   compiler rows `unwrap` and `expect` read the payload of the present
   variant and report through the runtime's panic when the variant is
   absent -- unwrap with the fixed message of its P-code, expect with
   its own, verbatim. The variants register through the ordinary
   constructor path, so the typeinfo the test reads joins the unit. */

/* The generated family of one std constructor, or NULL when std's
   surface does not carry it. */
static lower_gtype *std_ctor_family(lower_ctx *c, const ek_node *at,
                                    const char *owner, const char *name)
{
    size_t k, i;

    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, "std") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, owner) == 0
                && strcmp(m->name, name) == 0) {
                return lower_ctor_of_symbol(c, at, m, 0);
            }
        }
    }
    return NULL;
}

lower_val lower_unwrap_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_node *callee,
                            const char *family, const char *member,
                            int has_recv)
{
    int is_opt = strcmp(family, "Option") == 0;
    int is_res = strcmp(family, "Result") == 0;
    int is_expect = strcmp(member, "expect") == 0;
    int is_unwrap = strcmp(member, "unwrap") == 0;
    lower_gtype *g;
    lower_val a;
    const char *r, *t, *msg = NULL;
    unsigned d = c->depth;

    if ((!is_opt && !is_res) || (!is_expect && !is_unwrap) || !has_recv) {
        return (lower_val){ NULL, 0 };
    }
    if (e->u.call.args.count != (size_t)is_expect) {
        lower_error(c, e, "the call of `%s.%s` is malformed", family,
                    member);
        return lower_val_unit();
    }
    g = std_ctor_family(c, e, is_opt ? "Option" : "Result",
                        is_opt ? "Some" : "Ok");
    if (!g) {
        lower_error(c, e, "the variant `%s.%s` has no lowering",
                    is_opt ? "Option" : "Result", is_opt ? "Some" : "Ok");
        return lower_val_unit();
    }
    g->shared = 1; /* the typeinfo the dispatch tests joins the unit */
    /* the receiver and, for expect, the message evaluate in source
       order before the branch; the message's C text borrows the
       string, so it is read before the branch releases it */
    a = lower_sub(c, scope, callee->u.field.recv);
    r = lower_as_temp(c, scope, &a);
    if (is_expect) {
        a = lower_sub(c, scope, e->u.call.args.data[0]);
        msg = lower_as_temp(c, scope, &a);
    }
    t = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "if (%s.as.ref->info != &ekT_%s) {\n", r, g->stem);
    c->depth = d + 1;
    if (is_expect) {
        lower_pad(c);
        lower_emitf(c, "const char *msgc = ek_str_cstr(%s.as.ref);\n", msg);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", r);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", msg);
        lower_pad(c);
        lower_emitf(c, "ek_panic(\"P-expect\", msgc);\n");
    } else {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", r);
        lower_pad(c);
        lower_emitf(c, "ek_panic(\"P-unwrap\", \"%s on %s\");\n",
                    is_opt ? "Option.unwrap" : "Result.unwrap",
                    is_opt ? "None" : "Err");
    }
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_keep(((ekS_%s *)%s.as.ref)->p0);\n", t,
                g->stem, r);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", r);
    return (lower_val){ t, 1 };
}
