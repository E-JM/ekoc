#include <string.h>

#include "check/internal.h"

/* Declared types: the ck_decl and ck_ctor rows behind the surface
   symbols, built on demand with the shell registered first so a
   recursive right-hand side sees its own shape. The phantom-binder
   check runs here. */

/* The binder scope of a declared type: the binders themselves, so
   representations and phantom checks elaborate against the nodes
   substitution replaces. */
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

/* The constructor symbol of one variant of a declared type. */
static const ek_symbol *ctor_sym(ck_ctx *c, const ek_symbol *owner,
                                 const ek_node *dn, size_t k)
{
    const char *name;
    size_t i;

    if (!dn) {
        dn = owner->decl;
    }
    if (!dn || dn->kind != EK_N_TYPE_DECL
        || k >= dn->u.type_decl.variants.count) {
        return NULL;
    }
    name = dn->u.type_decl.variants.data[k]->u.variant.name;
    for (i = 0; i < c->in->module_count; i++) {
        const ek_surface *s = &c->in->modules[i];
        size_t j;

        if (strcmp(s->module, owner->module) != 0) {
            continue;
        }
        for (j = 0; j < s->count; j++) {
            const ek_symbol *m = &s->syms[j];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, owner->name) == 0
                && strcmp(m->name, name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* One constructor row: symbol, variant, and its payload in the
   declaration's binders. */
static const ck_ctor *build_ctor(ck_ctx *c, const ek_symbol *sym,
                                 const ek_symbol *owner, const ek_node *v,
                                 ck_tvars *tv)
{
    ck_ctor *ct = ek_arena_alloc(c->arena, sizeof *ct);

    if (!ct) {
        c->oom = 1;
        return NULL;
    }
    memset(ct, 0, sizeof *ct);
    ct->sym = sym;
    ct->owner = owner;
    ct->variant = v;
    if (v->u.variant.payload) {
        ct->payload = ck_elaborate(c, v->u.variant.payload, tv);
    }
    if (sym) {
        ck_map_put(c, &c->ctors, sym, ct);
    }
    return ct;
}

/* Phantom binders: every binder MUST occur in the right-hand side. */
static void check_phantom_binders(ck_ctx *c, const ck_decl *info)
{
    const ek_type **all = NULL;
    const ek_node *dn = info->decl;
    size_t nall = 0, cap = 0, k, j;

    for (k = 0; k < info->nctors; k++) {
        const ck_ctor *ct = info->ctors[k];

        if (ct && ct->payload) {
            ck_tvars_collect(c, ct->payload, &all, &nall, &cap);
        }
    }
    if (info->rec) {
        ck_tvars tv;

        ck_binder_scope(c, info, &tv);
        {
            const ek_type *rt = ck_elaborate(c, info->rec, &tv);

            if (rt) {
                ck_tvars_collect(c, rt, &all, &nall, &cap);
            }
        }
    }
    if (c->oom) {
        return;
    }
    for (k = 0; k < info->nbinders; k++) {
        int occurs = 0;

        for (j = 0; j < nall; j++) {
            if (all[j] == info->binders[k]) {
                occurs = 1;
            }
        }
        if (!occurs) {
            ck_error(c, dn, "E-phantom",
                     "the binder `%s` does not occur in the right-hand "
                     "side (Clause 7.6.2)",
                     dn->u.type_decl.params.data[k]);
        }
    }
}

/* The rigid binder nodes of one declaration, in order. */
static int decl_binders(ck_ctx *c, ck_decl *info, const ek_node *dn,
                        ck_tvars *tv)
{
    size_t k;

    info->nbinders = dn->u.type_decl.params.count;
    if (!info->nbinders) {
        return 1;
    }
    info->binders = ek_arena_alloc(c->arena,
                                   info->nbinders * sizeof *info->binders);
    if (!info->binders) {
        c->oom = 1;
        return 0;
    }
    for (k = 0; k < info->nbinders; k++) {
        ek_type *fresh = ck_fresh(c, EK_T_TVAR);

        if (!fresh) {
            c->oom = 1;
            return 0;
        }
        fresh->u.tvar.name = dn->u.type_decl.params.data[k];
        info->binders[k] = fresh;
        ck_tv_add(c, tv, dn->u.type_decl.params.data[k], fresh, dn);
    }
    return 1;
}

/* The constructor row of one variant, registered under its symbol. */
static const ck_ctor *decl_ctor(ck_ctx *c, const ck_decl *info,
                                const ek_node *dn, size_t k, ck_tvars *tv)
{
    const ek_symbol *csym = ctor_sym(c, info->sym, dn, k);

    return build_ctor(c, csym, info->sym, dn->u.type_decl.variants.data[k],
                      tv);
}

/* The declared type behind a surface symbol. */
static const ck_decl *build_decl(ck_ctx *c, const ek_symbol *sym)
{
    const ek_node *dn = sym->decl;
    ck_decl *info = ek_arena_alloc(c->arena, sizeof *info);
    ck_tvars tv;
    size_t k;

    if (!info) {
        c->oom = 1;
        return NULL;
    }
    memset(info, 0, sizeof *info);
    info->sym = sym;
    info->decl = dn;
    ck_map_put(c, &c->decls, sym, info);
    if (!dn) {
        return info; /* a std placeholder type: opaque */
    }
    if (dn->kind == EK_N_TYPE_DECL) {
        info->rhs = dn->u.type_decl.rhs;
        info->rec = dn->u.type_decl.type;
        memset(&tv, 0, sizeof tv);
        if (!decl_binders(c, info, dn, &tv)) {
            return info;
        }
        info->nctors = dn->u.type_decl.variants.count;
        if (info->nctors) {
            info->ctors = ek_arena_alloc(c->arena,
                                         info->nctors * sizeof *info->ctors);

            if (!info->ctors) {
                c->oom = 1;
                return info;
            }
            for (k = 0; k < info->nctors; k++) {
                info->ctors[k] = decl_ctor(c, info, dn, k, &tv);
            }
        }
        check_phantom_binders(c, info);
    } else if (dn->kind == EK_N_CTYPE_DECL) {
        info->rhs = EK_TYPE_RHS_RECORD;
        info->rec = dn->u.decl.body; /* fields judged in extern.c */
    }
    return info;
}

const ck_decl *ck_decl_of(ck_ctx *c, const ek_symbol *sym)
{
    const ck_decl *d;

    if (!sym) {
        return NULL;
    }
    d = (const ck_decl *)ck_map_get(&c->decls, sym);
    if (d) {
        return d;
    }
    return build_decl(c, sym);
}

/* The declared type that owns a constructor symbol: the type symbol of
   the constructor's module whose declaration node is the same. */
static const ek_symbol *ctor_owner(ck_ctx *c, const ek_symbol *sym)
{
    size_t i, j;

    if (!sym->decl || !sym->owner || !sym->module) {
        return NULL;
    }
    for (i = 0; i < c->in->module_count; i++) {
        const ek_surface *s = &c->in->modules[i];

        if (strcmp(s->module, sym->module) != 0) {
            continue;
        }
        for (j = 0; j < s->count; j++) {
            const ek_symbol *m = &s->syms[j];

            if (m->kind == EK_SYM_TYPE && m->decl == sym->decl
                && strcmp(m->name, sym->owner) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

const ck_ctor *ck_ctor_of(ck_ctx *c, const ek_symbol *sym)
{
    const ck_ctor *ct;
    const ek_symbol *owner;

    if (!sym) {
        return NULL;
    }
    ct = (const ck_ctor *)ck_map_get(&c->ctors, sym);
    if (ct || c->oom) {
        return ct;
    }
    owner = ctor_owner(c, sym);
    if (owner) {
        (void)ck_decl_of(c, owner);
    }
    return (const ck_ctor *)ck_map_get(&c->ctors, sym);
}
