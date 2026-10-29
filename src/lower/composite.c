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
static const ek_symbol *row_nominal(lower_ctx *c, const ek_node *at)
{
    const ek_ck_row *row = lower_row(c, at);
    const ek_type *t = row ? row->type : NULL;

    if (t && t->kind == EK_T_NOMINAL) {
        return t->u.nominal.sym;
    }
    return NULL;
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
        lower_val a = lower_expr(c, scope, item_expr(e->u.kids.data[i]));
        const char *t = lower_as_temp(c, scope, &a);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    return lower_val_unit();
}

/* The item temps of a literal, one slot per kid in source order;
   NULL where the kid carries no value expression. */
static const char **item_temps(lower_ctx *c, lower_scope *scope,
                               const ek_node *e)
{
    const char **argv = NULL;
    size_t cap = 0, i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];
        lower_val a;

        argv = lower_grow(c, argv, &cap, i + 1, sizeof *argv);
        if (!argv) {
            return NULL;
        }
        if (item->kind == EK_N_COMPOSITE_ITEM && !item->u.pair.b) {
            argv[i] = NULL;
            continue;
        }
        a = lower_expr(c, scope, item_expr(item));
        argv[i] = lower_as_temp(c, scope, &a);
    }
    return argv;
}

/* Releases the item temps, most recent first. */
static void release_temps(lower_ctx *c, const char **argv, size_t n)
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

/* A list literal: the elements evaluate left to right, then one fold
   builds the spine from the nil end. */
static lower_val list_literal(lower_ctx *c, lower_scope *scope,
                              const ek_node *e)
{
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *arr, *out;

    if (n == 0) {
        return (lower_val){ "ek_nil_val()", 0 };
    }
    argv = item_temps(c, scope, e);
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
    release_temps(c, argv, n);
    return (lower_val){ out, 1 };
}

/* The scalar key slots the runtime hashes: the tag word and the exact
   payload. */
