#include <string.h>

#include "check/internal.h"

/* Type positions (Clauses 7.6.1, 7.6.2): the elaboration of an
   annotation into a type, the type-variable scope of Clause 7.6.1, and
   the anonymous representation of a shaped nominal type (Clause
   7.5.5). Declared types are built in decl.c. */

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

void ck_tv_add(ck_ctx *c, ck_tvars *tv, const char *name, const ek_type *t,
               const ek_node *first)
{
    if (c->oom || !tv) {
        return;
    }
    if (tv->count == tv->cap) {
        size_t ncap = tv->cap ? tv->cap * 2 : 8;
        const char **nn = ek_arena_alloc(c->arena, ncap * sizeof *nn);
        const ek_type **nt = ek_arena_alloc(c->arena, ncap * sizeof *nt);
        const ek_node **nf = ek_arena_alloc(c->arena, ncap * sizeof *nf);

        if (!nn || !nt || !nf) {
            c->oom = 1;
            return;
        }
        if (tv->count) {
            memcpy(nn, tv->names, tv->count * sizeof *nn);
            memcpy(nt, tv->tvs, tv->count * sizeof *nt);
            memcpy(nf, tv->firsts, tv->count * sizeof *nf);
        }
        tv->names = nn;
        tv->tvs = nt;
        tv->firsts = nf;
        tv->cap = ncap;
    }
    tv->names[tv->count] = name;
    tv->tvs[tv->count] = t;
    tv->firsts[tv->count] = first;
    tv->count++;
    ck_map_put(c, &c->firsts, t, first);
}

/* The arity of an application of a declared type (Clause 7.6.2). */
static size_t type_arity(ck_ctx *c, const ek_symbol *sym)
{
    const ck_decl *d = sym ? ck_decl_of(c, sym) : NULL;

    return d ? d->nbinders : 0;
}

const ek_type *ck_elaborate(ck_ctx *c, const ek_node *t, ck_tvars *tv)
{
    const ek_type *out = NULL;
    const ek_ref *ref = NULL;

    if (c->oom || !t) {
        return NULL;
    }
    if (t->kind == EK_N_T_NAME) {
        const char *n0 = t->u.path.count ? t->u.path.data[0] : "";

        if (t->u.path.count == 1 && strcmp(n0, "_") == 0) {
            out = ck_unit(c); /* `_` is the unit type (7.5.4) */
        } else if (t->u.path.count == 1) {
            int lower = n0[0] >= 'a' && n0[0] <= 'z';

            if (lower) {
                /* A type variable of the enclosing annotation
                   (Clause 7.6.1); an unbound name generalizes. */
                const ek_type *v = tv_find(tv, n0);
                ek_type *fresh;

                if (v) {
                    out = v;
                } else {
                    fresh = ck_fresh(c, EK_T_TVAR);
                    if (!fresh) {
                        return NULL;
                    }
                    fresh->u.tvar.name = n0;
                    out = fresh;
                    ck_tv_add(c, tv, n0, out, t);
                }
            } else if (ck_ref_at(c, t, EK_SYM_TYPE, &ref) && ref->symbol) {
                size_t arity = type_arity(c, ref->symbol);

                if (arity) {
                    ck_error(c, t, "E-type-arity",
                             "the generic type `%s` needs %zu type "
                             "arguments (Clause 7.6.2)", ref->symbol->name,
                             arity);
                }
                out = ck_nominal(c, ref->symbol, NULL, 0);
            } else {
                out = ck_ivar(c); /* the placeholder std bridge */
            }
        } else if (t->u.path.count == 2) {
            const char *n1 = t->u.path.data[1];

            if (n1[0] >= 'a' && n1[0] <= 'z') {
                /* The identity space is PascalCase (Clauses 5.5.2,
                   7.4.1): a lowercase name in a qualified type. */
                ck_error(c, t, "E-name-case",
                         "the type name `%s` is not PascalCase "
                         "(Clause 5.5.2)", n1);
                out = ck_ivar(c);
            } else if (ck_ref_at(c, t, EK_SYM_TYPE, &ref) && ref->symbol) {
                size_t arity = type_arity(c, ref->symbol);

                if (arity) {
                    ck_error(c, t, "E-type-arity",
                             "the generic type `%s.%s` needs %zu type "
                             "arguments (Clause 7.6.2)", n0, n1, arity);
                }
                out = ck_nominal(c, ref->symbol, NULL, 0);
            } else {
                out = ck_ivar(c);
            }
        }
        ck_row(c, t, out, EK_RD_NONE);
        return out;
    }
    switch (t->kind) {
    case EK_N_T_APPLY: {
        const ek_node *head = t->u.t_apply.head;
        const ek_ref *href = NULL;
        const ek_symbol *sym = NULL;
        const ek_type **args = NULL;
        size_t i, n = t->u.t_apply.args.count, arity;

        if (head && head->kind == EK_N_T_NAME
            && ck_ref_at(c, head, EK_SYM_TYPE, &href)) {
            sym = href->symbol;
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
        out = sym ? ck_nominal(c, sym, args, n) : ck_ivar(c);
        break;
    }
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
    case EK_N_T_TUPLE: {
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
        out = ck_tuple(c, items, n);
        break;
    }
    case EK_N_T_RECORD: {
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
        out = ck_record(c, fs, n);
        break;
    }
    case EK_N_T_FN: {
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
                               : ck_unit(c); /* the elided arrow (6.9.3) */
        out = ck_fn(c, ps, n, ret, t->u.t_fn.arrow);
        break;
    }
    case EK_N_T_PARAM:
    case EK_N_T_FIELD:
        out = ck_elaborate(c, t->u.t_field.type, tv);
        break;
    default:
        out = ck_ivar(c);
        break;
    }
    ck_row(c, t, out, EK_RD_NONE);
    return out;
}

/* The binder scope of a declared type: the binders themselves, so
   representations and phantom checks elaborate against the nodes
   substitution replaces (Clause 7.6.4). */
void ck_binder_scope(ck_ctx *c, const ck_decl *d, ck_tvars *tv)
{
    const ek_node *dn = d->decl;
    size_t k;

    memset(tv, 0, sizeof *tv);
    if (!dn || dn->kind != EK_N_TYPE_DECL) {
        return;
    }
    for (k = 0; k < d->nbinders && k < dn->u.type_decl.params.count; k++) {
        ck_tv_add(c, tv, dn->u.type_decl.params.data[k], d->binders[k], dn);
    }
}

/* The anonymous representation of a record- or tuple-shaped nominal
   type, with its binders replaced by the applied arguments (Clause
   7.5.5). */
const ek_type *ck_decl_repr(ck_ctx *c, const ek_type *nom)
{
    const ck_decl *d;
    const ek_node *rec;
    ck_tvars tv;
    size_t i, n;

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
    n = rec->u.kids.count;
    if (d->rhs == EK_TYPE_RHS_RECORD) {
        ek_tfield *fs = ek_arena_alloc(c->arena, n * sizeof *fs);

        if (!fs) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            const ek_node *f = rec->u.kids.data[i];

            fs[i].name = f->u.t_field.name;
            fs[i].type = ck_apply_binders(c,
                                          ck_elaborate(c, f->u.t_field.type,
                                                       &tv),
                                          d->binders, nom->u.nominal.args,
                                          d->nbinders);
        }
        return ck_record(c, fs, n);
    }
    {
        const ek_type **items = ek_arena_alloc(c->arena, n * sizeof *items);

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
                                                     &tv),
                                        d->binders, nom->u.nominal.args,
                                        d->nbinders);
        }
        return ck_tuple(c, items, n);
    }
}
