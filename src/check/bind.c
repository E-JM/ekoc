#include <string.h>

#include "check/internal.h"

/* The binding forms of Clause 6.3 as check sees them: annotated and
   plain value bindings, rebinds, inverted bindings, namespace
   members, tests, and externs (Clauses 7.6.3, 7.6.5, 7.7.1, 7.8.1).
   Type declarations are built in decl.c, entry rules in entry.c. */

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
   check-only, so an unannotated binding over one is rejected
   (Clause 7.6.3). */
static const ek_node *as_lambda(const ek_node *v)
{
    while (v && v->kind == EK_N_PAREN) {
        v = v->u.wrap.inner;
    }
    return v && v->kind == EK_N_LAMBDA ? v : NULL;
}

/* Clause 7.6.5: every type variable of a function annotation MUST
   occur in at least one parameter type. */
static void check_phantom(ck_ctx *c, const ek_node *ann, const ek_type *t)
{
    const ek_type **all = NULL;
    const ek_type **ps = NULL;
    size_t nall = 0, nps = 0, i, j;

    if (!ann || ann->kind != EK_N_T_FN || !t || t->kind != EK_T_FN) {
        return;
    }
    all = ek_arena_alloc(c->arena, 64 * sizeof *all);
    ps = ek_arena_alloc(c->arena, 64 * sizeof *ps);
    if (!all || !ps) {
        c->oom = 1;
        return;
    }
    ck_tvars_collect(t, &all, &nall);
    for (i = 0; i < t->u.fn.count; i++) {
        ck_tvars_collect(t->u.fn.params[i], &ps, &nps);
    }
    for (i = 0; i < nall; i++) {
        int in_param = 0;
        const ek_node *first;

        for (j = 0; j < nps; j++) {
            if (ps[j] == all[i]) {
                in_param = 1;
            }
        }
        if (in_param) {
            continue;
        }
        first = ck_map_get(&c->firsts, all[i]);
        ck_error(c, first ? first : ann, "E-phantom",
                 "the type variable `%s` occurs only in the result; it "
                 "must occur in a parameter type (Clause 7.6.5)",
                 all[i]->u.tvar.name);
    }
}

/* The record shape behind a binding pattern's scrutinee, when one
   exists. */
static const ek_type *repr_of(ck_ctx *c, const ek_type *t)
{
    t = ck_prune(t);
    if (t && t->kind == EK_T_NOMINAL && t->u.nominal.sym) {
        return ck_decl_repr(c, t);
    }
    return t;
}

/* Binds an irrefutable pattern's names (Clause 7.7.1). */
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
                                ? p->u.tuple.items[i] : ck_ivar(c);

            bind_pattern(c, pat->u.kids.data[i], ct, NULL);
        }
        return;
    }
    case EK_N_P_BRACE: {
        const ek_type *p = repr_of(c, t);

        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            const ek_node *item = pat->u.p_brace.items.data[i];
            const char *field = NULL;
            const ek_type *ft = NULL;
            size_t k;

            if (item->kind == EK_N_P_FIELD) {
                field = item->u.p_field.name;
            } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
                field = item->u.path.data[0];
            }
            if (!field) {
                continue;
            }
            if (p && p->kind == EK_T_RECORD) {
                for (k = 0; k < p->u.record.count; k++) {
                    if (strcmp(p->u.record.fields[k].name, field) == 0) {
                        ft = p->u.record.fields[k].type;
                    }
                }
            }
            if (item->kind == EK_N_P_FIELD) {
                bind_pattern(c, item->u.p_field.pattern,
                             ft ? ft : ck_ivar(c), NULL);
            } else {
                ck_scope_bind(c, field, ft ? ft : ck_ivar(c));
            }
        }
        return;
    }
    default:
        return; /* refutable forms never reach a value binding */
    }
}

/* The admitted `<-` patterns are the parameter-position forms
   (Clauses 7.7.1, 7.8.1). */
static int invert_pattern(const ek_node *pat)
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
            if (!invert_pattern(pat->u.kids.data[i])) {
                return 0;
            }
        }
        return 1;
    default:
        return 0;
    }
}

/* The callback parameter an inverted binding takes: the last
   parameter of the call's function type (Clause 7.8.1). */
