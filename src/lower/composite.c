#include <string.h>

#include "lower/internal.h"

/* Constructors and composite literals: every composite is born as one
   owned value through a per-family construction helper, and every
   item expression lowers to a borrowed temp that dies once the
   helper's own retention stands. The expected type of the check row
   decides the family, as it decides the checker's own reading. */

/* The value expression of one composite item: items wrap their
   expression in the node, a stray kid lowers as itself. */
static const ek_node *item_expr(const ek_node *kid)
{
    return kid->kind == EK_N_COMPOSITE_ITEM ? kid->u.pair.b : kid;
}

/* The field name of a colon item, named the way check reads one;
   NULL when the key is an expression or the item is malformed. */
static const char *item_field(const ek_node *kid)
{
    const ek_node *key = kid->kind == EK_N_COMPOSITE_ITEM
                             ? kid->u.pair.a : NULL;

    if (key && key->kind == EK_N_NAME && key->u.path.count == 1) {
        return key->u.path.data[0];
    }
    return NULL;
}

/* The declared type a construction lands at, from the check row. */
const ek_symbol *lower_row_nominal(lower_ctx *c, const ek_node *at)
{
    const ek_type *t = lower_row_type(c, at);

    if (t && t->kind == EK_T_NOMINAL) {
        return t->u.nominal.sym;
    }
    return NULL;
}

/* True when a node's row leaves the type open: absent or an unbound
   inference variable. */
static int row_opaque(lower_ctx *c, const ek_node *n)
{
    const ek_type *t = n ? lower_row_type(c, n) : NULL;

    return !t || t->kind == EK_T_IVAR;
}

/* One item value: an item whose own row the check left opaque lowers
   under the expected type the literal's anchor re-derived for it. The
   override is the subtree's alone and restores on the way out. */
lower_val lower_item_under(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_type *t)
{
    const ek_node *sn = c->exp_node;
    const ek_type *st = c->exp_type;
    lower_val v;

    if (!t || !row_opaque(c, e)) {
        return lower_sub(c, scope, e);
    }
    c->exp_node = e;
    c->exp_type = t;
    v = lower_sub(c, scope, e);
    c->exp_node = sn;
    c->exp_type = st;
    return v;
}

/* A literal (or constructor) the row gives no family for: the item
   expressions still evaluate, and the literal is diagnosed. */
static lower_val opaque_literal(lower_ctx *c, lower_scope *scope,
                                const ek_node *e)
{
    size_t i;

    lower_error(c, e, "the composite literal is not supported by this "
                      "lowerer");
    for (i = 0; i < e->u.kids.count; i++) {
        lower_val a = lower_sub(c, scope, item_expr(e->u.kids.data[i]));
        const char *t = lower_as_temp(c, scope, &a);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    return lower_val_unit();
}

/* The expected type of one positional item: the element or component
   the literal's own effective type names. */
static const ek_type *item_expected(const ek_type *t, size_t i)
{
    if (!t) {
        return NULL;
    }
    if (t->kind == EK_T_LIST) {
        return t->u.list.elem;
    }
    if (t->kind == EK_T_TUPLE && i < t->u.tuple.count) {
        return t->u.tuple.items[i];
    }
    return NULL;
}

/* The item temps of a literal, one slot per kid in source order;
   NULL where the kid carries no value expression. t is the literal's
   own effective type for the positional families, rec the record type
   the named fields read; an item whose own row is opaque lowers under
   the type the literal's anchor names for it. */
const char **lower_item_temps(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_type *t,
                               const ek_type *rec)
{
    const char **argv = NULL;
    size_t cap = 0, i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];
        const ek_type *ft;
        lower_val a;

        argv = lower_grow(c, argv, &cap, i + 1, sizeof *argv);
        if (!argv) {
            return NULL;
        }
        if (item->kind == EK_N_COMPOSITE_ITEM && !item->u.pair.b) {
            argv[i] = NULL;
            continue;
        }
        ft = rec ? lower_record_field(rec, item_field(item)) : NULL;
        if (!ft) {
            ft = item_expected(t, i);
        }
        a = lower_item_under(c, scope, item_expr(item), ft);
        argv[i] = lower_as_temp(c, scope, &a);
    }
    return argv;
}

