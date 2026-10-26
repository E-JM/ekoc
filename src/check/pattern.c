#include <string.h>

#include "check/internal.h"

/* Pattern typing: a pattern is checked against the scrutinee type;
   the expected type decides the brace family, as it decides a
   composite literal. A constructor payload is the instantiated
   payload. Or-patterns and the value binding shapes live in
   patbind.c. */

/* A literal pattern is placed at the scrutinee type; against an
   internally known composite it is a type error. */
static const ek_type *lit_pattern(ck_ctx *c, const ek_node *p,
                                  const ek_type *scrut)
{
    const ek_node *lit = p->u.wrap.inner;
    int neg = lit && lit->kind == EK_N_UNARY;
    const ek_type *s = ck_prune(scrut);

    if (neg) {
        lit = lit->u.wrap.inner;
    }
    if (lit) {
        ck_place(c, lit, scrut, neg);
    }
    if (s && (s->kind == EK_T_LIST || s->kind == EK_T_DICT
              || s->kind == EK_T_TUPLE || s->kind == EK_T_RECORD
              || s->kind == EK_T_FN
              || (s->kind == EK_T_NOMINAL && s->u.nominal.sym
                  && s->u.nominal.sym->decl))) {
        ck_error(c, p, "E-type-mismatch",
                 "a literal pattern does not match %s (Clause 7.7.1)",
                 ck_type_name(c, s));
    }
    return scrut;
}

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
        return scrut;
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
        return scrut;
    }
    if (ct->payload) {
        const ck_decl *d = ck_decl_of(c, ct->owner);

        if (d) {
            cons_payload(c, p, slots, ct, d, n);
        }
    }
    return scrut;
}

/* An opaque scrutinee: the placeholder-std bridge and the unknown
   readings bind freely, never a new error. */
static const ek_type *opaque_cons(ck_ctx *c, const ek_node *p,
                                  const ek_type *scrut)
{
    if (p->u.cons.payload) {
        const ek_type *whole = ck_ivar(c);
        const ek_type *pl = ck_ivar(c);

        ck_conforms(c, scrut, whole, 0);
        ck_pattern(c, p->u.cons.payload, pl);
    }
    return scrut;
}

/* A constructor pattern's constructor comes from the scrutinee type;
   the selection is recorded for coverage. */
static const ek_type *cons_pattern(ck_ctx *c, const ek_node *p,
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

/* Record-pattern item: the field name, whether a `f: P` pair or the
   shorthand `f`. */
static const char *pat_field(const ek_node *item, const ek_node **sub)
{
    if (item->kind == EK_N_P_FIELD) {
        *sub = item->u.p_field.pattern;
        return item->u.p_field.name;
    }
    if (item->kind == EK_N_P_VAR) {
        *sub = item;
        return item->u.path.count == 1 ? item->u.path.data[0] : NULL;
    }
    *sub = NULL;
    return NULL;
}

static const ek_type *record_pattern(ck_ctx *c, const ek_node *p,
                                     const ek_type *rec)
{
    size_t i, k;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        const ek_node *item = p->u.p_brace.items.data[i];
        const ek_node *sub = NULL;
        const char *field = pat_field(item, &sub);
        const ek_type *ft = NULL;

        if (!field) {
            continue;
        }
        for (k = 0; k < rec->u.record.count; k++) {
            if (strcmp(rec->u.record.fields[k].name, field) == 0) {
                ft = rec->u.record.fields[k].type;
            }
        }
        if (!ft) {
            ck_error(c, item, "E-field-missing",
                     "the record type has no field `%s` (Clause 7.7.1)",
                     field);
            continue;
        }
        if (sub && sub != item) {
            ck_pattern(c, sub, ft);
        } else if (sub) {
            /* Shorthand: the item itself binds. */
            ck_scope_bind(c, field, ft);
        }
    }
    return rec;
}

static const ek_type *list_pattern(ck_ctx *c, const ek_node *p,
                                   const ek_type *lst)
{
    size_t i;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        ck_pattern(c, p->u.p_brace.items.data[i], lst->u.list.elem);
    }
    if (p->u.p_brace.rest_name) {
        ck_scope_bind(c, p->u.p_brace.rest_name, lst);
    }
    return lst;
}

