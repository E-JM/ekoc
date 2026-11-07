#include <string.h>

#include "check/internal.h"

/* Composite literals: the expected type decides the family, as it
   decides a brace pattern. A literal under an expected type the bridge
   leaves open takes the opaque reading: values are checked, fields and
   keys are not. The record family lives in recordlit.c. */

/* The family of a composite literal's items: 1 when the items are
   colon pairs. */
static int has_colons(const ek_node *e)
{
    size_t i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM && item->u.pair.a) {
            return 1;
        }
    }
    return 0;
}

/* The opaque placements: no expected type, or one the bridge leaves
   undetermined. Values are still checked; a bare name key
   stays unread (record field or dictionary key is not decidable under
   opacity, and resolve resolved no such key). A literal placed under
   an undetermined expectation is remembered: unification may still
   anchor it, and the sweep after substitution rejects the ones that
   stayed family-less. */
static const ek_type *composite_opaque(ck_ctx *c, const ek_node *e,
                                       const ek_type *exp)
{
    size_t i;
    int saved = c->opaque_arg;

    /* Opacity is contagious: an item of a family-less literal is as
       unplaceable as the literal itself, and the sweep reports the
       outermost point. */
    c->opaque_arg = 1;
    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM) {
            if (item->u.pair.a && item->u.pair.a->kind != EK_N_NAME) {
                ck_expr(c, item->u.pair.a, ck_opaque(c));
            }
            ck_expr(c, item->u.pair.b, ck_opaque(c));
        }
    }
    c->opaque_arg = saved;
    if (!exp) {
        ck_error(c, e, "E-literal-expected",
                 "a composite literal never stands alone; it requires "
                 "an expected type or an annotation (Clause 7.5.3)");
        return ck_opaque(c);
    }
    if ((exp->kind == EK_T_IVAR || exp->kind == EK_T_TVAR)
        && !c->opaque_arg) {
        ck_pending_literal(c, e);
    }
    return exp;
}

static const ek_type *comma_tuple(ck_ctx *c, const ek_node *e,
                                  const ek_type *exp);

/* A nominal with no representation to read: a bridged name that
   carries no symbol, or a declaration error recovery left without
   variants. Its literals and patterns stay opaque. */
static int opaque_nominal(ck_ctx *c, const ek_type *t)
{
    const ck_decl *d;

    if (t->kind != EK_T_NOMINAL) {
        return 0;
    }
    if (!t->u.nominal.sym) {
        return 1;
    }
    if (!t->u.nominal.sym->decl) {
        return 0; /* a std placeholder still has a known type name */
    }
    d = ck_decl_of(c, t->u.nominal.sym);
    return d && d->rhs == EK_TYPE_RHS_VARIANTS && d->nctors == 0;
}

/* The empty brace literal: its family comes from the expected type.
   A record still has to name every field, a tuple its components. */
static const ek_type *empty_literal(ck_ctx *c, const ek_node *e,
                                    const ek_type *exp, int allow_partial)
{
    if (exp->kind == EK_T_LIST || exp->kind == EK_T_DICT) {
        if (exp->kind == EK_T_DICT) {
            ck_key_domain_error(c, e, exp->u.dict.key);
        }
        return exp;
    }
    if (exp->kind == EK_T_RECORD) {
        ck_record_lit(c, e, exp, allow_partial);
        return exp;
    }
    if (exp->kind == EK_T_TUPLE) {
        return comma_tuple(c, e, exp);
    }
    if (exp->kind == EK_T_NOMINAL) {
        const ek_type *repr = ck_decl_repr(c, exp);

        if (repr && repr->kind == EK_T_RECORD) {
            ck_record_lit(c, e, repr, allow_partial);
            return exp;
        }
        if (repr && repr->kind == EK_T_TUPLE) {
            ck_composite(c, e, repr, 0);
            return exp;
        }
        if (opaque_nominal(c, exp)) {
            return exp;
        }
    }
    ck_error(c, e, "E-type-mismatch",
             "a comma literal expects a list or a tuple, not %s "
             "(Clause 7.5.3)", ck_type_name(c, exp));
    return exp;
}

