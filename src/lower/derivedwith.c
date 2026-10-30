#include <string.h>

#include "lower/internal.h"

/* The with service: record construction from a base. The literal
   supplies the named fields, every field it does not name takes the
   receiver's value through its own slot, and the fresh top level
   builds through the record family's construction helper. The original
   value is unchanged; there is no mutation. */

/* The field name of one literal item; NULL when the item is malformed
   or names nothing. */
static const char *item_field(const ek_node *kid)
{
    const ek_node *key = kid->kind == EK_N_COMPOSITE_ITEM ? kid->u.pair.a
                                                          : NULL;

    if (key && key->kind == EK_N_NAME && key->u.path.count == 1) {
        return key->u.path.data[0];
    }
    return NULL;
}

/* The malformed with: the receiver and the literal's items evaluate
   and die. */
static lower_val discard_with(lower_ctx *c, lower_scope *scope,
                              const ek_node *e, const ek_node *recv)
{
    const ek_node *arg = e->u.call.args.count == 1
                             ? e->u.call.args.data[0] : NULL;
    lower_val a;
    const char *t;
    size_t i;

    a = lower_sub(c, scope, recv);
    t = lower_as_temp(c, scope, &a);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    if (arg && arg->kind == EK_N_COMPOSITE) {
        for (i = 0; i < arg->u.kids.count; i++) {
            const ek_node *kid = arg->u.kids.data[i];
            const ek_node *val = kid->kind == EK_N_COMPOSITE_ITEM
                                     ? kid->u.pair.b : kid;

            if (!val) {
                continue;
            }
            a = lower_sub(c, scope, val);
            t = lower_as_temp(c, scope, &a);
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", t);
        }
    }
    return lower_val_unit();
}

/* The with service: the receiver's record rebuilt through its
   construction helper, the literal's named fields in place and the
   rest read off the receiver, so the fresh top level shares every
   field the literal does not name. */
lower_val lower_derived_with(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *recv)
{
    const ek_ck_row *rrow = recv ? lower_row(c, recv) : NULL;
    const ek_type *t = rrow ? rrow->type : NULL;
    const ek_node *lit = e->u.call.args.count == 1
                             ? e->u.call.args.data[0] : NULL;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    size_t nf, i;
    lower_val rv;
    const char *rt;
    const char **argv = NULL;
    size_t cap = 0;
    const char *r;

    if (!t || !lit || lit->kind != EK_N_COMPOSITE) {
        lower_error(c, e, "the call of `with` is malformed");
        return discard_with(c, scope, e, recv);
    }
    if (t->kind == EK_T_NOMINAL) {
        g = lower_decl_repr(c, t->u.nominal.sym);
    } else if (t->kind == EK_T_RECORD) {
        g = lower_anon_record(c, t);
    }
    if (!g || g->shape != GT_REC) {
        lower_error(c, e, "the receiver of `with` has no record lowering");
        return discard_with(c, scope, e, recv);
    }
    if (!lower_shape_of(c, e, t, &fnames, &ftypes, &nf, &g) || !g) {
        lower_error(c, e, "the receiver of `with` has no record lowering");
        return discard_with(c, scope, e, recv);
    }
    argv = lower_grow(c, NULL, &cap, nf ? nf : 1, sizeof *argv);
    if (!argv) {
        return lower_val_unit();
    }
    for (i = 0; i < nf; i++) {
        const char *fname = g->fnames[i];
        const ek_node *val = NULL;
        size_t k;

        for (k = 0; k < lit->u.kids.count; k++) {
            const char *f = item_field(lit->u.kids.data[k]);

            if (f && strcmp(f, fname) == 0) {
                val = lit->u.kids.data[k];
                break;
            }
        }
        if (val && (val->kind != EK_N_COMPOSITE_ITEM || val->u.pair.b)) {
            lower_val a = lower_sub(c, scope,
                                    val->kind == EK_N_COMPOSITE_ITEM
                                        ? val->u.pair.b : val);

            argv[i] = lower_as_temp(c, scope, &a);
        } else {
            argv[i] = NULL; /* an unnamed field reads the receiver */
        }
    }
    rv = lower_sub(c, scope, recv);
    rt = lower_as_temp(c, scope, &rv);
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", r, g->helper);
    for (i = 0; i < nf; i++) {
        if (argv[i]) {
            lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
        } else {
            lower_emitf(c, "%sek_keep(((ekS_%s *)%s.as.ref)->f_%s)",
                        i ? ", " : "", g->stem, rt, g->fnames[i]);
        }
    }
    lower_emitf(c, ");\n");
    for (i = nf; i > 0; i--) {
        if (argv[i - 1]) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", argv[i - 1]);
        }
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", rt);
    return (lower_val){ r, 1 };
}
