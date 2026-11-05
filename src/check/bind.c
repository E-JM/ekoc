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

/* Binds an irrefutable pattern's names. */
static void bind_pattern(ck_ctx *c, const ek_node *pat, const ek_type *t,
                         ck_scheme *gen);

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
   generalizes, and the body reconciles. The declaration sugar — the
   annotation's parameters bound over the value as its body — belongs
   to a module member; a nested binding binds a value of the annotated
   type. */
static const ek_type *annotated_binding(ck_ctx *c, const ek_node *m,
                                        const ek_node *val, ck_scheme *gen,
                                        int top)
{
    const ek_node *ann = m->u.bind.type;
    const ek_symbol *sym = ck_decl_symbol(c, m);
    const ek_type *bound;
    const ck_scheme *saved_decl = c->inst_decl;
    const ek_type **saved_vars = c->inst_vars;
    size_t saved_nvars = c->inst_nvars;
    int unfed, saved_open;
    ck_tvars tv;

    memset(&tv, 0, sizeof tv);
    bound = ck_elaborate(c, ann, &tv);
    ck_check_phantom(c, ann, bound);
    unfed = ck_annotation_bits(c, ann, bound);
    if (gen) {
        *gen = ck_generalize(c, bound);
    }
    /* The body is checked under its own generalized scheme: a
       top-level declaration reuses (or installs) the canonical one, a
       nested binding is identified by its own scheme. */
    if (c->inst_body && gen) {
        if (sym && sym->type) {
            ck_scheme *have = (ck_scheme *)ck_map_get(&c->schemes, sym);

            if (!have) {
                ck_map_put(c, &c->schemes, sym, gen);
                have = gen;
            }
            c->inst_decl = have;
        } else {
            c->inst_decl = gen;
        }
        c->inst_vars = gen->vars;
        c->inst_nvars = gen->nvars;
    }
    saved_open = c->open_scope;
    c->open_scope = ck_signature_opens(bound);
    if (val && bound && bound->kind == EK_T_FN && top) {
        annotated_fn_body(c, ann, val, bound);
    } else if (val) {
        const ek_type *vt = ck_expr(c, val, bound);
        ck_conform k = ck_conforms(c, vt, bound, 1);

        if (k != CK_OK) {
            if (bound && bound->kind == EK_T_FN) {
                ck_error(c, val, ck_conform_code(k),
                         "the binding is %s, the annotation is %s; a named "
                         "function is a module member, and a local function "
                         "value binds a lambda at its annotated type "
                         "(Clause 7.5.7)", ck_type_name(c, vt),
                         ck_type_name(c, bound));
            } else {
                ck_error(c, val, ck_conform_code(k),
                         "the binding is %s, the annotation is %s "
                         "(Clause 7.5.7)", ck_type_name(c, vt),
                         ck_type_name(c, bound));
            }
        }
    }
    c->open_scope = saved_open;
    c->inst_decl = saved_decl;
    c->inst_vars = saved_vars;
    c->inst_nvars = saved_nvars;
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

    const ek_symbol *sym = ck_decl_symbol(c, m);
    const ck_scheme *saved_decl = c->inst_decl;
    const ek_type **saved_vars = c->inst_vars;
    size_t saved_nvars = c->inst_nvars;
    ck_scheme *gen = NULL;

    memset(&tv, 0, sizeof tv);
    t = ck_elaborate(c, ann, &tv);
    if (binding) {
        ck_check_phantom(c, ann, t);
        ck_annotation_bits(c, ann, t);
        c->open_scope = ck_signature_opens(t);
    }
    if (binding && body && c->inst_body) {
        if (sym && sym->type) {
            ck_scheme *have = (ck_scheme *)ck_map_get(&c->schemes, sym);

            if (!have) {
                have = ek_arena_alloc(c->arena, sizeof *have);
                if (!have) {
                    c->oom = 1;
                    return t;
                }
                *have = ck_generalize(c, t);
                ck_map_put(c, &c->schemes, sym, have);
            }
            gen = have;
            c->inst_decl = gen;
        } else if (t) {
            gen = ek_arena_alloc(c->arena, sizeof *gen);
            if (!gen) {
                c->oom = 1;
                return t;
            }
            *gen = ck_generalize(c, t);
            c->inst_decl = gen;
        }
        if (gen) {
            c->inst_vars = gen->vars;
            c->inst_nvars = gen->nvars;
        }
    }
    if (body && ann->kind == EK_N_T_FN && t && t->kind == EK_T_FN) {
        annotated_fn_body(c, ann, body, t);
    } else if (body) {
        ck_expr(c, body, t);
    }
    c->open_scope = saved_open;
    c->inst_decl = saved_decl;
    c->inst_vars = saved_vars;
    c->inst_nvars = saved_nvars;
    if (binding && body) {
        ck_declare_bit(c, m, ck_own_bit(t));
        ck_reconcile(c, m, t, body);
    }
    return t;
}

