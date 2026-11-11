#include <string.h>

#include "lower/internal.h"

/* The irrefutable destructuring of an initial binding: the right-hand
   side lowers to a temp, every bound name reads its slot as an owned
   keep, and the temp dies once the reads stand. Check guarantees the
   walk never tests — the pattern names subvalues that are always
   there. */

/* The tuple representation the value's row names: the anonymous
   family of a tuple row, the declared family of a tuple-shaped
   nominal; NULL when the row names neither. */
static lower_gtype *tuple_repr_of(lower_ctx *c, const ek_type *t)
{
    if (t && t->kind == EK_T_TUPLE) {
        return lower_anon_tuple(c, t->u.tuple.count);
    }
    if (t && t->kind == EK_T_NOMINAL && t->u.nominal.sym) {
        lower_gtype *g = lower_decl_repr(c, t->u.nominal.sym);

        return g && g->shape == GT_TUP ? g : NULL;
    }
    return NULL;
}

/* The family a brace pattern reads: the record or tuple the type
   names, as the arm walk decides it. A list family never reaches a
   binding. */
static lower_gtype *brace_repr_of(lower_ctx *c, const ek_type *t)
{
    if (t && t->kind == EK_T_TUPLE) {
        return lower_anon_tuple(c, t->u.tuple.count);
    }
    if (t && t->kind == EK_T_RECORD) {
        return lower_anon_record(c, t);
    }
    if (t && t->kind == EK_T_NOMINAL && t->u.nominal.sym) {
        return lower_decl_repr(c, t->u.nominal.sym);
    }
    return NULL;
}

/* The type a pattern node reads at: its own check row when the walk
   closed it, else the type handed down. */
static const ek_type *repr_type(lower_ctx *c, const ek_node *pat,
                                const ek_type *t)
{
    const ek_type *own = lower_row_type(c, pat);

    if (own && !lower_type_has_var(own)) {
        return own;
    }
    return t;
}

/* One bound name: an owned keep of its slot. */
static int destructure_pat(lower_ctx *c, lower_scope *scope,
                           const ek_node *pat, const char *slot,
                           const ek_type *t, int rebind);

/* A destructured name: an initial binding declares; a rebind moves
   into the name the scope already carries and declares a fresh one
   otherwise, exactly the single-name rebind's own reading. */
static int bind_name(lower_ctx *c, lower_scope *scope, const char *name,
                     const char *slot, int rebind)
{
    const char *cname = lower_c_name(c, scope, name);
    const lower_local *loc;

    if (!rebind) {
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", cname, slot);
        lower_scope_add(c, scope, name, cname, 0, NULL);
        return 1;
    }
    loc = scope_find_local(scope, name);
    if (!loc) {
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", cname, slot);
        lower_scope_add(c, scope, name, cname, 0, NULL);
        return 1;
    }
    if (loc->is_param) {
        return 0; /* a borrowed binding cannot rebind; the single-name
                     form names it, here the walk declines */
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", loc->cname);
    lower_pad(c);
    lower_emitf(c, "%s = ek_keep(%s);\n", loc->cname, slot);
    return 1;
}

/* One record item: the field's slot, read by name. */
static int destructure_field(lower_ctx *c, lower_scope *scope,
                             const ek_node *item, lower_gtype *g,
                             const char *slot, int rebind)
{
    const ek_node *sub = NULL;
    const char *field;
    const char *sub_slot;
    size_t k;

    if (item->kind == EK_N_P_FIELD) {
        field = item->u.p_field.name;
        sub = item->u.p_field.pattern;
    } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
        field = item->u.path.data[0];
        sub = item;
    } else {
        return 0;
    }
    for (k = 0; k < g->nf; k++) {
        if (strcmp(g->fnames[k], field) == 0) {
            break;
        }
    }
    if (k >= g->nf) {
        return 0; /* check diagnosed the missing field */
    }
    sub_slot = lower_str(c, "((ekS_%s *)%s.as.ref)->f_%s", g->stem, slot,
                         field);
    if (sub == item) {
        return bind_name(c, scope, field, sub_slot, rebind);
    }
    return destructure_pat(c, scope, sub, sub_slot, NULL, rebind);
}

/* One component slot of a generated family, by position. */
static const char *field_slot(lower_ctx *c, lower_gtype *g,
                              const char *slot, size_t i)
{
    return lower_str(c, "((ekS_%s *)%s.as.ref)->f_%zu", g->stem, slot, i);
}

/* A tuple group: every component reads out of the family the type
   names, at its own component type. */
static int destructure_paren(lower_ctx *c, lower_scope *scope,
                             const ek_node *pat, const char *slot,
                             const ek_type *t, int rebind)
{
    lower_gtype *g = tuple_repr_of(c, t);
    size_t i;

    if (!g || pat->u.kids.count != g->nf) {
        return 0;
    }
    for (i = 0; i < g->nf; i++) {
        const ek_type *ct = t && t->kind == EK_T_TUPLE
                                && i < t->u.tuple.count
                                ? t->u.tuple.items[i] : NULL;

        if (!destructure_pat(c, scope, pat->u.kids.data[i],
                             field_slot(c, g, slot, i), ct, rebind)) {
            return 0;
        }
    }
    return 1;
}

