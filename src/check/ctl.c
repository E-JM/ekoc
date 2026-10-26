#include <string.h>

#include "check/internal.h"

/* The control forms: `if` (Clause 6.7.4, 7.7.4) and `case`
   (Clauses 7.7.2-7.7.4). Arm typing unifies the arm bodies, a `Never`
   arm adapts, and exhaustiveness runs on the unguarded arms whose
   bodies are not `Never`. */

const ek_type *ck_if(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_type *bool_t = ck_std_type(c, "Bool");
    const ek_type *unit = ck_unit(c);
    const ek_type *cond = ck_expr(c, e->u.ifexpr.cond, bool_t);
    const ek_type *then;
    const ek_type *ct = ck_prune(cond);

    if (!ct || (ct->kind != EK_T_IVAR && !ck_is_std(ct, "Bool"))) {
        ck_error(c, e->u.ifexpr.cond, "E-type-mismatch",
                 "an `if` condition is `Bool`, not %s (Clauses 6.7.4, "
                 "7.7.4)", ck_type_name(c, cond));
    }
    then = ck_expr(c, e->u.ifexpr.then, expected);
    if (!e->u.ifexpr.els) {
        /* else: _ (Clause 6.7.4): the then-branch is `_` or `Never`. */
        const ek_type *t = ck_prune(then);

        if (t && t->kind != EK_T_IVAR && t->kind != EK_T_NEVER
            && t != unit) {
            ck_error(c, e, "E-type-mismatch",
                     "an `if` without `else` has type %s; the then-branch "
                     "is `_` or `Never` (Clause 6.7.4)",
                     ck_type_name(c, then));
        }
        ck_row(c, e, unit, EK_RD_NONE);
        return unit;
    }
    {
        const ek_type *els = ck_expr(c, e->u.ifexpr.els,
                                     expected ? expected : then);
        const ek_type *t = ck_unify(c, then, els, 1);

        if (!t) {
            ck_error(c, e, "E-type-mismatch",
                     "the branches of `if` are %s and %s (Clause 7.7.4)",
                     ck_type_name(c, then), ck_type_name(c, els));
            t = expected ? expected : then;
        }
        ck_row(c, e, t, EK_RD_NONE);
        return t;
    }
}

/* One arm: its patterns bind against the anchored scrutinee columns,
   its guard is `Bool`, and its body is checked (Clauses 7.7.1,
   7.7.3). */
static const ek_type *case_arm(ck_ctx *c, const ek_node *arm,
                               const ek_type **scrut, size_t nsc,
                               const ek_type *expected)
{
    const ek_type *atype;
    size_t k;

    ck_scope_push(c);
    for (k = 0; k < arm->u.arm.patterns.count; k++) {
        size_t col = k < nsc ? k : (nsc ? nsc - 1 : 0);

        ck_pattern(c, arm->u.arm.patterns.data[k], scrut[col]);
    }
    if (arm->u.arm.guard) {
        const ek_type *g = ck_expr(c, arm->u.arm.guard,
                                   ck_std_type(c, "Bool"));
        const ek_type *gt = ck_prune(g);

        if (!gt || (gt->kind != EK_T_IVAR && !ck_is_std(gt, "Bool"))) {
            ck_error(c, arm->u.arm.guard, "E-type-mismatch",
                     "a guard is `Bool`, not %s (Clause 7.7.3)",
                     ck_type_name(c, g));
        }
    }
    atype = ck_expr(c, arm->u.arm.body, expected);
    ck_scope_pop(c);
    return atype;
}

/* Every arm has the same type; a `Never` arm adapts (7.7.4). */
static const ek_type *case_result(ck_ctx *c, const ek_node *e,
                                  const ek_type *const *atypes, size_t na,
                                  const ek_type *expected)
{
    const ek_type *seen = NULL;
    size_t i;

    for (i = 0; i < na; i++) {
        const ek_type *t = atypes[i];

        if (ck_prune(t) && ck_prune(t)->kind == EK_T_NEVER) {
            continue;
        }
        if (!seen) {
            seen = t;
        } else if (!ck_unify(c, seen, t, 1)) {
            ck_error(c, e, "E-type-mismatch",
                     "the arms of the case are %s and %s (Clause "
                     "7.7.4)", ck_type_name(c, seen),
                     ck_type_name(c, t));
        }
    }
    return expected ? expected : (seen ? seen : ck_never(c));
}

const ek_type *ck_case(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_type **scrut;
    const ek_node **arms;
    int *contrib;
    const ek_type **atypes;
    size_t i, nsc = e->u.caseexpr.scrutinees.count;
    size_t na = e->u.caseexpr.arms.count;
    const ek_type *result;

    scrut = ek_arena_alloc(c->arena, (nsc ? nsc : 1) * sizeof *scrut);
    arms = ek_arena_alloc(c->arena, (na ? na : 1) * sizeof *arms);
    contrib = ek_arena_alloc(c->arena, (na ? na : 1) * sizeof *contrib);
    atypes = ek_arena_alloc(c->arena, (na ? na : 1) * sizeof *atypes);
    if (!scrut || !arms || !contrib || !atypes) {
        c->oom = 1;
        return expected;
    }
    if (!nsc) {
        scrut[0] = ck_ivar(c); /* malformed case; patterns bind loosely */
    }
    for (i = 0; i < nsc; i++) {
        scrut[i] = ck_expr(c, e->u.caseexpr.scrutinees.data[i], NULL);
    }
    for (i = 0; i < na; i++) {
        const ek_node *arm = e->u.caseexpr.arms.data[i];

        arms[i] = arm;
        atypes[i] = case_arm(c, arm, scrut, nsc, expected);
        contrib[i] = ck_prune(atypes[i])
                && ck_prune(atypes[i])->kind != EK_T_NEVER;
    }
    ck_exhaust(c, e, scrut, nsc, arms, contrib, na);
    result = case_result(c, e, atypes, na, expected);
    ck_row(c, e, result, EK_RD_NONE);
    return result;
}