/* A declared service name and its fixed parameter count: `show` is
   one receiver, the equality and ordering family two. Zero names a
   service no dispatch fixes; those members are ordinary. */
static int service_arity(const char *name)
{
    if (strcmp(name, "show") == 0) {
        return 1;
    }
    if (strcmp(name, "eq") == 0 || strcmp(name, "ne") == 0
        || strcmp(name, "lt") == 0 || strcmp(name, "le") == 0
        || strcmp(name, "gt") == 0 || strcmp(name, "ge") == 0) {
        return 2;
    }
    return 0;
}

/* A declared service of the dispatch set carries a fixed signature:
   the receiver type the namespace names, the fixed result, and the
   pure arrow. A wrong signature breaks every call site that reads
   the member as the service, so the declaration is where it is
   reported. */
static void service_signature(ck_ctx *c, const ek_node *m, const ek_type *t)
{
    const ek_type *f = ck_prune(t);
    const char *name = m->u.namespace.name;
    const char *owner = m->u.namespace.owner.count
        ? m->u.namespace.owner.data[m->u.namespace.owner.count - 1]
        : m->u.namespace.name;
    const ek_type *p0;
    int want = service_arity(name);
    int ok;

    if (!want || !f) {
        return;
    }
    p0 = f->kind == EK_T_FN && f->u.fn.count ? f->u.fn.params[0] : NULL;
    ok = f->kind == EK_T_FN && f->u.fn.arrow == EK_ARROW_PURE
        && f->u.fn.count == (size_t)want && p0 && p0->kind == EK_T_NOMINAL
        && p0->u.nominal.sym
        && strcmp(p0->u.nominal.sym->name, owner) == 0
        && (want == 1 ? ck_is_std(f->u.fn.result, "String")
                      : p0 == f->u.fn.params[1]
                            && ck_is_std(f->u.fn.result, "Bool"));
    if (ok) {
        return;
    }
    ck_error(c, m, "E-service-signature",
             want == 1
                 ? "`%s` over `%s` is `(t: %s) -> String`, pure "
                   "(Clause 9.8.2)"
                 : "`%s` over `%s` is `(a: %s, b: %s) -> Bool`, pure "
                   "(Clauses 9.8.4, 9.8.5)",
             name, owner, owner, owner);
}

/* A declaration body is a block or one expression; both are checked
   in the same scope and against the same expected type. */
static const ek_type *decl_body(ck_ctx *c, const ek_node *body,
                                const ek_type *expected)
{
    if (!body) {
        return ck_unit(c);
    }
    if (body->kind == EK_N_BLOCK) {
        return ck_block(c, body, expected);
    }
    return ck_expr(c, body, expected);
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
    case EK_N_NAMESPACE: {
        int saved_body = c->inst_body;
        const ck_scheme *saved_decl = c->inst_decl;
        const ek_type **saved_vars = c->inst_vars;
        size_t saved_nvars = c->inst_nvars;
        const ek_type *t;

        c->inst_body = 1;
        c->inst_decl = NULL;
        c->inst_vars = NULL;
        c->inst_nvars = 0;
        t = ck_annotated_decl(c, m, m->u.namespace.type,
                              m->u.namespace.value, 1);
        c->inst_body = saved_body;
        c->inst_decl = saved_decl;
        c->inst_vars = saved_vars;
        c->inst_nvars = saved_nvars;
        service_signature(c, m, t);
        return t;
    }
    case EK_N_TEST_DECL: {
        int saved_body = c->inst_body;
        const ck_scheme *saved_decl = c->inst_decl;
        const ek_type **saved_vars = c->inst_vars;
        size_t saved_nvars = c->inst_nvars;

        c->inst_body = 1;
        c->inst_decl = NULL;
        c->inst_vars = NULL;
        c->inst_nvars = 0;
        (void)decl_body(c, m->u.decl.body, NULL);
        c->inst_body = saved_body;
        c->inst_decl = saved_decl;
        c->inst_vars = saved_vars;
        c->inst_nvars = saved_nvars;
        return ck_unit(c);
    }
    case EK_N_EXTERN_DECL:
        return ck_annotated_decl(c, m, m->u.extern_decl.type, NULL, 0);
    default:
        return NULL;
    }
}
