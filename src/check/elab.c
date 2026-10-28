#include <string.h>

#include "check/internal.h"

/* Type positions: the elaboration of an annotation into a type, and
   the anonymous representation of a shaped nominal type. The binder
   scope of a declaration lives in decl.c. */

/* The machine words. The placeholder surface does not list them, so
   check owns their identity for literal placement, the ctype field
   predicate, and the marshalling whitelist. */
static int is_word_name(const char *n)
{
    static const char *words[] = {"I8",  "I16", "I32", "I64",
                                  "U8",  "U16", "U32", "U64",
                                  "F32", "F64"};
    size_t i;

    for (i = 0; i < sizeof words / sizeof *words; i++) {
        if (strcmp(words[i], n) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The type variable of one annotation name, found innermost first. */
static const ek_type *tv_find(const ck_tvars *tv, const char *name)
{
    size_t i;

    for (; tv; tv = tv->parent) {
        for (i = 0; i < tv->count; i++) {
            if (strcmp(tv->names[i], name) == 0) {
                return tv->tvs[i];
            }
        }
    }
    return NULL;
}

/* The arity of an application of a declared type. */
static size_t type_arity(ck_ctx *c, const ek_symbol *sym)
{
    const ck_decl *d = sym ? ck_decl_of(c, sym) : NULL;

    return d ? d->nbinders : 0;
}

static const ek_type *tvar_name(ck_ctx *c, const ek_node *t, const char *n0,
                                ck_tvars *tv)
{
    const ek_type *v = tv_find(tv, n0);
    ek_type *fresh;

    if (v) {
        return v;
    }
    fresh = ck_fresh(c, EK_T_TVAR);
    if (!fresh) {
        return NULL;
    }
    fresh->u.tvar.name = n0;
    ck_tv_add(c, tv, n0, fresh, t);
    return fresh;
}

/* A Pascal name at one path segment: a declared type, a machine word,
   or the placeholder bridge. */
static const ek_type *type_name(ck_ctx *c, const ek_node *t,
                                const ek_ref *ref, const char *n0)
{
    if (ref && ref->symbol) {
        size_t arity = type_arity(c, ref->symbol);

        if (arity) {
            ck_error(c, t, "E-type-arity",
                     "the generic type `%s` needs %zu type arguments "
                     "(Clause 7.6.2)", ref->symbol->name, arity);
        }
        return ck_nominal(c, ref->symbol, NULL, 0);
    }
    if (is_word_name(n0)) {
        return ck_std_type(c, n0);
    }
    return ck_ivar(c); /* the placeholder std bridge */
}

/* A type-position name of one or two path segments. */
static const ek_type *elab_name(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    const char *n0 = t->u.path.count ? t->u.path.data[0] : "";
    const ek_ref *ref = NULL;
    const ek_type *out = NULL;

    if (t->u.path.count == 1 && strcmp(n0, "_") == 0) {
        out = ck_unit(c); /* `_` is the unit type */
    } else if (t->u.path.count == 1 && strcmp(n0, "Never") == 0) {
        out = ck_never(c); /* `Never` is the bottom type */
    } else if (t->u.path.count == 1) {
        if (n0[0] >= 'a' && n0[0] <= 'z') {
            out = tvar_name(c, t, n0, tv);
        } else {
            (void)ck_ref_at(c, t, EK_SYM_TYPE, &ref);
            out = type_name(c, t, ref, n0);
        }
    } else if (t->u.path.count == 2) {
        const char *n1 = t->u.path.data[1];

        if (n1[0] >= 'a' && n1[0] <= 'z') {
            /* The identity space is PascalCase: a lowercase name in a
               qualified type. */
            ck_error(c, t, "E-name-case",
                     "the type name `%s` is not PascalCase "
                     "(Clause 5.5.2)", n1);
            out = ck_ivar(c);
        } else if (ck_ref_at(c, t, EK_SYM_TYPE, &ref)) {
            if (ref->symbol && type_arity(c, ref->symbol)) {
                ck_error(c, t, "E-type-arity",
                         "the generic type `%s.%s` needs %zu type "
                         "arguments (Clause 7.6.2)", n0, n1,
                         type_arity(c, ref->symbol));
            }
            out = ref->symbol ? ck_nominal(c, ref->symbol, NULL, 0)
                              : ck_ivar(c);
        } else {
            out = ck_ivar(c);
        }
    }
    if (!out && c->oom) {
        return NULL;
    }
    ck_row(c, t, out, EK_RD_NONE);
    return out;
}

/* An application of a declared type. */
static const ek_type *elab_apply(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    const ek_node *head = t->u.t_apply.head;
    const ek_ref *href = NULL;
    const ek_symbol *sym = NULL;
    const ek_type **args = NULL;
    size_t i, n = t->u.t_apply.args.count, arity;

    if (head && head->kind == EK_N_T_NAME
        && ck_ref_at(c, head, EK_SYM_TYPE, &href)) {
        sym = href->symbol;
    }
    if (!sym && head && head->kind == EK_N_T_NAME && head->u.path.count) {
        const char *last = head->u.path.data[head->u.path.count - 1];

        if (last[0] >= 'a' && last[0] <= 'z') {
            ck_error(c, head, "E-type-apply",
                     "the type variable `%s` heads an application; a "
                     "variable denotes a type, not a type constructor "
                     "(Clause 7.6.1)", last);
        }
    }
    arity = type_arity(c, sym);
    if (sym && arity && arity != n) {
        ck_error(c, t, "E-type-arity",
                 "the application of `%s` supplies %zu arguments, it "
                 "takes %zu (Clause 7.6.2)", sym->name, n, arity);
    }
    if (n) {
        args = ek_arena_alloc(c->arena, n * sizeof *args);
        if (!args) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            args[i] = ck_elaborate(c, t->u.t_apply.args.data[i], tv);
        }
    }
    return sym ? ck_nominal(c, sym, args, n) : ck_ivar(c);
}

/* The parameter types and the result of a function type; an elided
   result is `_`. */
static const ek_type *elab_fn(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    const ek_type **ps = NULL;
    const ek_type *ret;
    size_t i, n = t->u.t_fn.params.count;

    if (n) {
        ps = ek_arena_alloc(c->arena, n * sizeof *ps);
        if (!ps) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            ps[i] = ck_elaborate(c,
                                 t->u.t_fn.params.data[i]->u.t_field.type,
                                 tv);
        }
    }
    ret = t->u.t_fn.result ? ck_elaborate(c, t->u.t_fn.result, tv)
                           : ck_unit(c);
    return ck_fn(c, ps, n, ret, t->u.t_fn.arrow);
}

/* The fields of a bracket record group. */
static ek_tfield *group_fields(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    ek_tfield *fs = NULL;
    size_t i, n = t->u.kids.count;

    if (n) {
        fs = ek_arena_alloc(c->arena, n * sizeof *fs);
        if (!fs) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            const ek_node *f = t->u.kids.data[i];

            fs[i].name = f->u.t_field.name;
            fs[i].type = ck_elaborate(c, f->u.t_field.type, tv);
        }
    }
    return fs;
}

