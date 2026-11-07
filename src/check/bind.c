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
    if (m->u.bind.type) {
        gen = ck_mono(c, NULL);
        bound = annotated_binding(c, m, val, gen, top);
    } else {
        const ek_node *lam = as_lambda(val);

        if (lam) {
            /* `:=` does not generalize, and a lambda is check-only. */
            ck_error(c, lam, "E-lambda-check-only",
                     "`:=` does not generalize and a lambda is "
                     "check-only: no expected type supplies the "
                     "parameter types (Clauses 6.3.3, 7.6.3)");
        }
        bound = ck_expr(c, val, NULL);
    }
    if (!ck_irrefutable(c, pat, bound)) {
        ck_error(c, pat, "E-refutable-binding",
                 "a value binding admits only irrefutable patterns "
                 "(Clause 7.7.1)");
    }
    if (!top) {
        bind_pattern(c, pat, bound, gen);
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