static const ek_type *tuple_pattern(ck_ctx *c, const ek_node *p,
                                    const ek_type *tup)
{
    size_t i, n = 0;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        if (p->u.p_brace.items.data[i]->kind != EK_N_P_FIELD) {
            n++;
        }
    }
    if (p->u.p_brace.rest != EK_REST_NONE || n != tup->u.tuple.count) {
        ck_error(c, p, "E-type-mismatch",
                 "the tuple pattern has %zu components, the scrutinee has "
                 "%zu (Clause 7.7.1)", n, tup->u.tuple.count);
    }
    for (i = 0; i < p->u.p_brace.items.count && i < tup->u.tuple.count; i++) {
        ck_pattern(c, p->u.p_brace.items.data[i], tup->u.tuple.items[i]);
    }
    return tup;
}

/* The opaque readings: the brace family is not decidable, so items
   bind against fresh variables and nothing is rejected. */
static const ek_type *opaque_pattern(ck_ctx *c, const ek_node *p)
{
    size_t i;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        const ek_node *item = p->u.p_brace.items.data[i];
        const ek_node *sub = NULL;
        const char *field = pat_field(item, &sub);

        if (field && sub && sub != item) {
            ck_pattern(c, sub, ck_ivar(c));
        } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
            ck_scope_bind(c, item->u.path.data[0], ck_ivar(c));
        } else if (item->kind != EK_N_P_FIELD) {
            ck_pattern(c, item, ck_ivar(c));
        }
    }
    if (p->u.p_brace.rest_name) {
        ck_scope_bind(c, p->u.p_brace.rest_name, ck_ivar(c));
    }
    return ck_ivar(c);
}

static const ek_type *brace_pattern(ck_ctx *c, const ek_node *p,
                                    const ek_type *scrut)
{
    const ek_type *s = ck_prune(scrut);

    if (s && s->kind == EK_T_LIST) {
        return list_pattern(c, p, s);
    }
    if (s && s->kind == EK_T_TUPLE) {
        return tuple_pattern(c, p, s);
    }
    if (s && s->kind == EK_T_RECORD) {
        return record_pattern(c, p, s);
    }
    if (s && s->kind == EK_T_NOMINAL && s->u.nominal.sym) {
        const ek_type *repr = ck_decl_repr(c, s);

        if (repr && repr->kind == EK_T_RECORD) {
            return record_pattern(c, p, repr);
        }
        if (repr && repr->kind == EK_T_TUPLE) {
            return tuple_pattern(c, p, repr);
        }
    }
    if (!s || s->kind == EK_T_IVAR || s->kind == EK_T_TVAR
        || (s->kind == EK_T_NOMINAL && !s->u.nominal.sym)) {
        return opaque_pattern(c, p);
    }
    ck_error(c, p, "E-type-mismatch",
             "a brace pattern does not match %s (Clause 7.7.1)",
             ck_type_name(c, s));
    return opaque_pattern(c, p);
}

const ek_type *ck_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut)
{
    if (c->oom || !p) {
        return scrut;
    }
    ck_row(c, p, scrut, EK_RD_NONE);
    switch (p->kind) {
    case EK_N_P_WILDCARD:
        return scrut;
    case EK_N_P_VAR:
        if (p->u.path.count == 1 && strcmp(p->u.path.data[0], "_") != 0) {
            ck_scope_bind(c, p->u.path.data[0], scrut);
        }
        return scrut;
    case EK_N_P_LIT:
        return lit_pattern(c, p, scrut);
    case EK_N_P_CONS:
        return cons_pattern(c, p, scrut);
    case EK_N_P_BRACE:
        return brace_pattern(c, p, scrut);
    case EK_N_P_PAREN:
        if (p->u.kids.count == 1) {
            return ck_pattern(c, p->u.kids.data[0], scrut);
        }
        return scrut; /* multi-scrutinee groups are typed per column */
    case EK_N_P_OR:
        return ck_or_pattern(c, p, scrut);
    case EK_N_P_FIELD:
        return ck_pattern(c, p->u.p_field.pattern, scrut);
    default:
        return scrut;
    }
}
