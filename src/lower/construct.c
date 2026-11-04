#include <string.h>

#include "lower/internal.h"

/* The composites born at a family border: the list and dictionary
   literals, the nominal-to-anonymous shape adaptation, and the
   constructor applications. Each item evaluates in source order into
   a temp, the family builds from owned reads, and the temps die. */
/* A list literal: the elements evaluate left to right, then one fold
   builds the spine from the nil end. */
lower_val lower_list_literal(lower_ctx *c, lower_scope *scope,
                              const ek_node *e)
{
    const ek_type *t = lower_row_type(c, e);
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *arr, *out;

    if (n == 0) {
        return (lower_val){ "ek_nil_val()", 0 };
    }
    argv = lower_item_temps(c, scope, e, t, NULL);
    if (!argv) {
        return lower_val_unit();
    }
    lower_use_list(c);
    arr = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s[] = {", arr);
    for (i = 0; i < n; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "",
                    argv[i] ? argv[i] : "ek_val_unit()");
    }
    lower_emitf(c, "};\n");
    out = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ekn_list(%s, %zu);\n", out, arr, n);
    lower_release_temps(c, argv, n);
    return (lower_val){ out, 1 };
}

/* The key and value temps of the entries, in source order; the arrays
   hold one slot per kid, NULL where a side is absent. t is the
   literal's own dictionary type, naming the key and value types the
   opaque rows read. A key that lowers to the unit placeholder under a
   determined non-unit key type diagnoses: the placeholder keys every
   entry alike, and the unit value itself is never a non-unit key. */
static void dict_item_temps(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_type *t,
                            const char **kt, const char **vt)
{
    const ek_type *dtype = t && t->kind == EK_T_DICT ? t : NULL;
    size_t i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];
        const ek_node *key = item->kind == EK_N_COMPOSITE_ITEM
                                 ? item->u.pair.a : NULL;
        const ek_node *val = item->kind == EK_N_COMPOSITE_ITEM
                                 ? item->u.pair.b : item;
        lower_val a;

        kt[i] = NULL;
        vt[i] = NULL;
        if (key) {
            const ek_type *kt_row;

            a = lower_item_under(c, scope, key, dtype ? dtype->u.dict.key : NULL);
            kt_row = dtype ? lower_row_type(c, key) : NULL;
            if (!a.is_temp && strcmp(a.text, "ek_val_unit()") == 0
                && kt_row && kt_row->kind != EK_T_UNIT) {
                lower_error(c, key, "the dictionary key has no lowering");
            }
            kt[i] = lower_as_temp(c, scope, &a);
        }
        if (val) {
            a = lower_item_under(c, scope, val, dtype ? dtype->u.dict.val : NULL);
            vt[i] = lower_as_temp(c, scope, &a);
        }
    }
}

/* Releases the entry temps, most recent first. */
static void release_dict_temps(lower_ctx *c, const char **kt,
                               const char **vt, size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        if (vt[i]) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", vt[i]);
        }
        if (kt[i]) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", kt[i]);
        }
    }
}

/* A dictionary literal: every keyed entry's key and value evaluate in
   source order, then the set chain builds the table one fresh
   dictionary at a step, each intermediate dying once its replacement
   exists. */
lower_val lower_dict_literal(lower_ctx *c, lower_scope *scope,
                              const ek_node *e)
{
    const ek_type *t = lower_row_type(c, e);
    const char **kt, **vt;
    size_t n = e->u.kids.count, cap = 0, i;
    const char *d;

    if (t && t->kind == EK_T_DICT && !lower_key_hooks(c, t->u.dict.key)) {
        lower_error(c, e, "the dictionary key is not a key domain this "
                          "lowerer supports");
    }
    if (n == 0) {
        return (lower_val){ "ek_dict_new()", 0 };
    }
    kt = lower_grow(c, NULL, &cap, n * 2, sizeof *kt);
    if (!kt) {
        return lower_val_unit();
    }
    vt = kt + n;
    dict_item_temps(c, scope, e, t, kt, vt);
    d = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_dict_new();\n", d);
    for (i = 0; i < n; i++) {
        const char *nd;

        if (!kt[i] || !vt[i]) {
            continue; /* a malformed entry check already diagnosed */
        }
        nd = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_dict_set(%s, %s, %s);\n", nd, d,
                    kt[i], vt[i]);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", d);
        d = nd;
    }
    release_dict_temps(c, kt, vt, n);
    return (lower_val){ d, 1 };
}


/* The one-directional shape adaptation: a nominal record
   flowing into an anonymous record expectation rebuilds field by field
   -- the shared anonymous family never meets a nominal object, because
   the two layouts order one field set differently. The destination
   family births from owned reads of the source family's slots. The
   value comes back unchanged when the pair is not that shape. */
