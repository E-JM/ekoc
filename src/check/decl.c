#include <string.h>

#include "check/internal.h"

/* Declared types (Clauses 7.5.2, 7.5.6, 7.6.2): the ck_decl and
   ck_ctor rows behind the surface symbols, built on demand with the
   shell registered first so a recursive right-hand side sees its own
   shape. The phantom-binder check runs here. */

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

/* Phantom binders (Clause 7.6.2): every binder MUST occur in the
   right-hand side. */
static void check_phantom_binders(ck_ctx *c, const ck_decl *info)
{
    const ek_type **all = ek_arena_alloc(c->arena, 64 * sizeof *all);
    const ek_node *dn = info->decl;
    size_t nall = 0, k, j;

    if (!all) {
        c->oom = 1;
        return;
    }
    for (k = 0; k < info->nctors; k++) {
        const ck_ctor *ct = info->ctors[k];

        if (ct && ct->payload) {
            ck_tvars_collect(ct->payload, &all, &nall);
        }
    }
    if (info->rec) {
        ck_tvars tv;

        ck_binder_scope(c, info, &tv);
        {
            const ek_type *rt = ck_elaborate(c, info->rec, &tv);

            if (rt) {
                ck_tvars_collect(rt, &all, &nall);
            }
        }
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
        info->nbinders = dn->u.type_decl.params.count;
        memset(&tv, 0, sizeof tv);
        if (info->nbinders) {
            const ek_type **bs = ek_arena_alloc(c->arena,
                                                info->nbinders * sizeof *bs);

            if (!bs) {
                c->oom = 1;
                return info;
            }
            info->binders = bs;
            for (k = 0; k < info->nbinders; k++) {
                ek_type *fresh = ck_fresh(c, EK_T_TVAR);

                if (!fresh) {
                    c->oom = 1;
                    return info;
                }
                fresh->u.tvar.name = dn->u.type_decl.params.data[k];
                bs[k] = fresh;
                ck_tv_add(c, &tv, dn->u.type_decl.params.data[k], fresh, dn);
            }
        }
        info->nctors = dn->u.type_decl.variants.count;
        if (info->nctors) {
            const ck_ctor **cs = ek_arena_alloc(c->arena,
                                                info->nctors * sizeof *cs);

            if (!cs) {
                c->oom = 1;
                return info;
            }
            info->ctors = cs;
            for (k = 0; k < info->nctors; k++) {
                const ek_symbol *csym = ctor_sym(c, sym, dn, k);

                cs[k] = build_ctor(c, csym, sym,
                                   dn->u.type_decl.variants.data[k], &tv);
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

const ck_ctor *ck_ctor_of(ck_ctx *c, const ek_symbol *sym)
{
    if (!sym) {
        return NULL;
    }
    return (const ck_ctor *)ck_map_get(&c->ctors, sym);
}