/* A brace pattern over a record or tuple shape: the record items read
   by field name, the tuple items by position. */
static int destructure_brace(lower_ctx *c, lower_scope *scope,
                             const ek_node *pat, const char *slot,
                             const ek_type *t, int rebind)
{
    lower_gtype *g;
    size_t i;

    if (pat->u.p_brace.rest != EK_REST_NONE) {
        return 0; /* a list pattern never reaches a binding */
    }
    g = brace_repr_of(c, t);
    if (!g) {
        return 0;
    }
    if (g->shape == GT_REC) {
        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            if (!destructure_field(c, scope,
                                   pat->u.p_brace.items.data[i], g, slot,
                                   rebind)) {
                return 0;
            }
        }
        return 1;
    }
    for (i = 0; i < pat->u.p_brace.items.count && i < g->nf; i++) {
        if (!destructure_pat(c, scope, pat->u.p_brace.items.data[i],
                             field_slot(c, g, slot, i), NULL, rebind)) {
            return 0;
        }
    }
    return 1;
}

/* The payload of the sole constructor the family names. */
static int destructure_cons(lower_ctx *c, lower_scope *scope,
                            const ek_node *pat, const char *slot,
                            const ek_type *t, int rebind)
{
    const ek_ref *ref = lower_ref(c, pat);
    const ek_symbol *tsym = t && t->kind == EK_T_NOMINAL
                                ? t->u.nominal.sym : NULL;
    lower_gtype *g = tsym ? lower_ctor_family(c, pat, ref, tsym) : NULL;

    if (!g) {
        return 0;
    }
    if (!g->nf || !pat->u.cons.payload) {
        return !g->nf && !pat->u.cons.payload;
    }
    return destructure_pat(c, scope, pat->u.cons.payload,
                           lower_str(c, "((ekS_%s *)%s.as.ref)->p0",
                                     g->stem, slot), NULL, rebind);
}

/* The irrefutable walk of one pattern node over its slot. */
static int destructure_pat(lower_ctx *c, lower_scope *scope,
                           const ek_node *pat, const char *slot,
                           const ek_type *t, int rebind)
{
    const ek_type *rt = repr_type(c, pat, t);

    switch (pat->kind) {
    case EK_N_P_WILDCARD:
        return 1;
    case EK_N_P_VAR:
        if (pat->u.path.count == 1
            && strcmp(pat->u.path.data[0], "_") != 0) {
            return bind_name(c, scope, pat->u.path.data[0], slot, rebind);
        }
        return 1;
    case EK_N_P_FIELD:
        return destructure_pat(c, scope, pat->u.p_field.pattern, slot, rt,
                               rebind);
    case EK_N_P_PAREN:
        if (pat->u.kids.count == 1) {
            return destructure_pat(c, scope, pat->u.kids.data[0], slot, rt,
                                   rebind);
        }
        return destructure_paren(c, scope, pat, slot, rt, rebind);
    case EK_N_P_BRACE:
        return destructure_brace(c, scope, pat, slot, rt, rebind);
    case EK_N_P_CONS:
        return destructure_cons(c, scope, pat, slot, rt, rebind);
    default:
        return 0;
    }
}

/* A destructuring binding: the right-hand side lowers to a temp —
   adapting through the annotation's shape when one is written — the
   pattern reads its subvalues out, and the temp dies. */
int lower_binding_destructure(lower_ctx *c, lower_scope *scope,
                              const ek_node *m, const ek_node *pat)
{
    const ek_type *vt = lower_row_type(c, m->u.bind.value);
    lower_val v;
    const char *t;
    int ok;

    v = lower_expr(c, scope, m->u.bind.value);
    if (m->u.bind.type) {
        /* the annotation's expectation adapts a nominal record value
           into the anonymous shape it declares */
        const ek_ck_row *arow = lower_row(c, m->u.bind.type);

        v = lower_shape_adapt(c, scope, v, m, vt,
                              arow ? arow->type : NULL);
        vt = arow ? arow->type : vt;
    }
    t = lower_as_temp(c, scope, &v);
    ok = destructure_pat(c, scope, pat, t, vt, 0);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return ok;
}

/* A destructuring rebind: the same walk with the rebind's name
   reading, and no annotation adaptation -- the rebind carries no
   annotation of its own. */
int lower_rebind_destructure(lower_ctx *c, lower_scope *scope,
                             const ek_node *m, const ek_node *pat)
{
    const ek_type *vt = lower_row_type(c, m->u.bind.value);
    lower_val v;
    const char *t;
    int ok;

    v = lower_expr(c, scope, m->u.bind.value);
    t = lower_as_temp(c, scope, &v);
    ok = destructure_pat(c, scope, pat, t, vt, 1);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return ok;
}
