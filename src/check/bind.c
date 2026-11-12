#include <string.h>

#include "check/internal.h"

/* The binding forms as check sees them: annotated and plain value
   bindings, rebinds, and inverted bindings, dispatched by ck_stmt.
   The annotated bindings and the declared members live in
   declcheck.c; type declarations are built in decl.c, entry rules in
   entry.c. */

ck_scheme *ck_mono(ck_ctx *c, const ek_type *t)
{
    ck_scheme *s = ek_arena_alloc(c->arena, sizeof *s);

    if (!s) {
        c->oom = 1;
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->type = t;
    return s;
}

/* The lambda beneath one level of parentheses, else NULL: a lambda is
   check-only, so an unannotated binding over one is rejected. */
static const ek_node *as_lambda(const ek_node *v)
{
    while (v && v->kind == EK_N_PAREN) {
        v = v->u.wrap.inner;
    }
    return v && v->kind == EK_N_LAMBDA ? v : NULL;
}

/* The surface symbol one top-level pattern name binds: the FUNC row
   the surface pass registered for this binding. */
static const ek_symbol *top_sym_of(ck_ctx *c, const ek_node *m,
                                   const char *name)
{
    size_t i;

    for (i = 0; i < c->cur->count; i++) {
        const ek_symbol *sym = &c->cur->syms[i];

        if (sym->kind == EK_SYM_FUNC && sym->decl == m
            && strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

/* One top-level name takes the scheme its component read. */
static void top_scheme(ck_ctx *c, const ek_node *m, const char *name,
                       const ek_type *t)
{
    const ek_symbol *sym;
    ck_scheme *s;

    if (!t || c->oom) {
        return;
    }
    sym = top_sym_of(c, m, name);
    if (!sym) {
        return;
    }
    s = ek_arena_alloc(c->arena, sizeof *s);
    if (!s) {
        c->oom = 1;
        return;
    }
    memset(s, 0, sizeof *s);
    *s = ck_generalize(c, t);
    ck_map_put(c, &c->schemes, sym, s);
}

/* The names one pattern binds, walked for the surface install. */
static void top_names(ck_ctx *c, const ek_node *pat, const ek_node *m)
{
    size_t i;

    if (!pat || c->oom) {
        return;
    }
    switch (pat->kind) {
    case EK_N_P_VAR:
        if (pat->u.path.count == 1
            && strcmp(pat->u.path.data[0], "_") != 0) {
            top_scheme(c, m, pat->u.path.data[0],
                       ck_scope_get(c, pat->u.path.data[0]));
        }
        return;
    case EK_N_P_PAREN:
        for (i = 0; i < pat->u.kids.count; i++) {
            top_names(c, pat->u.kids.data[i], m);
        }
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            const ek_node *item = pat->u.p_brace.items.data[i];

            if (item->kind == EK_N_P_FIELD) {
                top_names(c, item->u.p_field.pattern, m);
            } else {
                top_names(c, item, m);
            }
        }
        return;
    case EK_N_P_CONS:
        top_names(c, pat->u.cons.payload, m);
        return;
    default:
        return;
    }
}

/* A top-level pattern binding: the names are the module surface's own
   rows. A single name takes the bound type; a group names its
   components; the structured families walk in a temporary scope whose
   reads the install harvests. */
static void bind_top_pattern(ck_ctx *c, const ek_node *pat,
                             const ek_node *m, const ek_type *bound)
{
    const ek_type *p;
    size_t i;

    if (!pat || c->oom) {
        return;
    }
    if (pat->kind == EK_N_P_WILDCARD) {
        return;
    }
    if (pat->kind == EK_N_P_VAR) {
        if (pat->u.path.count == 1
            && strcmp(pat->u.path.data[0], "_") != 0) {
            top_scheme(c, m, pat->u.path.data[0], bound);
        }
        return;
    }
    if (pat->kind == EK_N_P_PAREN && pat->u.kids.count == 1) {
        bind_top_pattern(c, pat->u.kids.data[0], m, bound);
        return;
    }
    if (pat->kind == EK_N_P_PAREN) {
        p = ck_prune(bound);
        for (i = 0; i < pat->u.kids.count; i++) {
            const ek_type *ct = p && p->kind == EK_T_TUPLE
                                    && i < p->u.tuple.count
                                ? p->u.tuple.items[i] : ck_opaque(c);

            bind_top_pattern(c, pat->u.kids.data[i], m, ct);
        }
        return;
    }
    ck_scope_push(c);
    ck_pattern(c, pat, bound);
    top_names(c, pat, m);
    ck_scope_pop(c);
}

/* Binds an irrefutable pattern's names. */
static void bind_pattern(ck_ctx *c, const ek_node *pat, const ek_type *t,
                         ck_scheme *gen)
{
    size_t i;

    if (!pat || c->oom) {
        return;
    }
    switch (pat->kind) {
    case EK_N_P_VAR:
        if (pat->u.path.count == 1 && strcmp(pat->u.path.data[0], "_") != 0) {
            ck_scope_bind_scheme(c, pat->u.path.data[0],
                                 gen ? gen : ck_mono(c, t));
        }
        return;
    case EK_N_P_PAREN: {
        const ek_type *p = ck_prune(t);

        if (pat->u.kids.count == 1) {
            bind_pattern(c, pat->u.kids.data[0], t, gen);
            return;
        }
        for (i = 0; i < pat->u.kids.count; i++) {
            const ek_type *ct = p && p->kind == EK_T_TUPLE
                                    && i < p->u.tuple.count
                                ? p->u.tuple.items[i] : ck_opaque(c);

            bind_pattern(c, pat->u.kids.data[i], ct, NULL);
        }
        return;
    }
    case EK_N_P_BRACE:
    case EK_N_P_CONS:
        /* The structured families type through the pattern walk, so
           the binding's rows carry the family the lowerer reads; a
           refutable shape already took its error above. */
        if (ck_irrefutable(c, pat, t)) {
            ck_pattern(c, pat, t);
        }
        return;
    default:
        return; /* refutable forms never reach a value binding */
    }
}

/* One binding member: LET, REBIND, or INVERT. Returns the type the
   binding introduces. */
/* The type an annotated or plain binding's value reads: the annotation's
   elaboration, or the right-hand side's own type with the top-level
   purity rule applied. */
static const ek_type *binding_bound(ck_ctx *c, const ek_node *m,
                                    const ek_node *val, ck_scheme **gen,
                                    int top)
{
    const ek_type *bound;

    if (m->u.bind.type) {
        *gen = ck_mono(c, NULL);
        return annotated_binding(c, m, val, *gen, top);
    }
    {
        const ek_node *lam = as_lambda(val);

        if (lam) {
            /* `:=` does not generalize, and a lambda is check-only. */
            ck_error(c, lam, "E-lambda-check-only",
                     "`:=` does not generalize and a lambda is "
                     "check-only: no expected type supplies the "
                     "parameter types (Clauses 6.3.3, 7.6.3)");
        }
    }
    bound = ck_expr(c, val, NULL);
    /* Top-level initialization is pure whatever the binding form; an
       extern call needs no capability, so the rebind is checked, not
       ruled out by scope. A body with a reported site claims no bit
       and owes no judgment here. */
    if (top && val && !ck_body_silent(c, val)
        && ck_body_bit(c, val) != EK_BIT_PURE) {
        ck_error(c, m, "E-world-bit",
                 "top-level initialization is pure; a bit-setting "
                 "call belongs inside a world-touching function "
                 "(Clauses 7.1.2, 7.9.2)");
    }
    return bound;
}

static const ek_type *ck_binding(ck_ctx *c, const ek_node *m, int top)
{
    const ek_node *pat = m->u.bind.pattern;
    const ek_node *val = m->u.bind.value;
    ck_scheme *gen = NULL;
    const ek_type *bound;

    if (m->kind == EK_N_INVERT) {
        return ck_invert_binding(c, m, pat, top);
    }
    if (top) {
        /* A top-level body: the module pass records its generic uses. */
        c->inst_body = 1;
        c->inst_decl = NULL;
        c->inst_vars = NULL;
        c->inst_nvars = 0;
    }
    bound = binding_bound(c, m, val, &gen, top);
    if (!ck_irrefutable(c, pat, bound)) {
        ck_error(c, pat, "E-refutable-binding",
                 "a value binding admits only irrefutable patterns "
                 "(Clause 7.7.1)");
    }
    if (!top) {
        bind_pattern(c, pat, bound, gen);
    } else if (pat && (pat->kind != EK_N_P_VAR || !m->u.bind.type)) {
        /* A top-level pattern names the module surface's own rows, and
           an unannotated name takes the type its right-hand side
           read. */
        bind_top_pattern(c, pat, m, bound);
    }
    if (top) {
        c->inst_body = 0;
        c->inst_decl = NULL;
        c->inst_vars = NULL;
        c->inst_nvars = 0;
    }
    ck_row(c, m, bound, EK_RD_NONE);
    return bound;
}

const ek_type *ck_stmt(ck_ctx *c, const ek_node *m, int top)
{
    if (c->oom || !m) {
        return NULL;
    }
    switch (m->kind) {
    case EK_N_LET:
    case EK_N_REBIND:
    case EK_N_INVERT:
        return ck_binding(c, m, top);
    case EK_N_NAMESPACE:
        return ck_namespace_member(c, m);
    case EK_N_TEST_DECL:
        ck_test_decl(c, m);
        return ck_unit(c);
    case EK_N_EXTERN_DECL:
        return ck_annotated_decl(c, m, m->u.extern_decl.type, NULL, 0);
    default:
        return NULL;
    }
}