lower_val lower_shape_adapt(lower_ctx *c, lower_scope *scope, lower_val v,
                            const ek_node *at, const ek_type *from,
                            const ek_type *to)
{
    const ek_type *repr;
    lower_gtype *g;
    const char *t;
    const char **argv;
    size_t i, k;

    if (!from || from->kind != EK_T_NOMINAL || !to
        || to->kind != EK_T_RECORD || !to->u.record.count) {
        return v;
    }
    repr = lower_repr_type(c, from);
    if (!repr || repr->kind != EK_T_RECORD
        || repr->u.record.count != to->u.record.count) {
        return v;
    }
    g = lower_anon_record(c, to);
    if (!g || g->shape != GT_REC || !g->fnames) {
        return v;
    }
    for (i = 0; i < g->nf; i++) {
        int found = 0;

        for (k = 0; k < repr->u.record.count; k++) {
            if (repr->u.record.fields[k].name
                && strcmp(repr->u.record.fields[k].name, g->fnames[i])
                       == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return v; /* check refused a mismatched pair already */
        }
    }
    g->constructed = 1;
    t = lower_as_temp(c, scope, &v);
    {
        size_t cap = 0;

        argv = lower_grow(c, NULL, &cap, g->nf, sizeof *argv);
    }
    if (!argv) {
        c->oom = 1;
        return lower_val_unit();
    }
    for (i = 0; i < g->nf; i++) {
        const char *slot = lower_field_slot(c, at, from, t, g->fnames[i]);

        argv[i] = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", argv[i],
                    slot ? slot : "ek_val_unit()");
    }
    v = (lower_val){ lower_temp(c, scope), 1 };
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", v.text, g->helper);
    for (i = 0; i < g->nf; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
    }
    lower_emitf(c, ");\n");
    for (i = g->nf; i > 0; i--) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i - 1]);
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return v;
}

/* The constructor a type-qualified apply head names: the receiver
   names the type, the link the variant of that type's own
   declaration. NULL outside the form. */
static const ek_symbol *qualified_ctor(lower_ctx *c, const ek_node *head)
{
    const ek_ref *tr;
    const ek_symbol *tsym;
    size_t k, i;

    if (!head || head->kind != EK_N_FIELD) {
        return NULL;
    }
    tr = head->u.field.recv ? lower_ref(c, head->u.field.recv) : NULL;
    if (!tr || tr->kind != EK_SYM_TYPE || !tr->symbol) {
        return NULL;
    }
    tsym = tr->symbol;
    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, tsym->module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, tsym->name) == 0
                && strcmp(m->name, head->u.field.name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* A bare constructor name: the nullary helper call. A payload
   constructor needs an application and stays unsupported. */
lower_val lower_ctor_value(lower_ctx *c, const ek_node *e, const ek_ref *ref)
{
    const ek_symbol *tsym = lower_row_nominal(c, e);
    lower_gtype *g;

    if (!tsym) {
        lower_error(c, e, "the constructor `%s` is not supported by "
                          "this lowerer",
                    ref->symbol ? ref->symbol->name : "?");
        return lower_val_unit();
    }
    g = lower_ctor_built(c, e, ref, tsym);
    if (!g) {
        return lower_val_unit(); /* the family diagnosed the mismatch */
    }
    if (g->nf) {
        lower_error(c, e, "the constructor `%s` is not supported by "
                          "this lowerer",
                    ref->symbol ? ref->symbol->name : "?");
        return lower_val_unit();
    }
    return (lower_val){ lower_str(c, "%s()", g->helper), 0 };
}

/* A constructor application: the payload argument lowers to a temp,
   the helper retains it into the born object, and the temp dies. A
   type-qualified head the resolver bridged names its variant through
   the type, and the family builds from the constructor's own
   declaration. */
lower_val lower_ctor_apply(lower_ctx *c, lower_scope *scope,
                           const ek_node *e)
{
    const ek_node *head = e->u.pair.a;
    const ek_ref *ref = head ? lower_ref(c, head) : NULL;
    const ek_symbol *tsym = lower_row_nominal(c, e);
    int qualified = !ref || ref->kind != EK_SYM_CONSTRUCTOR;
    const ek_symbol *cs = qualified ? qualified_ctor(c, head) : NULL;
    lower_gtype *g;
    lower_val a;
    const char *at, *r;

    if (qualified && !cs) {
        return lower_val_unit(); /* resolve diagnosed the head */
    }
    a = e->u.pair.b ? lower_sub(c, scope, e->u.pair.b) : lower_val_unit();
    at = lower_as_temp(c, scope, &a);
    if (qualified) {
        g = lower_ctor_of_symbol(c, e, cs, 1);
    } else if (!tsym) {
        lower_error(c, e, "the constructor application is not supported "
                          "by this lowerer");
        g = NULL;
    } else {
        g = lower_ctor_built(c, e, ref, tsym);
    }
    if (!g) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", at);
        return lower_val_unit();
    }
    r = lower_temp(c, scope);
    lower_pad(c);
    if (g->nf) {
        lower_emitf(c, "ek_val %s = %s(%s);\n", r, g->helper, at);
    } else {
        /* check diagnosed the arity; the argument still evaluates */
        lower_emitf(c, "ek_val %s = %s();\n", r, g->helper);
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", at);
    return (lower_val){ r, 1 };
}
