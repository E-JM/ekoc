#include <string.h>

#include "lower/internal.h"

/* The constructor families: one registered struct and typeinfo per
   variant, the helper that builds it, and the registrations a pattern
   test, a value construction, and an intrinsic dispatch each need. */
/* The constructor a reference denotes, re-derived the way check picks
   one: the candidate owned by the expected type's declaration. */
const ek_symbol *lower_pick_ctor(const ek_ref *ref, const ek_symbol *tsym)
{
    size_t i;

    for (i = 0; i < ref->ncands; i++) {
        const ek_symbol *cand = ref->cands[i];

        if (cand && cand->owner && cand->module && tsym->module
            && strcmp(cand->owner, tsym->name) == 0
            && strcmp(cand->module, tsym->module) == 0) {
            return cand;
        }
    }
    if (ref->ncands == 0 && ref->symbol && ref->symbol->owner
        && strcmp(ref->symbol->owner, tsym->name) == 0) {
        return ref->symbol;
    }
    return NULL;
}

/* The family of one named variant: the payload arity and the helper
   spelling follow the variant, the identity the constructor. */
static lower_gtype *named_ctor(lower_ctx *c, const ek_symbol *cs,
                               const ek_node *v, int construct)
{
    const char **slots;
    lower_gtype *g;
    size_t cap = 0;

    g = lower_add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_sym = cs;
    g->stem = lower_str(c, "%s$%s$%s", cs->module, cs->owner, cs->name);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_CTOR;
    g->nf = v->u.variant.payload ? 1 : 0;
    g->used = 1;
    g->constructed = construct;
    if (g->nf) {
        slots = lower_grow(c, NULL, &cap, 1, sizeof *slots);
        if (slots) {
            slots[0] = "";
            g->fnames = slots;
        }
    }
    return g;
}

/* Registers the family of one constructor over its owner's
   declaration: one struct per constructor, its payload the variant's
   own. A construction-site registration marks the helper live; a
   shared registration (the runtime builds the values) only stamps the
   identity. */
lower_gtype *lower_register_ctor(lower_ctx *c, const ek_node *at,
                                  const ek_symbol *cs, const ek_node *dn,
                                  int construct)
{
    lower_gtype *g;
    size_t i;

    if (!cs || !dn || dn->kind != EK_N_TYPE_DECL) {
        lower_error(c, at, "the constructor `%s` is not supported by "
                          "this lowerer",
                    cs ? cs->name : "?");
        return NULL;
    }
    g = lower_find_family(c, cs, NULL);
    if (g) {
        if (construct) {
            g->constructed = 1;
        }
        return g;
    }
    for (i = 0; i < dn->u.type_decl.variants.count; i++) {
        const ek_node *v = dn->u.type_decl.variants.data[i];

        if (strcmp(v->u.variant.name, cs->name) == 0) {
            return named_ctor(c, cs, v, construct);
        }
    }
    lower_error(c, at, "the constructor `%s` is not supported by this "
                       "lowerer", cs->name);
    return NULL;
}

/* Registers the constructor family a pattern tests: the typeinfo
   identity alone, no construction helper -- the runtime and the wrap
   sites may be the only builders. */
lower_gtype *lower_ctor_family(lower_ctx *c, const ek_node *at,
                               const ek_ref *ref, const ek_symbol *tsym)
{
    const ek_symbol *cs = tsym ? lower_pick_ctor(ref, tsym) : NULL;
    lower_gtype *g = lower_register_ctor(c, at, cs, tsym ? tsym->decl : NULL, 0);

    if (g) {
        g->shared = 1;
    }
    return g;
}

/* Registers the constructor family a value position builds: the
   construction helper joins the unit and the site calls it. */
lower_gtype *lower_ctor_built(lower_ctx *c, const ek_node *at,
                              const ek_ref *ref, const ek_symbol *tsym)
{
    const ek_symbol *cs = tsym ? lower_pick_ctor(ref, tsym) : NULL;

    return lower_register_ctor(c, at, cs, tsym ? tsym->decl : NULL, 1);
}

/* Registers the constructor family of a symbol the intrinsic dispatch
   wraps with: no reference node stands behind it, so the owner type's
   declaration in the constructor's own module supplies the variants.
   A zero construct stamps the identity for the runtime's builders
   without emitting a helper. */
lower_gtype *lower_ctor_of_symbol(lower_ctx *c, const ek_node *at,
                                  const ek_symbol *cs, int construct)
{
    const ek_symbol *tsym = NULL;
    size_t k, i;

    if (!cs || !cs->owner || !cs->module) {
        lower_error(c, at, "the constructor `%s` is not supported by "
                          "this lowerer",
                    cs ? cs->name : "?");
        return NULL;
    }
    for (k = 0; k < c->resolved.module_count && !tsym; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, cs->module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_TYPE && strcmp(m->name, cs->owner) == 0) {
                tsym = m;
                break;
            }
        }
    }
    return lower_register_ctor(c, at, cs, tsym ? tsym->decl : NULL, construct);
}