/* The elements of a bracket group: a record's `f: T` fields and a
   tuple's bare types share one walk. */
static const ek_type *elab_group(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    if (t->kind == EK_N_T_RECORD) {
        ek_tfield *fs = group_fields(c, t, tv);

        if (!fs && t->u.kids.count) {
            return NULL;
        }
        return ck_record(c, fs, t->u.kids.count);
    }
    {
        const ek_type **items = NULL;
        size_t i, n = t->u.kids.count;

        if (n) {
            items = ek_arena_alloc(c->arena, n * sizeof *items);
            if (!items) {
                c->oom = 1;
                return NULL;
            }
            for (i = 0; i < n; i++) {
                items[i] = ck_elaborate(c, t->u.kids.data[i], tv);
            }
        }
        return ck_tuple(c, items, n);
    }
}

const ek_type *ck_elaborate(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    const ek_type *out = NULL;

    if (c->oom || !t) {
        return NULL;
    }
    if (t->kind == EK_N_T_NAME) {
        out = elab_name(c, t, tv);
        return out;
    }
    switch (t->kind) {
    case EK_N_T_APPLY:
        out = elab_apply(c, t, tv);
        break;
    case EK_N_T_LIST:
    case EK_N_T_PAREN: {
        const ek_type *inner = ck_elaborate(c, t->u.wrap.inner, tv);

        out = t->kind == EK_N_T_LIST ? ck_list(c, inner) : inner;
        break;
    }
    case EK_N_T_DICT: {
        const ek_type *k = ck_elaborate(c, t->u.pair.a, tv);
        const ek_type *v = ck_elaborate(c, t->u.pair.b, tv);

        out = ck_dict(c, k, v);
        break;
    }
    case EK_N_T_TUPLE:
    case EK_N_T_RECORD:
        out = elab_group(c, t, tv);
        break;
    case EK_N_T_FN:
        out = elab_fn(c, t, tv);
        break;
    case EK_N_T_PARAM:
    case EK_N_T_FIELD:
        out = ck_elaborate(c, t->u.t_field.type, tv);
        break;
    default:
        out = ck_ivar(c);
        break;
    }
    if (!out && c->oom) {
        return NULL;
    }
    ck_row(c, t, out, EK_RD_NONE);
    return out;
}

