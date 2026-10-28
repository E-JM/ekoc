#include <string.h>

#include "check/internal.h"

/* Coverage of one scrutinee type, judged by constructors at every
   depth: an algebraic scrutinee must have every constructor covered, a
   tuple every component, a record one arm whose named fields are
   covered, a list both the empty and the non-empty lists. A scrutinee
   with no visible case structure is covered only by a catch-all; an
   inference variable suspends the judgment. The multi-scrutinee
   product is in exhaust_prod.c. */

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

static int covers_list(ck_ctx *c, const ek_type *t, const ek_node *const *pats,
                       size_t n)
{
    int empty = 0, nonempty = 0;
    size_t i, k;

    for (i = 0; i < n; i++) {
        const ek_node *flat[32];
        size_t nf = ck_pat_flatten(c, pats[i], flat, 0, 32);

        for (k = 0; k < nf && k < 32; k++) {
            const ek_node *p = flat[k];

            if (pat_catchall(p)) {
                empty = nonempty = 1;
            } else if (p->kind == EK_N_P_BRACE) {
                if (p->u.p_brace.rest == EK_REST_NONE
                    && p->u.p_brace.items.count == 0) {
                    empty = 1; /* the empty list */
                } else if (p->u.p_brace.rest == EK_REST_SUFFIX) {
                    nonempty = 1; /* every non-empty list */
                } else if (p->u.p_brace.rest == EK_REST_PREFIX) {
                    size_t last = p->u.p_brace.items.count;

                    if (last) {
                        const ek_node *one =
                            p->u.p_brace.items.data[last - 1];

                        if (ck_covers(c, t->u.list.elem, &one, 1)) {
                            nonempty = 1;
                        }
                    }
                }
            }
        }
    }
    return empty && nonempty;
}

static int covers_tuple(ck_ctx *c, const ek_type *t,
                        const ek_node *const *pats, size_t n)
{
    size_t j, k;
    int all = 1;

    for (j = 0; j < t->u.tuple.count && all; j++) {
        const ek_node *comps[32];
        size_t nc = 0;

        for (k = 0; k < n; k++) {
            const ek_node *p = pats[k];

            if (pat_catchall(p)) {
                continue; /* covers this component entirely */
            }
            if (p->kind == EK_N_P_BRACE && p->u.p_brace.rest == EK_REST_NONE
                && j < p->u.p_brace.items.count && nc < 32) {
                comps[nc++] = p->u.p_brace.items.data[j];
            }
        }
        if (nc) {
            all = ck_covers(c, t->u.tuple.items[j], comps, nc);
        }
    }
    return all;
}

static int covers_record(ck_ctx *c, const ek_type *t,
                         const ek_node *const *pats, size_t n)
{
    size_t i, k, j;

    for (i = 0; i < n; i++) {
        const ek_node *p = pats[i];
        int arm_ok = 1;

        if (pat_catchall(p)) {
            return 1;
        }
        if (p->kind != EK_N_P_BRACE) {
            continue;
        }
        for (k = 0; k < p->u.p_brace.items.count && arm_ok; k++) {
            const ek_node *item = p->u.p_brace.items.data[k];
            const ek_node *sub = NULL;
            const char *field = NULL;
            const ek_type *ft = NULL;

            if (item->kind == EK_N_P_FIELD) {
                field = item->u.p_field.name;
                sub = item->u.p_field.pattern;
            } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
                field = item->u.path.data[0];
                sub = item;
            }
            for (j = 0; j < t->u.record.count; j++) {
                if (field && strcmp(t->u.record.fields[j].name, field) == 0) {
                    ft = t->u.record.fields[j].type;
                }
            }
            if (!ft || !ck_covers(c, ft, &sub, 1)) {
                arm_ok = 0;
            }
        }
        if (arm_ok) {
            return 1; /* a record pattern alone can cover */
        }
    }
    return 0;
}

/* One constructor of the scrutinee ADT: the arms headed by it must
   jointly cover its instantiated payload. */
