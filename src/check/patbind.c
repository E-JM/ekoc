#include <string.h>

#include "check/internal.h"

/* Or-patterns and the binding-shape judgments. Every alternative
   binds the same names at the same types; the first alternative's
   bindings stand. A value binding admits only irrefutable patterns. */

typedef struct {
    const char **names;
    const ek_type **types;
    size_t n, cap;
} ck_binds;

static void binds_put(ck_ctx *c, ck_binds *b, const char *name,
                      const ek_type *t)
{
    if (c->oom) {
        return;
    }
    if (b->n == b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 8;
        const char **nn = ek_arena_alloc(c->arena, ncap * sizeof *nn);
        const ek_type **nt = ek_arena_alloc(c->arena, ncap * sizeof *nt);

        if (!nn || !nt) {
            c->oom = 1;
            return;
        }
        if (b->n) {
            memcpy(nn, b->names, b->n * sizeof *nn);
            memcpy(nt, b->types, b->n * sizeof *nt);
        }
        b->names = nn;
        b->types = nt;
        b->cap = ncap;
    }
    b->names[b->n] = name;
    b->types[b->n] = t;
    b->n++;
}

static void binds_of_scope(ck_ctx *c, const ck_scope *s, ck_binds *b)
{
    size_t i;

    (void)c;
    for (i = 0; i < s->count; i++) {
        binds_put(c, b, s->names[i], s->vals[i] ? s->vals[i]->type : NULL);
    }
}

static void compare_binds(ck_ctx *c, const ek_node *p, const ck_binds *first,
                          const ck_binds *more)
{
    size_t i, k;

    if (first->n != more->n) {
        ck_error(c, p, "E-type-mismatch",
                 "the or-alternatives bind different name sets "
                 "(Clauses 6.8.3, 7.7.1)");
        return;
    }
    for (i = 0; i < more->n; i++) {
        int found = 0;

        for (k = 0; k < first->n; k++) {
            if (strcmp(first->names[k], more->names[i]) == 0) {
                found = 1;
                if (!ck_unify(c, first->types[k], more->types[i], 0)) {
                    ck_error(c, p, "E-type-mismatch",
                             "`%s` is bound at different types in the "
                             "or-alternatives (Clause 6.8.3)",
                             more->names[i]);
                }
            }
        }
        if (!found) {
            ck_error(c, p, "E-type-mismatch",
                     "the or-alternatives bind different name sets "
                     "(Clauses 6.8.3, 7.7.1)");
        }
    }
}

const ek_type *ck_or_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut)
{
    ck_binds first;
    size_t i;

    memset(&first, 0, sizeof first);
    for (i = 0; i < p->u.kids.count; i++) {
        ck_binds b;

        ck_scope_push(c);
        ck_pattern(c, p->u.kids.data[i], scrut);
        memset(&b, 0, sizeof b);
        binds_of_scope(c, c->locals, &b);
        ck_scope_pop(c);
        if (c->oom) {
            return scrut;
        }
        if (i == 0) {
            first = b;
        } else {
            compare_binds(c, p, &first, &b);
        }
    }
    /* The first alternative's bindings stand (same names, same
       types). */
    for (i = 0; i < first.n; i++) {
        ck_scope_bind(c, first.names[i], first.types[i]);
    }
    return scrut;
}

/* One arm's patterns bind together: a name that two of the arm's
   patterns bind is a duplicate, judged across the patterns with each
   pattern's binding set taken whole. Duplicates inside one pattern are
   judged in the resolver; different arms bind independently. */

typedef struct {
    const char *name;
    const ek_node *at;
} ck_bound;

typedef struct {
    ck_bound *v;
    size_t n, cap;
} ck_seen;