/* The colon family: a record (plain or shaped), or a dictionary. */
static const ek_type *composite_colons(ck_ctx *c, const ek_node *e,
                                       const ek_type *exp, int allow_partial)
{
    const ek_type *repr;
    size_t i;

    if (exp->kind == EK_T_RECORD) {
        return ck_record_lit(c, e, exp, allow_partial);
    }
    if (exp->kind == EK_T_NOMINAL) {
        if (opaque_nominal(c, exp)) {
            return composite_opaque(c, e, exp);
        }
        repr = ck_decl_repr(c, exp);

        if (repr && repr->kind == EK_T_RECORD) {
            ck_record_lit(c, e, repr, allow_partial);
            return exp; /* the literal lands at the nominal type */
        }
    }
    if (exp->kind == EK_T_DICT) {
        ck_key_domain_error(c, e, exp->u.dict.key);
        for (i = 0; i < e->u.kids.count; i++) {
            const ek_node *item = e->u.kids.data[i];

            if (item->kind == EK_N_COMPOSITE_ITEM) {
                ck_item_value(c, item->u.pair.a, exp->u.dict.key, 0);
                ck_item_value(c, item->u.pair.b, exp->u.dict.val, 0);
            }
        }
        return exp;
    }
    ck_error(c, e, "E-type-mismatch",
             "a colon literal expects a record or a dictionary, not "
             "%s (Clause 7.5.3)", ck_type_name(c, exp));
    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM) {
            ck_expr(c, item->u.pair.b, NULL);
        }
    }
    return exp;
}

/* A tuple literal against its expected arity. */
static const ek_type *comma_tuple(ck_ctx *c, const ek_node *e,
                                  const ek_type *exp)
{
    size_t n = 0, i;

    for (i = 0; i < e->u.kids.count; i++) {
        if (e->u.kids.data[i]->kind == EK_N_COMPOSITE_ITEM) {
            n++;
        }
    }
    if (n != exp->u.tuple.count) {
        ck_error(c, e, "E-type-mismatch",
                 "the tuple literal has %zu components, the expected "
                 "type has %zu (Clause 7.5.3)", n, exp->u.tuple.count);
    }
    for (i = 0; i < e->u.kids.count && i < exp->u.tuple.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM) {
            ck_item_value(c, item->u.pair.b, exp->u.tuple.items[i], 0);
        }
    }
    return exp;
}

/* The comma family: a list, a tuple, or a tuple-shaped instantiation. */
static const ek_type *composite_comma(ck_ctx *c, const ek_node *e,
                                      const ek_type *exp)
{
    size_t i;

    switch (exp->kind) {
    case EK_T_LIST:
        for (i = 0; i < e->u.kids.count; i++) {
            const ek_node *item = e->u.kids.data[i];

            if (item->kind == EK_N_COMPOSITE_ITEM) {
                ck_item_value(c, item->u.pair.b, exp->u.list.elem, 0);
            }
        }
        return exp;
    case EK_T_TUPLE:
        return comma_tuple(c, e, exp);
    case EK_T_NOMINAL: {
        /* A tuple shape extends to a generic instantiation by the
           shape of its right-hand side. */
        const ek_type *repr;

        if (opaque_nominal(c, exp)) {
            return composite_opaque(c, e, exp);
        }
        repr = ck_decl_repr(c, exp);
        if (repr && repr->kind == EK_T_TUPLE) {
            ck_composite(c, e, repr, 0);
            return exp; /* the literal lands at the instantiation */
        }
        ck_error(c, e, "E-type-mismatch",
                 "a comma literal expects a list or a tuple, not %s "
                 "(Clause 7.5.3)", ck_type_name(c, exp));
        break;
    }
    default:
        ck_error(c, e, "E-type-mismatch",
                 "a comma literal expects a list or a tuple, not %s "
                 "(Clause 7.5.3)", ck_type_name(c, exp));
        break;
    }
    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM) {
            ck_expr(c, item->u.pair.b, NULL);
        }
    }
    return exp;
}

/* Remembers a literal placed under an undetermined expectation; the
   final sweep rejects it when substitution left it family-less. */
void ck_pending_literal(ck_ctx *c, const ek_node *e)
{
    ck_pend_lit *grown = ck_grow(c, c->pend_lits, c->npend + 1, &c->cpend,
                                 sizeof *c->pend_lits);

    if (c->oom) {
        return;
    }
    c->pend_lits = grown;
    c->pend_lits[c->npend].node = e;
    c->pend_lits[c->npend].mod = c->cur;
    c->pend_lits[c->npend].file = ck_file(c);
    c->npend++;
}

const ek_type *ck_composite(ck_ctx *c, const ek_node *e,
                            const ek_type *expected, int allow_partial)
{
    const ek_type *exp = ck_prune(expected);
    int colons = has_colons(e);
    int partial = allow_partial || c->partial_ok;

    if (c->oom) {
        return NULL;
    }
    if (!exp || exp->kind == EK_T_IVAR || exp->kind == EK_T_TVAR
        || (exp->kind == EK_T_NOMINAL && !exp->u.nominal.sym)) {
        return composite_opaque(c, e, exp);
    }
    if (e->u.kids.count == 0) {
        return empty_literal(c, e, exp, partial);
    }
    if (colons) {
        return composite_colons(c, e, exp, partial);
    }
    return composite_comma(c, e, exp);
}
