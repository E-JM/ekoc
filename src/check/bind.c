#include <string.h>

#include "check/internal.h"

/* The binding forms as check sees them: annotated and plain value
   bindings, rebinds, inverted bindings, namespace members, tests, and
   externs. Type declarations are built in decl.c, entry rules in
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

/* Binds an irrefutable pattern's names. */
static void bind_pattern(ck_ctx *c, const ek_node *pat, const ek_type *t,
                         ck_scheme *gen);

/* The brace case of a value-binding pattern: fields bind at their
   field types. */
static void bind_brace(ck_ctx *c, const ek_node *pat, const ek_type *t)
{
    const ek_type *p = repr_of(c, t);
    size_t i;

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
                                ? p->u.tuple.items[i] : ck_ivar(c);

            bind_pattern(c, pat->u.kids.data[i], ct, NULL);
        }
        return;
    }
    case EK_N_P_BRACE:
        bind_brace(c, pat, t);
        return;
    default:
        return; /* refutable forms never reach a value binding */
    }
}

/* The inverted form: the value is a call whose last parameter types
   the pattern. */
static const ek_type *invert_binding(ck_ctx *c, const ek_node *m,
                                     const ek_node *pat, int top)
{
    const ek_node *val = m->u.bind.value;
    const ek_type *vt = ck_expr(c, val, NULL);
    const ek_type *bound;

    if (!ck_invert_pattern(pat)) {
        ck_error(c, pat, "E-refutable-binding",
                 "an inverted binding takes a parameter-position "
                 "pattern (Clauses 7.7.1, 7.8.1)");
    }
    bound = ck_callback_param(c, m, vt);
    if (!top) {
        bind_pattern(c, pat, bound, NULL);
    }
    ck_row(c, m, bound, EK_RD_NONE);
    return bound;
}

/* The right-hand side of an annotated function binding: the
   annotation's parameters bind over it, a lambda is checked against
   the whole arrow, and any other expression against the result. */
static void annotated_fn_body(ck_ctx *c, const ek_node *ann,
                              const ek_node *val, const ek_type *bound)
{
    size_t k, n = ann->kind == EK_N_T_FN ? ann->u.t_fn.params.count : 0;

    ck_scope_push(c);
    for (k = 0; k < n; k++) {
        const ek_node *p = ann->u.t_fn.params.data[k];

        if (p->u.t_field.name && strcmp(p->u.t_field.name, "_") != 0) {
            ck_scope_bind(c, p->u.t_field.name, bound->u.fn.params[k]);
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
                     "(Clauses 6.3.2, 7.8.1)", ck_type_name(c, vt),
                     ck_type_name(c, bound->u.fn.result));
        }
    }
    ck_scope_pop(c);
}

/* One annotated binding: the annotation types the value, the scheme
   generalizes, and the body reconciles. */
static const ek_type *annotated_binding(ck_ctx *c, const ek_node *m,
                                        const ek_node *val, ck_scheme *gen)
{
    const ek_node *ann = m->u.bind.type;
    const ek_type *bound;
    int unfed, saved_open;
    ck_tvars tv;

    memset(&tv, 0, sizeof tv);
    bound = ck_elaborate(c, ann, &tv);
    ck_check_phantom(c, ann, bound);
    unfed = ck_annotation_bits(c, ann, bound);
    if (gen) {
        *gen = ck_generalize(c, bound);
    }
    saved_open = c->open_scope;
    c->open_scope = ck_signature_opens(bound);
    if (val && bound && bound->kind == EK_T_FN) {
        annotated_fn_body(c, ann, val, bound);
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
    c->open_scope = saved_open;
    if (val) {
        ck_declare_bit(c, m, ck_own_bit(bound));
        if (!unfed) {
            ck_reconcile(c, m, bound, val);
        }
    }
    return bound;
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
        return invert_binding(c, m, pat, top);
    }
    if (m->u.bind.type) {
        gen = ck_mono(c, NULL);
        bound = annotated_binding(c, m, val, gen);
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
   surface symbol alike. A namespace member is an annotated binding; an
   extern carries no body and its rules are the extern.c set. */
static const ek_type *ck_annotated_decl(ck_ctx *c, const ek_node *m,
                                        const ek_node *ann,
                                        const ek_node *body, int binding)
{
    ck_tvars tv;
    const ek_type *t;
    int saved_open = c->open_scope;

    memset(&tv, 0, sizeof tv);
    t = ck_elaborate(c, ann, &tv);
    if (binding) {
        ck_annotation_bits(c, ann, t);
        c->open_scope = ck_signature_opens(t);
    }
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
    c->open_scope = saved_open;
    if (binding && body) {
        ck_declare_bit(c, m, ck_own_bit(t));
        ck_reconcile(c, m, t, body);
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
        return ck_annotated_decl(c, m, m->u.namespace.type,
                                 m->u.namespace.value, 1);
    case EK_N_TEST_DECL:
        ck_block(c, m->u.decl.body, NULL);
        return ck_unit(c);
    case EK_N_EXTERN_DECL:
        return ck_annotated_decl(c, m, m->u.extern_decl.type, NULL, 0);
    default:
        return NULL;
    }
}
