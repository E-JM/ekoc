#include <string.h>

#include "lower/internal.h"

/* The with service: record construction from a base. The literal
   supplies the named fields, every field it does not name takes the
   receiver's value through its own slot, and the fresh top level
   builds through the record family's construction helper. A nested
   literal over a record field is partial the same way: the fields it
   does not name read the receiver's field value at that slot. The
   original value is unchanged; there is no mutation. */

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

/* The value expression of the item naming fname; NULL when no item
   names the field or the item carries no value. */
static const ek_node *lit_field(const ek_node *lit, const char *fname)
{
    size_t i;

    for (i = 0; i < lit->u.kids.count; i++) {
        const char *f = item_field(lit->u.kids.data[i]);

        if (f && strcmp(f, fname) == 0) {
            return lit->u.kids.data[i]->u.pair.b;
        }
    }
    return NULL;
}

/* The record value type one field's literal can be partial over: the
   field's own record, or the declared representation of its nominal.
   NULL when the field carries neither. */
static const ek_type *field_record(lower_ctx *c, const ek_type *ft)
{
    const ek_type *r = NULL;

    if (ft && ft->kind == EK_T_RECORD) {
        r = ft;
    } else if (ft && ft->kind == EK_T_NOMINAL) {
        r = lower_repr_type(c, ft);
    }
    return r && r->kind == EK_T_RECORD ? r : NULL;
}

/* True when a literal is partial over rec: it names fewer fields than
   the record holds, so the unnamed ones must read the receiver. */
static int is_partial(const ek_node *val, const ek_type *rec)
{
    size_t named = 0, i;

    if (val->kind != EK_N_COMPOSITE) {
        return 0;
    }
    for (i = 0; i < val->u.kids.count; i++) {
        if (item_field(val->u.kids.data[i])) {
            named++;
        }
    }
    return named < rec->u.record.count;
}

/* The record family and the field spellings and applied types of a
   with receiver; 0 when the shape has no record lowering. */
static int with_shape(lower_ctx *c, const ek_node *at, const ek_type *t,
                      lower_gtype **g, const char ***fnames,
                      const ek_type ***ftypes, size_t *nf)
{
    if (t->kind == EK_T_NOMINAL) {
        *g = lower_decl_repr(c, t->u.nominal.sym);
    } else if (t->kind == EK_T_RECORD) {
        *g = lower_anon_record(c, t);
    } else {
        *g = NULL;
    }
    return *g && (*g)->shape == GT_REC
        && lower_shape_of(c, at, t, fnames, ftypes, nf, g) && *g;
}

static const char *with_build(lower_ctx *c, lower_scope *scope,
                              const ek_node *lit, const ek_type *t,
                              const char *base);

/* One argument temp of a rebuilt record: the literal's value when an
   item names the field -- a nested partial literal builds over the
   receiver's field value at the slot -- and the receiver's slot read
   kept when no item names it. NULL when the build fails; the caller
   keeps the plain lowering. */
static const char *field_arg(lower_ctx *c, lower_scope *scope,
                             const ek_node *lit, const char *fname,
                             const ek_type *ft, const char *stem,
                             const char *base)
{
    const ek_node *val = lit_field(lit, fname);
    const ek_type *rec = val ? field_record(c, ft) : NULL;
    lower_val a;

    if (!val) {
        lower_val v;

        v.text = lower_str(c, "ek_keep(((ekS_%s *)(%s).as.ref)->f_%s)",
                           stem, base, fname);
        v.is_temp = 0;
        return lower_as_temp(c, scope, &v);
    }
    if (rec && is_partial(val, rec)) {
        const char *sub = lower_str(c, "((ekS_%s *)(%s).as.ref)->f_%s",
                                    stem, base, fname);

        if (sub) {
            const char *t = with_build(c, scope, val, ft, sub);

            if (t) {
                return t;
            }
        }
    }
    a = lower_sub(c, scope, val);
    return lower_as_temp(c, scope, &a);
}

/* Releases the argument temps, most recent first. */
static void release_args(lower_ctx *c, const char **argv, size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i]);
    }
}

/* The construction call over the argument temps: the fresh value is
   the caller's, each argument dying now the helper holds its own
   keeps. */
static const char *emit_build(lower_ctx *c, lower_scope *scope,
                              const lower_gtype *g, const char **argv,
                              size_t nf)
{
    const char *r = lower_temp(c, scope);
    size_t i;

    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", r, g->helper);
    for (i = 0; i < nf; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
    }
    lower_emitf(c, ");\n");
    release_args(c, argv, nf);
    return r;
}

/* The rebuilt record value of one partial literal: every field an
   item names comes from the literal, every other field reads the
   base's own slot, and the build runs through the family's
   construction helper. NULL when the shape has no lowering; the
   caller keeps the plain lowering of the literal. */
static const char *with_build(lower_ctx *c, lower_scope *scope,
                              const ek_node *lit, const ek_type *t,
                              const char *base)
{
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    const char **argv = NULL;
    size_t cap = 0, nf, i;

    if (!with_shape(c, lit, t, &g, &fnames, &ftypes, &nf)) {
        return NULL;
    }
    argv = lower_grow(c, NULL, &cap, nf ? nf : 1, sizeof *argv);
    if (!argv) {
        return NULL;
    }
    g->constructed = 1;
    for (i = 0; i < nf; i++) {
        argv[i] = field_arg(c, scope, lit, g->fnames[i], ftypes[i],
                            g->stem, base);
        if (!argv[i]) {
            release_args(c, argv, i);
            return NULL;
        }
    }
    return emit_build(c, scope, g, argv, nf);
}

/* The with service: the receiver's record rebuilt through its
   construction helper, the literal's named fields in place and the
   rest read off the receiver, so the fresh top level shares every
   field the literal does not name. */
lower_val lower_derived_with(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *recv)
{
    const ek_type *t = recv ? lower_row_type(c, recv) : NULL;
    const ek_node *lit = e->u.call.args.count == 1
                             ? e->u.call.args.data[0] : NULL;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    const char **argv = NULL;
    size_t cap = 0, nf, i;
    lower_val rv;
    const char *rt, *r;

    if (!t || !lit || lit->kind != EK_N_COMPOSITE) {
        lower_error(c, e, "the call of `with` is malformed");
        return discard_with(c, scope, e, recv);
    }
    if (!with_shape(c, e, t, &g, &fnames, &ftypes, &nf)) {
        lower_error(c, e, "the receiver of `with` has no record lowering");
        return discard_with(c, scope, e, recv);
    }
    argv = lower_grow(c, NULL, &cap, nf ? nf : 1, sizeof *argv);
    if (!argv) {
        return lower_val_unit();
    }
    rv = lower_sub(c, scope, recv);
    rt = lower_as_temp(c, scope, &rv);
    g->constructed = 1;
    for (i = 0; i < nf; i++) {
        argv[i] = field_arg(c, scope, lit, g->fnames[i], ftypes[i],
                            g->stem, rt);
        if (!argv[i]) {
            release_args(c, argv, i);
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", rt);
            return lower_val_unit();
        }
    }
    r = emit_build(c, scope, g, argv, nf);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", rt);
    return (lower_val){ r, 1 };
}