static int seen_has(const ck_seen *s, const char *name)
{
    size_t i;

    for (i = 0; i < s->n; i++) {
        if (strcmp(s->v[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* A binding name joins the set; `_` binds nothing and never
   conflicts. */
static void seen_put(ck_ctx *c, ck_seen *s, const char *name,
                     const ek_node *at)
{
    if (c->oom || !name || strcmp(name, "_") == 0) {
        return;
    }
    if (s->n == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 8;
        ck_bound *nv = ek_arena_alloc(c->arena, ncap * sizeof *nv);

        if (!nv) {
            c->oom = 1;
            return;
        }
        if (s->n) {
            memcpy(nv, s->v, s->n * sizeof *nv);
        }
        s->v = nv;
        s->cap = ncap;
    }
    s->v[s->n].name = name;
    s->v[s->n].at = at;
    s->n++;
}

/* The binding names one pattern carries, with or-alternatives
   contributing their (necessarily identical) sets. */
static void collect_bound(ck_ctx *c, ck_seen *s, const ek_node *p)
{
    size_t i;

    if (!p || c->oom) {
        return;
    }
    switch (p->kind) {
    case EK_N_P_VAR:
        if (p->u.path.count == 1) {
            seen_put(c, s, p->u.path.data[0], p);
        }
        return;
    case EK_N_P_OR:
    case EK_N_P_PAREN:
        for (i = 0; i < p->u.kids.count; i++) {
            collect_bound(c, s, p->u.kids.data[i]);
        }
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < p->u.p_brace.items.count; i++) {
            collect_bound(c, s, p->u.p_brace.items.data[i]);
        }
        seen_put(c, s, p->u.p_brace.rest_name, p);
        return;
    case EK_N_P_FIELD:
        collect_bound(c, s, p->u.p_field.pattern);
        return;
    case EK_N_P_CONS:
        collect_bound(c, s, p->u.cons.payload);
        return;
    default:
        return;
    }
}

/* Run before the arm's patterns bind: each pattern is compared against
   the union of the earlier ones, and a shared name is reported at that
   occurrence. A name repeated inside one pattern counts once here; the
   resolver owns within-pattern duplicates. */
void ck_arm_binds_together(ck_ctx *c, const ek_node *arm)
{
    ck_seen seen;
    size_t k;

    if (c->oom || !arm || arm->u.arm.patterns.count < 2) {
        return;
    }
    memset(&seen, 0, sizeof seen);
    for (k = 0; k < arm->u.arm.patterns.count; k++) {
        ck_seen now;
        size_t i;

        memset(&now, 0, sizeof now);
        collect_bound(c, &now, arm->u.arm.patterns.data[k]);
        for (i = 0; i < now.n; i++) {
            size_t j;
            int earlier = 0;

            for (j = 0; j < i; j++) {
                if (strcmp(now.v[j].name, now.v[i].name) == 0) {
                    earlier = 1;
                }
            }
            if (earlier) {
                continue;
            }
            if (seen_has(&seen, now.v[i].name)) {
                ck_error(c, now.v[i].at, "E-name-conflict",
                         "`%s` is bound by two patterns of the arm "
                         "(Clauses 6.8.6, 7.4.6)", now.v[i].name);
            } else {
                seen_put(c, &seen, now.v[i].name, now.v[i].at);
            }
        }
    }
}

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

int ck_irrefutable(ck_ctx *c, const ek_node *p, const ek_type *t)
{
    size_t i;

    if (!p) {
        return 1;
    }
    switch (p->kind) {
    case EK_N_P_WILDCARD:
    case EK_N_P_VAR:
        return 1;
    case EK_N_P_CONS:
        return cons_irrefutable(c, p, t);
    case EK_N_P_PAREN: {
        const ek_type *repr = brace_repr_of(c, t);

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
    case EK_N_P_BRACE: {
        const ek_type *repr;

        if (p->u.p_brace.rest != EK_REST_NONE) {
            return 0; /* a list pattern */
        }
        repr = brace_repr_of(c, t);
        if (!repr || (repr->kind != EK_T_RECORD && repr->kind != EK_T_TUPLE)) {
            return 0; /* a list, an ADT, an opaque, or a scalar family */
        }
        for (i = 0; i < p->u.p_brace.items.count; i++) {
            const ek_node *item = p->u.p_brace.items.data[i];
            const ek_type *ft = NULL;

            if (repr->kind == EK_T_RECORD) {
                const char *field = item->kind == EK_N_P_FIELD
                                        ? item->u.p_field.name
                                        : item->kind == EK_N_P_VAR
                                              && item->u.path.count == 1
                                              ? item->u.path.data[0] : NULL;
                size_t k;

                for (k = 0; field && k < repr->u.record.count; k++) {
                    if (strcmp(repr->u.record.fields[k].name, field) == 0) {
                        ft = repr->u.record.fields[k].type;
                    }
                }
                if (field && !ft) {
                    return 1; /* the pattern walk names the field */
                }
            } else if (i < repr->u.tuple.count) {
                ft = repr->u.tuple.items[i];
            }
            if (item->kind == EK_N_P_FIELD) {
                if (!ck_irrefutable(c, item->u.p_field.pattern, ft)) {
                    return 0;
                }
            } else if (!ck_irrefutable(c, item, ft)) {
                return 0;
            }
        }
        return 1;
    }
    default:
        return 0; /* literal, list, or-pattern */
    }
}

/* The admitted `<-` patterns are the parameter-position forms. */
int ck_invert_pattern(const ek_node *pat)
{
    size_t i;

    if (!pat) {
        return 1;
    }
    switch (pat->kind) {
    case EK_N_P_VAR:
    case EK_N_P_WILDCARD:
        return 1;
    case EK_N_P_PAREN:
        for (i = 0; i < pat->u.kids.count; i++) {
            if (!ck_invert_pattern(pat->u.kids.data[i])) {
                return 0;
            }
        }
        return 1;
    default:
        return 0;
    }
}
