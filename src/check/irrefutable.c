#include <string.h>

#include "check/internal.h"

/* A value binding admits only irrefutable patterns: `_`, a variable,
   a tuple or group of irrefutable patterns, a record pattern whose
   subpatterns are irrefutable, and the sole constructor of a
   single-variant ADT at an irrefutable payload pattern. The judgment
   is against the bound type: a family the type cannot decide is
   refutable. */

/* The record or tuple shape behind a pattern position's type, when one
   is decidable. */
static const ek_type *brace_repr_of(ck_ctx *c, const ek_type *t)
{
    const ek_type *s = ck_prune(t);

    if (s && (s->kind == EK_T_RECORD || s->kind == EK_T_TUPLE)) {
        return s;
    }
    if (s && s->kind == EK_T_NOMINAL && s->u.nominal.sym) {
        return ck_decl_repr(c, s);
    }
    return NULL;
}

/* The sole constructor a constructor pattern selects: the selection
   belongs to the scrutinee's declaration and is its only variant. The
   payload's own judgment belongs to the caller. */
static const ck_ctor *sole_ctor(ck_ctx *c, const ek_node *p, const ek_type *t)
{
    const ek_type *s = ck_prune(t);
    const ek_symbol *sel;
    const ck_ctor *ct;
    const ck_decl *d;

    if (!s || s->kind != EK_T_NOMINAL || !s->u.nominal.sym
        || !s->u.nominal.sym->decl) {
        return NULL;
    }
    sel = ck_cons_select(c, p, s);
    ct = sel ? ck_ctor_of(c, sel) : NULL;
    if (!ct || ct->owner != s->u.nominal.sym) {
        return NULL;
    }
    d = ck_decl_of(c, ct->owner);
    if (!d || d->nctors != 1 || d->ctors[0] != ct) {
        return NULL;
    }
    return ct;
}

/* A constructor pattern's irrefutability: the sole constructor of the
   scrutinee's ADT, whose payload pattern walks at the instantiated
   payload. A payload-arity mismatch stays admissible here so the
   pattern walk names it precisely. */
static int cons_irrefutable(ck_ctx *c, const ek_node *p, const ek_type *t)
{
    const ek_type *s = ck_prune(t);
    const ck_ctor *ct = sole_ctor(c, p, t);
    const ck_decl *d;
    const ek_type **repl = NULL;
    const ek_type *pt;
    size_t n, i;

    if (!ct) {
        return 0;
    }
    if (!ct->payload || !p->u.cons.payload) {
        return 1; /* a payload-arity mismatch is the walk's error */
    }
    d = ck_decl_of(c, ct->owner);
    n = d ? d->nbinders : 0;
    if (n && (!s || s->u.nominal.argc < n)) {
        return 0;
    }
    if (n) {
        repl = ek_arena_alloc(c->arena, n * sizeof *repl);
        if (!repl) {
            c->oom = 1;
            return 1;
        }
        for (i = 0; i < n; i++) {
            repl[i] = ck_prune(s->u.nominal.args[i]);
        }
    }
    pt = ck_apply_binders(c, ct->payload, d->binders, repl, n);
    return ck_irrefutable(c, p->u.cons.payload, pt);
}

/* The field type a record item binds at, the component type of a
   tuple item, and NULL for a tuple item past the arity. A record item
   whose field the shape does not carry is named by the pattern walk,
   so the caller treats it as admissible. */
static const ek_type *item_field_type(const ek_type *repr,
                                      const ek_node *item, size_t i,
                                      int *named_miss)
{
    const char *field = NULL;
    const ek_type *ft = NULL;
    size_t k;

    *named_miss = 0;
    if (repr->kind == EK_T_RECORD) {
        if (item->kind == EK_N_P_FIELD) {
            field = item->u.p_field.name;
        } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
            field = item->u.path.data[0];
        }
        for (k = 0; field && k < repr->u.record.count; k++) {
            if (strcmp(repr->u.record.fields[k].name, field) == 0) {
                ft = repr->u.record.fields[k].type;
            }
        }
        if (field && !ft) {
            *named_miss = 1;
        }
        return ft;
    }
    return i < repr->u.tuple.count ? repr->u.tuple.items[i] : NULL;
}

/* A group of irrefutable patterns over a tuple type: each component
   walks at its component type. One component is no tuple: the group
   is the pattern's own parentheses, and the component walks at the
   bound type. */
static int paren_irrefutable(ck_ctx *c, const ek_node *p, const ek_type *t)
{
    const ek_type *repr = brace_repr_of(c, t);
    size_t i;

    if (p->u.kids.count == 1) {
        return ck_irrefutable(c, p->u.kids.data[0], t);
    }
    /* a group destructures a tuple: the arity must answer at the bound
       type, so a group over anything else is refutable */
    if (!repr || repr->kind != EK_T_TUPLE
        || repr->u.tuple.count != p->u.kids.count) {
        return 0;
    }
    for (i = 0; i < p->u.kids.count; i++) {
        const ek_type *ct = repr && repr->kind == EK_T_TUPLE
                                && i < repr->u.tuple.count
                                ? repr->u.tuple.items[i] : NULL;

        if (!ck_irrefutable(c, p->u.kids.data[i], ct)) {
            return 0;
        }
    }
    return 1;
}

/* A brace pattern over a record or tuple shape: every item's
   subpattern is irrefutable at its field or component type. A field
   the shape does not carry stays admissible here; the pattern walk
   names the miss precisely. */
static int brace_irrefutable(ck_ctx *c, const ek_node *p, const ek_type *t)
{
    const ek_type *repr;
    size_t i;

    if (p->u.p_brace.rest != EK_REST_NONE) {
        return 0; /* a list pattern */
    }
    repr = brace_repr_of(c, t);
    if (!repr || (repr->kind != EK_T_RECORD && repr->kind != EK_T_TUPLE)) {
        return 0; /* a list, an ADT, an opaque, or a scalar family */
    }
    for (i = 0; i < p->u.p_brace.items.count; i++) {
        const ek_node *item = p->u.p_brace.items.data[i];
        int named_miss = 0;
        const ek_type *ft = item_field_type(repr, item, i, &named_miss);
        const ek_node *sub = item->kind == EK_N_P_FIELD
                                 ? item->u.p_field.pattern : item;

        if (named_miss) {
            return 1; /* the pattern walk names the field */
        }
        if (!ck_irrefutable(c, sub, ft)) {
            return 0;
        }
    }
    return 1;
}

int ck_irrefutable(ck_ctx *c, const ek_node *p, const ek_type *t)
{
    if (!p) {
        return 1;
    }
    switch (p->kind) {
    case EK_N_P_WILDCARD:
    case EK_N_P_VAR:
        return 1;
    case EK_N_P_CONS:
        return cons_irrefutable(c, p, t);
    case EK_N_P_PAREN:
        return paren_irrefutable(c, p, t);
    case EK_N_P_BRACE:
        return brace_irrefutable(c, p, t);
    default:
        return 0; /* literal, list, or-pattern */
    }
}
