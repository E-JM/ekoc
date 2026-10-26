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

/* A value binding admits only irrefutable patterns: `_`, a variable,
   a tuple or group of irrefutable patterns, and a record pattern whose
   subpatterns are irrefutable. */
int ck_irrefutable(const ek_node *p)
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
            if (!ck_irrefutable(p->u.kids.data[i])) {
                return 0;
            }
        }
        return 1;
    case EK_N_P_BRACE:
        if (p->u.p_brace.rest != EK_REST_NONE) {
            return 0; /* a list pattern */
        }
        for (i = 0; i < p->u.p_brace.items.count; i++) {
            const ek_node *item = p->u.p_brace.items.data[i];

            if (item->kind == EK_N_P_FIELD
                && !ck_irrefutable(item->u.p_field.pattern)) {
                return 0;
            }
        }
        return 1;
    default:
        return 0; /* constructor, literal, list, or-pattern */
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

/* The callback parameter an inverted binding takes: the last
   parameter of the call's function type. */
const ek_type *ck_callback_param(ck_ctx *c, const ek_node *m,
                                 const ek_type *call)
{
    const ek_type *f = ck_prune(call);

    if (!f) {
        return ck_ivar(c);
    }
    if (f->kind != EK_T_FN) {
        if (f->kind != EK_T_IVAR) {
            ck_error(c, m->u.bind.value, "E-type-mismatch",
                     "the right-hand side of `<-` is not a call with a "
                     "callback parameter (Clause 7.8.1)");
        }
        return ck_ivar(c);
    }
    if (f->u.fn.count == 0) {
        ck_error(c, m->u.bind.value, "E-type-mismatch",
                 "the call has no callback parameter for `<-` "
                 "(Clause 7.8.1)");
        return ck_ivar(c);
    }
    return f->u.fn.params[f->u.fn.count - 1];
}