static int scalar_key_name(const ek_symbol *sym)
{
    static const char *names[] = {
        "Bool", "Int", "UInt", "Float", "Char", "I8", "I16", "I32",
        "I64", "U8", "U16", "U32", "U64", "F32", "F64"
    };
    size_t i;

    if (!sym || !sym->name) {
        return 0;
    }
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        if (strcmp(names[i], sym->name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int scalar_key(const ek_type *k)
{
    if (!k) {
        return 1; /* an undetermined key: the row exists no clearer */
    }
    if (k->kind == EK_T_UNIT || k->kind == EK_T_NEVER
        || k->kind == EK_T_IVAR || k->kind == EK_T_TVAR) {
        return 1;
    }
    if (k->kind == EK_T_NOMINAL) {
        return k->u.nominal.sym
            && (!k->u.nominal.sym->decl
                || strcmp(k->u.nominal.sym->module, "std") == 0)
            && scalar_key_name(k->u.nominal.sym);
    }
    return 0;
}

/* A dictionary literal: the pairs evaluate in source order, then the
   set chain builds the table one fresh dictionary at a step, each
   intermediate dying once its replacement exists. */
static lower_val dict_literal(lower_ctx *c, lower_scope *scope,
                              const ek_node *e)
{
    const ek_ck_row *row = lower_row(c, e);
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *d;

    if (row && row->type && row->type->kind == EK_T_DICT
        && !scalar_key(row->type->u.dict.key)) {
        lower_error(c, e, "the dictionary key is not a scalar this "
                          "lowerer supports");
    }
    if (n == 0) {
        return (lower_val){ "ek_dict_new()", 0 };
    }
    argv = item_temps(c, scope, e);
    if (!argv) {
        return lower_val_unit();
    }
    d = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_dict_new();\n", d);
    for (i = 0; i + 1 < n; i += 2) {
        const char *nd;

        if (!argv[i] || !argv[i + 1]) {
            continue; /* a malformed pair check already diagnosed */
        }
        nd = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_dict_set(%s, %s, %s);\n", nd, d,
                    argv[i], argv[i + 1]);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", d);
        d = nd;
    }
    release_temps(c, argv, n);
    return (lower_val){ d, 1 };
}

/* A record literal: the items map to the family's fields by name, in
   the layout order the declaration fixes; a field no item names was
   diagnosed by check and passes the unit value. */
static lower_val record_literal(lower_ctx *c, lower_scope *scope,
                                const ek_node *e, lower_gtype *g)
{
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *r;

    if (!g || g->shape != GT_REC) {
        return opaque_literal(c, scope, e);
    }
    g->constructed = 1;
    argv = item_temps(c, scope, e);
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
    release_temps(c, argv, n);
    return (lower_val){ r, 1 };
}

/* A tuple literal: the items map to the components by position. */
static lower_val tuple_literal(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, lower_gtype *g)
{
    const char **argv;
    size_t n = e->u.kids.count, i;
    const char *r;

    if (!g) {
        return opaque_literal(c, scope, e);
    }
    g->constructed = 1;
    argv = item_temps(c, scope, e);
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
    release_temps(c, argv, n);
    return (lower_val){ r, 1 };
}

/* The literal's family from the expected type: a nominal lands through
   its declared representation. */
static lower_val nominal_literal(lower_ctx *c, lower_scope *scope,
                                 const ek_node *e, const ek_symbol *tsym)
{
    const ek_node *dn = tsym ? tsym->decl : NULL;
    lower_gtype *g;

    if (dn && dn->kind == EK_N_TYPE_DECL
        && dn->u.type_decl.rhs == EK_TYPE_RHS_RECORD) {
        g = lower_decl_repr(c, tsym);
        return record_literal(c, scope, e, g);
    }
    if (dn && dn->kind == EK_N_TYPE_DECL
        && dn->u.type_decl.rhs == EK_TYPE_RHS_TUPLE) {
        g = lower_decl_repr(c, tsym);
        return tuple_literal(c, scope, e, g);
    }
    return opaque_literal(c, scope, e);
}

/* A bare constructor name: the nullary helper call. A payload
   constructor needs an application and stays unsupported. */
lower_val lower_ctor_value(lower_ctx *c, const ek_node *e, const ek_ref *ref)
{
    const ek_symbol *tsym = row_nominal(c, e);
    lower_gtype *g;

    if (!tsym) {
        lower_error(c, e, "the constructor `%s` is not supported by "
                          "this lowerer",
                    ref->symbol ? ref->symbol->name : "?");
        return lower_val_unit();
    }
    g = lower_ctor_family(c, e, ref, tsym);
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
   the helper retains it into the born object, and the temp dies. */
lower_val lower_ctor_apply(lower_ctx *c, lower_scope *scope,
                           const ek_node *e)
{
    const ek_node *head = e->u.pair.a;
    const ek_ref *ref = head ? lower_ref(c, head) : NULL;
    const ek_symbol *tsym = row_nominal(c, e);
    lower_gtype *g;
    lower_val a;
    const char *at, *r;

    if (!ref || ref->kind != EK_SYM_CONSTRUCTOR) {
        return lower_val_unit(); /* resolve diagnosed the head */
    }
    a = e->u.pair.b ? lower_expr(c, scope, e->u.pair.b) : lower_val_unit();
    at = lower_as_temp(c, scope, &a);
    if (!tsym) {
        lower_error(c, e, "the constructor application is not supported "
                          "by this lowerer");
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", at);
        return lower_val_unit();
    }
    g = lower_ctor_family(c, e, ref, tsym);
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

lower_val lower_composite(lower_ctx *c, lower_scope *scope,
                          const ek_node *e)
{
    const ek_ck_row *row = lower_row(c, e);
    const ek_type *t = row ? row->type : NULL;

    if (!t) {
        return lower_val_unit(); /* check diagnosed the literal */
    }
    switch (t->kind) {
    case EK_T_LIST:
        return list_literal(c, scope, e);
    case EK_T_DICT:
        return dict_literal(c, scope, e);
    case EK_T_RECORD:
        return record_literal(c, scope, e, lower_anon_record(c, t));
    case EK_T_TUPLE:
        return tuple_literal(c, scope, e,
                             lower_anon_tuple(c, t->u.tuple.count));
    case EK_T_NOMINAL:
        return nominal_literal(c, scope, e, t->u.nominal.sym);
    default:
        return opaque_literal(c, scope, e);
    }
}
