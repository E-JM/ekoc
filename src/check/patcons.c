#include "check/internal.h"

/* Constructor patterns: the constructor a pattern selects comes from
   the scrutinee type and is recorded for coverage, a known ADT checks
   the selection against its own declaration, and the payload walks at
   the instantiated payload. A scrutinee the bridge leaves opaque
   binds freely, never a new error. */

/* The payload walk of a constructor pattern that matched its ADT: the
   instantiation slots are the fresh variables the scrutinee conformed
   to, and the payload pattern is checked at the instantiated
   payload. */
static void cons_payload(ck_ctx *c, const ek_node *p, ek_type **slots,
                         const ck_ctor *ct, const ck_decl *d, size_t n)
{
    const ek_type **repl = NULL;
    const ek_type *pt;

    if (n) {
        size_t i;

        repl = ek_arena_alloc(c->arena, n * sizeof *repl);
        if (!repl) {
            c->oom = 1;
            return;
        }
        for (i = 0; i < n; i++) {
            repl[i] = ck_prune(slots[i]);
        }
    }
    pt = ck_apply_binders(c, ct->payload, d->binders, repl, n);
    ck_pattern(c, p->u.cons.payload, pt);
}

/* One fresh instantiation slot per binder: the variables the
   scrutinee conforms to. */
static int cons_slots(ck_ctx *c, size_t n, ek_type ***slots,
                      const ek_type ***args)
{
    size_t i;

    *slots = ek_arena_alloc(c->arena, n * sizeof **slots);
    *args = ek_arena_alloc(c->arena, n * sizeof **args);
    if (!*slots || !*args) {
        c->oom = 1;
        return 0;
    }
    for (i = 0; i < n; i++) {
        (*slots)[i] = (ek_type *)ck_ivar(c);
        (*args)[i] = (*slots)[i];
    }
    return 1;
}

/* The scrutinee's own ADT: a user declaration. A std placeholder
   type has no declaration and its case structure stays opaque. */
static int known_adt(const ek_type *s)
{
    return s && s->kind == EK_T_NOMINAL && s->u.nominal.sym
        && s->u.nominal.sym->decl != NULL;
}

/* The scrutinee type anchors an unqualified pattern's constructor;
   a qualified pattern names one declaration directly. */
static const ek_symbol *cons_select(const ek_ref *ref,
                                    const ek_type *scrut)
{
    if (!ref) {
        return NULL;
    }
    if (ref->ncands) {
        return ck_ctor_pick(ref, scrut);
    }
    if (ref->symbol && ref->symbol->kind == EK_SYM_CONSTRUCTOR) {
        return ref->symbol;
    }
    return NULL;
}

static const ek_type *opaque_cons(ck_ctx *c, const ek_node *p,
                                  const ek_type *scrut);

/* A pattern over a known ADT: the selected constructor must belong to
   the scrutinee's type, and its payload walks at the instantiated
   payload type. */
static const ek_type *adt_cons(ck_ctx *c, const ek_node *p,
                               const ek_type *scrut, const ek_ref *ref,
                               const ek_symbol *sel)
{
    const ck_ctor *ct = sel ? ck_ctor_of(c, sel) : NULL;
    const ek_symbol *name = sel ? sel : (ref ? ref->symbol : NULL);
    ek_type **slots = NULL;
    const ek_type **args = NULL;
    const ek_type *nom;
    size_t n;
    ck_conform k;

    if (!ct || !ct->owner) {
        ck_error(c, p, ck_conform_code(CK_NOMINAL),
                 "the constructor `%s` does not match the scrutinee "
                 "type %s (Clause 7.7.1)",
                 name ? name->name : "?", ck_type_name(c, scrut));
        return opaque_cons(c, p, scrut);
    }
    {
        const ck_decl *d = ck_decl_of(c, ct->owner);

        n = d ? d->nbinders : 0;
    }
    if (n && !cons_slots(c, n, &slots, &args)) {
        return scrut;
    }
    nom = ck_nominal(c, ct->owner, args, n);
    k = ck_conforms(c, scrut, nom, 0);
    if (k != CK_OK) {
        ck_error(c, p, ck_conform_code(k),
                 "the constructor `%s` does not match the scrutinee "
                 "type %s (Clause 7.7.1)", name->name,
                 ck_type_name(c, scrut));
        return opaque_cons(c, p, scrut);
    }
    if (ct->payload && p->u.cons.payload) {
        const ck_decl *d = ck_decl_of(c, ct->owner);

        if (d) {
            cons_payload(c, p, slots, ct, d, n);
        }
    } else if (ct->payload) {
        ck_error(c, p, "E-type-mismatch",
                 "the constructor `%s` carries a payload, the pattern "
                 "supplies none (Clause 7.7.1)", name->name);
    } else if (p->u.cons.payload) {
        ck_error(c, p->u.cons.payload, "E-type-mismatch",
                 "the constructor `%s` takes no payload (Clause 7.7.1)",
                 name->name);
    }
    return scrut;
}

/* An opaque scrutinee: the placeholder-std bridge and the unknown
   readings bind freely, never a new error. */
static const ek_type *opaque_cons(ck_ctx *c, const ek_node *p,
                                  const ek_type *scrut)
{
    if (p->u.cons.payload) {
        const ek_type *whole = ck_opaque(c);
        const ek_type *pl = ck_opaque(c);

        ck_conforms(c, scrut, whole, 0);
        ck_pattern(c, p->u.cons.payload, pl);
    }
    return scrut;
}

/* A constructor pattern's constructor comes from the scrutinee type;
   the selection is recorded for coverage. */
const ek_type *cons_pattern(ck_ctx *c, const ek_node *p,
                            const ek_type *scrut)
{
    const ek_ref *ref = NULL;
    const ek_symbol *sel;
    const ek_type *s = ck_prune(scrut);

    (void)ck_ref_at(c, p, 0, &ref);
    sel = cons_select(ref, s);
    if (sel) {
        ck_map_put(c, &c->pats, p, sel);
    }
    if (known_adt(s)) {
        return adt_cons(c, p, scrut, ref, sel);
    }
    return opaque_cons(c, p, scrut);
}

/* The constructor symbol a constructor pattern selects against a type:
   the case-arm selection offered to the binding-shape walk. */
const ek_symbol *ck_cons_select(ck_ctx *c, const ek_node *p,
                                const ek_type *scrut)
{
    const ek_ref *ref = NULL;

    (void)ck_ref_at(c, p, 0, &ref);
    return cons_select(ref, ck_prune(scrut));
}