/* The record representation, binders replaced by the applied
   arguments. */
static const ek_type *repr_record(ck_ctx *c, const ck_decl *d,
                                  const ek_node *rec, const ek_type *nom,
                                  ck_tvars *tv)
{
    ek_tfield *fs;
    size_t i, n = rec->u.kids.count;

    fs = ek_arena_alloc(c->arena, n * sizeof *fs);
    if (!fs) {
        c->oom = 1;
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const ek_node *f = rec->u.kids.data[i];

        fs[i].name = f->u.t_field.name;
        fs[i].type = ck_apply_binders(c,
                                      ck_elaborate(c, f->u.t_field.type,
                                                   tv),
                                      d->binders, nom->u.nominal.args,
                                      d->nbinders);
    }
    return ck_record(c, fs, n);
}

/* The tuple representation, binders replaced likewise. */
static const ek_type *repr_tuple(ck_ctx *c, const ck_decl *d,
                                 const ek_node *rec, const ek_type *nom,
                                 ck_tvars *tv)
{
    const ek_type **items;
    size_t i, n = rec->u.kids.count;

    items = ek_arena_alloc(c->arena, n * sizeof *items);
    if (!items) {
        c->oom = 1;
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const ek_node *item = rec->u.kids.data[i];

        items[i] = ck_apply_binders(c,
                                    ck_elaborate(c,
                                                 item->kind == EK_N_T_FIELD
                                                     ? item->u.t_field
                                                           .type
                                                     : item,
                                                 tv),
                                    d->binders, nom->u.nominal.args,
                                    d->nbinders);
    }
    return ck_tuple(c, items, n);
}

/* The anonymous representation of a record- or tuple-shaped nominal
   type, with its binders replaced by the applied arguments. */
const ek_type *ck_decl_repr(ck_ctx *c, const ek_type *nom)
{
    const ck_decl *d;
    const ek_node *rec;
    ck_tvars tv;

    if (!nom->u.nominal.sym) {
        return NULL; /* std-opaque: no representation */
    }
    d = ck_decl_of(c, nom->u.nominal.sym);
    if (!d || (d->rhs != EK_TYPE_RHS_RECORD && d->rhs != EK_TYPE_RHS_TUPLE)) {
        return NULL;
    }
    if (d->nbinders && nom->u.nominal.argc != d->nbinders) {
        return NULL; /* unapplied: partial application does not exist */
    }
    rec = d->rec;
    if (!rec) {
        return NULL;
    }
    ck_binder_scope(c, d, &tv);
    if (d->rhs == EK_TYPE_RHS_RECORD) {
        return repr_record(c, d, rec, nom, &tv);
    }
    return repr_tuple(c, d, rec, nom, &tv);
}