int ck_covers_ctor(ck_ctx *c, const ck_ctor *ct, const ek_type *scrut,
                   const ek_node *const *rows, size_t nr)
{
    const ek_node *payloads[32];
    size_t np = 0, i;
    const ck_decl *d = ck_decl_of(c, ct->owner);
    const ek_type **repl = NULL;
    const ek_type *pt;
    size_t nb = d ? d->nbinders : 0;

    for (i = 0; i < nr && np < 32; i++) {
        const ek_node *flat[32];
        size_t nf = ck_pat_flatten(c, rows[i], flat, 0, 32), k;

        for (k = 0; k < nf && k < 32 && np < 32; k++) {
            const ek_node *p = flat[k];

            if (pat_catchall(p)) {
                payloads[np++] = NULL; /* a variable covers the payload */
            } else if (p->kind == EK_N_P_CONS
                       && ck_pat_head(c, p) == ct->sym
                       && (p->u.cons.payload || !ct->payload)) {
                payloads[np++] = p->u.cons.payload;
            }
        }
    }
    if (np == 0) {
        return 0; /* the constructor is never matched */
    }
    if (!ct->payload) {
        return 1;
    }
    if (nb) {
        repl = ek_arena_alloc(c->arena, nb * sizeof *repl);
        if (!repl) {
            c->oom = 1;
            return 1;
        }
        for (i = 0; i < nb; i++) {
            repl[i] = scrut->u.nominal.args[i];
        }
    }
    pt = ck_apply_binders(c, ct->payload, d->binders, repl, nb);
    /* The arms headed by the constructor cover its payload jointly:
       one arm may cover part of a nested ADT and another the rest. */
    return ck_covers(c, pt, payloads, np);
}

static int covers_adt(ck_ctx *c, const ck_decl *d, const ek_type *scrut,
                      const ek_node *const *pats, size_t n)
{
    size_t i;
    int all = 1;

    for (i = 0; i < d->nctors && all; i++) {
        const ck_ctor *ct = d->ctors[i];
        const ek_node *rows[32];
        size_t nr = 0, k;

        for (k = 0; k < n; k++) {
            const ek_node *flat[32];
            size_t nf = ck_pat_flatten(c, pats[k], flat, 0, 32), q;
            int hit = 0;

            for (q = 0; q < nf && q < 32; q++) {
                if (!pat_catchall(flat[q])
                    && ck_pat_head(c, flat[q]) == ct->sym) {
                    hit = 1;
                }
            }
            if (hit && nr < 32) {
                rows[nr++] = pats[k];
            }
        }
        all = nr ? ck_covers_ctor(c, ct, scrut, rows, nr) : 0;
    }
    return all;
}

/* Does one of pats cover every value of t? A scrutinee whose case
   structure is invisible — the std scalars and every type without a
   declaration here — is covered only by a variable or `_`; a literal
   names one value of many. An inference variable suspends the
   judgment: the bridge owes the type, not the coverage. */
int ck_covers(ck_ctx *c, const ek_type *t, const ek_node *const *pats,
              size_t n)
{
    const ek_type *s = ck_prune(t);
    size_t i;

    for (i = 0; i < n; i++) {
        if (pat_catchall(pats[i])) {
            return 1;
        }
    }
    if (!s || s->kind == EK_T_IVAR || s->kind == EK_T_TVAR) {
        return 1;
    }
    if (!n) {
        return 0;
    }
    if (s->kind == EK_T_LIST) {
        return covers_list(c, s, pats, n);
    }
    if (s->kind == EK_T_TUPLE) {
        return covers_tuple(c, s, pats, n);
    }
    if (s->kind == EK_T_RECORD) {
        return covers_record(c, s, pats, n);
    }
    if (s->kind == EK_T_NOMINAL && s->u.nominal.sym) {
        const ck_decl *d = ck_decl_of(c, s->u.nominal.sym);

        if (d && d->rhs == EK_TYPE_RHS_VARIANTS && d->nctors) {
            return covers_adt(c, d, s, pats, n);
        }
        if (d
            && (d->rhs == EK_TYPE_RHS_RECORD || d->rhs == EK_TYPE_RHS_TUPLE)) {
            const ek_type *repr = ck_decl_repr(c, s);

            if (repr) {
                return ck_covers(c, repr, pats, n);
            }
        }
    }
    return 0;
}
