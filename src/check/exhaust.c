#include <string.h>

#include "check/internal.h"

/* Pattern-shape utilities shared with the exhaustiveness walk: the
   catch-all judgment, the flattening of or-patterns, and the head a
   constructor pattern selects. The walk itself, one column per
   scrutinee, is in exhaust_prod.c. */

static int pat_catchall(const ek_node *p)
{
    size_t i;

    if (!p) {
        return 1;
    }
    switch (p->kind) {
    case EK_N_P_WILDCARD:
    case EK_N_P_VAR:
        return 1;
    case EK_N_P_PAREN:
        for (i = 0; i < p->u.kids.count; i++) {
            if (!pat_catchall(p->u.kids.data[i])) {
                return 0;
            }
        }
        return 1;
    case EK_N_P_FIELD:
        return pat_catchall(p->u.p_field.pattern);
    case EK_N_P_CONS:
        /* A constructor pattern matches only its constructor, payload
           included or not. */
        return 0;
    default:
        return 0;
    }
}

/* Every alternative a pattern contributes at one depth: or-patterns
   flatten, one-kid groups unwrap. */
size_t ck_pat_flatten(ck_ctx *c, const ek_node *p, const ek_node **out,
                      size_t n, size_t cap)
{
    size_t i;

    if (!p || n >= cap) {
        return n;
    }
    if (p->kind == EK_N_P_OR) {
        for (i = 0; i < p->u.kids.count; i++) {
            n = ck_pat_flatten(c, p->u.kids.data[i], out, n, cap);
        }
        return n;
    }
    if (p->kind == EK_N_P_PAREN && p->u.kids.count == 1) {
        return ck_pat_flatten(c, p->u.kids.data[0], out, n, cap);
    }
    out[n++] = p;
    return n;
}

/* The constructor symbol a pattern row heads; NULL for other rows.
   The selected candidate is the one the scrutinee type anchored, so a
   case over one ADT is not confused by a same-named constructor of
   another. */
const ek_symbol *ck_pat_head(ck_ctx *c, const ek_node *p)
{
    const ek_ref *ref = NULL;
    const ek_symbol *sel;

    if (!p || p->kind != EK_N_P_CONS) {
        return NULL;
    }
    sel = ck_map_get(&c->pats, p);
    if (sel) {
        return sel;
    }
    if (ck_ref_at(c, p, EK_SYM_CONSTRUCTOR, &ref) && ref->symbol) {
        return ref->symbol;
    }
    return NULL;
}

int ck_pat_catchall(const ek_node *p)
{
    return pat_catchall(p);
}