/* Releases the item temps, most recent first. */
void lower_release_temps(lower_ctx *c, const char **argv, size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        if (argv[i]) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", argv[i]);
        }
    }
}

/* A record literal: the items map to the family's fields by name, in
   the layout order the declaration fixes; a field no item names was
   diagnosed by check and passes the unit value. rec is the record
   value type the item rows read their field types from, NULL when the
   literal's anchor named none. */
static lower_val record_literal(lower_ctx *c, lower_scope *scope,
                                const ek_node *e, lower_gtype *g,
                                const ek_type *rec)
{
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *r;

    if (!g || g->shape != GT_REC) {
        return opaque_literal(c, scope, e);
    }
    g->constructed = 1;
    argv = lower_item_temps(c, scope, e, NULL, rec);
    if (!argv) {
        return lower_val_unit();
    }
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", r, g->helper);
    for (i = 0; i < g->nf; i++) {
        const char *arg = NULL;
        size_t k;

        for (k = 0; k < n; k++) {
            const char *f = item_field(e->u.kids.data[k]);

            if (f && strcmp(f, g->fnames[i]) == 0) {
                arg = argv[k];
                break;
            }
        }
        lower_emitf(c, "%s%s", i ? ", " : "",
                    arg ? arg : "ek_val_unit()");
    }
    lower_emitf(c, ");\n");
    lower_release_temps(c, argv, n);
    return (lower_val){ r, 1 };
}

/* A tuple literal: the items map to the components by position. */
static lower_val tuple_literal(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, lower_gtype *g,
                               const ek_type *tup)
{
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *r;

    if (!g) {
        return opaque_literal(c, scope, e);
    }
    g->constructed = 1;
    argv = lower_item_temps(c, scope, e, tup, NULL);
    if (!argv) {
        return lower_val_unit();
    }
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", r, g->helper);
    for (i = 0; i < g->nf; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "",
                    i < n && argv[i] ? argv[i] : "ek_val_unit()");
    }
    lower_emitf(c, ");\n");
    lower_release_temps(c, argv, n);
    return (lower_val){ r, 1 };
}

/* The literal's family from the expected type: a nominal lands through
   its declared representation, whose field and component types the
   opaque item rows read. */
static lower_val nominal_literal(lower_ctx *c, lower_scope *scope,
                                 const ek_node *e, const ek_type *t)
{
    const ek_symbol *tsym = t && t->kind == EK_T_NOMINAL
                                ? t->u.nominal.sym : NULL;
    const ek_node *dn = tsym ? tsym->decl : NULL;
    lower_gtype *g;

    if (dn && dn->kind == EK_N_CTYPE_DECL) {
        g = lower_decl_repr(c, tsym);
        return record_literal(c, scope, e, g, NULL);
    }
    if (dn && dn->kind == EK_N_TYPE_DECL
        && dn->u.type_decl.rhs == EK_TYPE_RHS_RECORD) {
        g = lower_decl_repr(c, tsym);
        return record_literal(c, scope, e, g, lower_repr_type(c, t));
    }
    if (dn && dn->kind == EK_N_TYPE_DECL
        && dn->u.type_decl.rhs == EK_TYPE_RHS_TUPLE) {
        g = lower_decl_repr(c, tsym);
        return tuple_literal(c, scope, e, g, lower_repr_type(c, t));
    }
    return opaque_literal(c, scope, e);
}

lower_val lower_composite(lower_ctx *c, lower_scope *scope,
                          const ek_node *e)
{
    const ek_type *t = lower_row_type(c, e);

    if (!t) {
        return lower_val_unit(); /* check diagnosed the literal */
    }
    switch (t->kind) {
    case EK_T_LIST:
        return lower_list_literal(c, scope, e);
    case EK_T_DICT:
        return lower_dict_literal(c, scope, e);
    case EK_T_RECORD:
        return record_literal(c, scope, e, lower_anon_record(c, t), t);
    case EK_T_TUPLE:
        return tuple_literal(c, scope, e,
                             lower_anon_tuple(c, t->u.tuple.count), t);
    case EK_T_NOMINAL:
        return nominal_literal(c, scope, e, t);
    default:
        return opaque_literal(c, scope, e);
    }
}