static const ek_type *callback_param(ck_ctx *c, const ek_node *m,
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

/* One binding member: LET, REBIND, or INVERT (Clauses 6.3.1-6.3.4).
   Returns the type the binding introduces. */
static const ek_type *ck_binding(ck_ctx *c, const ek_node *m, int top)
{
    const ek_node *pat = m->u.bind.pattern;
    const ek_node *ann = m->u.bind.type;
    const ek_node *val = m->u.bind.value;
    ck_scheme *gen = NULL;
    const ek_type *bound;

    if (m->kind == EK_N_INVERT) {
        const ek_type *vt = ck_expr(c, val, NULL);

        if (!invert_pattern(pat)) {
            ck_error(c, pat, "E-refutable-binding",
                     "an inverted binding takes a parameter-position "
                     "pattern (Clauses 7.7.1, 7.8.1)");
        }
        bound = callback_param(c, m, vt);
        if (!top) {
            bind_pattern(c, pat, bound, NULL);
        }
        ck_row(c, m, bound, EK_RD_NONE);
        return bound;
    }
    if (ann) {
        ck_tvars tv;

        memset(&tv, 0, sizeof tv);
        bound = ck_elaborate(c, ann, &tv);
        check_phantom(c, ann, bound);
        gen = ck_mono(c, NULL);
        if (gen) {
            *gen = ck_generalize(c, bound);
        }
        if (val && bound && bound->kind == EK_T_FN) {
            /* The RHS is the function body (Clause 7.9.4): the
               annotation's parameters bind over it, a lambda is
               checked against the whole arrow, and any other
               expression against the result. */
            size_t k, n = ann->kind == EK_N_T_FN
                              ? ann->u.t_fn.params.count : 0;

            ck_scope_push(c);
            for (k = 0; k < n; k++) {
                const ek_node *p = ann->u.t_fn.params.data[k];

                if (p->u.t_field.name
                    && strcmp(p->u.t_field.name, "_") != 0) {
                    ck_scope_bind(c, p->u.t_field.name,
                                  bound->u.fn.params[k]);
                }
            }
            if (val->kind == EK_N_LAMBDA) {
                const ek_type *vt = ck_expr(c, val, bound);

                if (ck_conforms(c, vt, bound, 1) != CK_OK) {
                    ck_error(c, val, "E-type-mismatch",
                             "the binding is %s, the annotation is %s "
                             "(Clause 7.5.7)", ck_type_name(c, vt),
                             ck_type_name(c, bound));
                }
            } else {
                const ek_type *vt = ck_expr(c, val, bound->u.fn.result);
                ck_conform k = ck_conforms(c, vt, bound->u.fn.result, 1);

                if (k != CK_OK) {
                    ck_error(c, val, ck_conform_code(k),
                             "the body is %s, the result type is %s "
                             "(Clauses 6.3.2, 7.8.1)",
                             ck_type_name(c, vt),
                             ck_type_name(c, bound->u.fn.result));
                }
            }
            ck_scope_pop(c);
        } else if (val) {
            const ek_type *vt = ck_expr(c, val, bound);
            ck_conform k = ck_conforms(c, vt, bound, 1);

            if (k != CK_OK) {
                ck_error(c, val, ck_conform_code(k),
                         "the binding is %s, the annotation is %s "
                         "(Clause 7.5.7)", ck_type_name(c, vt),
                         ck_type_name(c, bound));
            }
        }
    } else {
        const ek_node *lam = as_lambda(val);

        if (lam) {
            /* `:=` does not generalize, and a lambda is check-only
               (Clause 7.6.3). */
            ck_error(c, lam, "E-lambda-check-only",
                     "`:=` does not generalize and a lambda is "
                     "check-only: no expected type supplies the "
                     "parameter types (Clauses 6.3.3, 7.6.3)");
        }
        bound = ck_expr(c, val, NULL);
    }
    if (!ck_irrefutable(pat)) {
        ck_error(c, pat, "E-refutable-binding",
                 "a value binding admits only irrefutable patterns "
                 "(Clause 7.7.1)");
    }
    if (!top) {
        bind_pattern(c, pat, bound, gen);
    }
    ck_row(c, m, bound, EK_RD_NONE);
    return bound;
}

/* A namespace member or extern: the annotation types the body and the
   surface symbol alike. */
static const ek_type *ck_annotated_decl(ck_ctx *c, const ek_node *ann,
                                        const ek_node *body)
{
    ck_tvars tv;
    const ek_type *t;

    memset(&tv, 0, sizeof tv);
    t = ck_elaborate(c, ann, &tv);
    if (body && ann->kind == EK_N_T_FN) {
        size_t i, n = ann->u.t_fn.params.count;

        ck_scope_push(c);
        for (i = 0; i < n; i++) {
            const ek_node *p = ann->u.t_fn.params.data[i];

            if (p->u.t_field.name && strcmp(p->u.t_field.name, "_") != 0) {
                ck_scope_bind(c, p->u.t_field.name,
                              t && t->kind == EK_T_FN && i < t->u.fn.count
                                  ? t->u.fn.params[i] : ck_ivar(c));
            }
        }
        ck_expr(c, body, t && t->kind == EK_T_FN ? t->u.fn.result : t);
        ck_scope_pop(c);
    } else if (body) {
        ck_expr(c, body, t);
    }
    return t;
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
        return ck_annotated_decl(c, m->u.namespace.type,
                                 m->u.namespace.value);
    case EK_N_TEST_DECL:
        ck_block(c, m->u.decl.body, NULL);
        return ck_unit(c);
    case EK_N_EXTERN_DECL:
        return ck_annotated_decl(c, m->u.extern_decl.type, NULL);
    default:
        return NULL;
    }
}
