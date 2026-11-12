#include <string.h>

#include "check/internal.h"

/* The annotated forms. An annotation types the value, the scheme
   generalizes, and the body reconciles under the annotation's own
   variables; the declaration sugar binds the annotation's parameters
   over the value as its body. A namespace member, an extern, and a
   test are the declared members of ck_stmt, and a declared service of
   the dispatch set carries the signature every call site reads. */

/* The instantiation state of the enclosing body, saved while a
   declaration body runs under its own generalized scheme. */
typedef struct {
    int body;
    int open;
    const ck_scheme *decl;
    const ek_type **vars;
    size_t nvars;
} ck_inst_saved;

static void inst_save(ck_ctx *c, ck_inst_saved *s)
{
    s->body = c->inst_body;
    s->open = c->open_scope;
    s->decl = c->inst_decl;
    s->vars = c->inst_vars;
    s->nvars = c->inst_nvars;
}

/* A declaration body runs under a fresh instantiation state: its own
   scheme, no enclosing call. */
static void inst_reset(ck_ctx *c, int body)
{
    c->inst_body = body;
    c->inst_decl = NULL;
    c->inst_vars = NULL;
    c->inst_nvars = 0;
}

static void inst_restore(ck_ctx *c, const ck_inst_saved *s)
{
    c->inst_body = s->body;
    c->open_scope = s->open;
    c->inst_decl = s->decl;
    c->inst_vars = s->vars;
    c->inst_nvars = s->nvars;
}

/* The value beneath parentheses or a one-member block: the wrapper a
   binding or an arm carries without changing what it names. */
const ek_node *ck_value_inner(const ek_node *val)
{
    while (val && (val->kind == EK_N_PAREN
                   || (val->kind == EK_N_BLOCK
                       && val->u.kids.count == 1))) {
        val = val->kind == EK_N_PAREN ? val->u.wrap.inner
                                      : val->u.kids.data[0];
    }
    return val;
}

/* The lambda beneath the wrappers: the arm body an annotated if or
   case carries at the value reading. */
static const ek_node *arm_lambda(const ek_node *b)
{
    const ek_node *inner = ck_value_inner(b);

    return inner && inner->kind == EK_N_LAMBDA ? inner : NULL;
}

/* Every arm of the if or case is a lambda: the whole expression reads
   as a value of the arrow, not as the binding's body. */
static int arms_are_lambdas(const ek_node *val)
{
    size_t i;

    if (val->kind == EK_N_IF) {
        return arm_lambda(val->u.ifexpr.then) != NULL
               && (!val->u.ifexpr.els
                   || arm_lambda(val->u.ifexpr.els) != NULL);
    }
    if (val->kind == EK_N_CASE) {
        if (val->u.caseexpr.arms.count == 0) {
            return 0;
        }
        for (i = 0; i < val->u.caseexpr.arms.count; i++) {
            if (!arm_lambda(val->u.caseexpr.arms.data[i]->u.arm.body)) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

/* The right-hand side of an annotated function binding: the
   annotation's parameters bind over it, a lambda is checked against
   the whole arrow, any other expression against the result -- and an
   if or case whose every arm is a lambda is the value reading, the
   arms checking against the whole arrow. */
void annotated_fn_body(ck_ctx *c, const ek_node *ann, const ek_node *val,
                       const ek_type *bound)
{
    size_t k, n = ann->kind == EK_N_T_FN ? ann->u.t_fn.params.count : 0;

    ck_scope_push(c);
    for (k = 0; k < n; k++) {
        const ek_node *p = ann->u.t_fn.params.data[k];

        if (p->u.t_field.name && strcmp(p->u.t_field.name, "_") != 0) {
            ck_scope_bind(c, p->u.t_field.name, bound->u.fn.params[k]);
        }
    }
    const ek_node *lam = val;

    while (lam && lam->kind == EK_N_PAREN) {
        lam = lam->u.wrap.inner;
    }
    if (lam && lam->kind == EK_N_LAMBDA) {
        const ek_type *vt = ck_expr(c, lam, bound);

        if (ck_conforms(c, vt, bound, 1) != CK_OK) {
            ck_error(c, lam, "E-type-mismatch",
                     "the binding is %s, the annotation is %s "
                     "(Clause 7.5.7)", ck_type_name(c, vt),
                     ck_type_name(c, bound));
        }
    } else if (arms_are_lambdas(val)
               || arms_are_lambdas(ck_value_inner(val))) {
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

/* The conformance miss of an annotated binding's value: a function
   annotation spells the local-function rule out. */
static void annotated_mismatch(ck_ctx *c, const ek_node *val, ck_conform k,
                               const ek_type *vt, const ek_type *bound)
{
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

/* The value of one annotated binding against its annotation: the
   declaration sugar for a module member's function annotation, the
   conformance report for everything else. */
static void annotated_value(ck_ctx *c, const ek_node *ann,
                            const ek_node *val, const ek_type *bound,
                            int top)
{
    if (val && bound && bound->kind == EK_T_FN && top) {
        annotated_fn_body(c, ann, val, bound);
        return;
    }
    if (val) {
        const ek_type *vt = ck_expr(c, val, bound);
        ck_conform k = ck_conforms(c, vt, bound, 1);

        if (k != CK_OK) {
            annotated_mismatch(c, val, k, vt, bound);
        }
    }
}

/* The scheme a top-level body instantiates at: the canonical scheme
   of the declared symbol, or the binding's own. */
static void install_scheme(ck_ctx *c, const ek_symbol *sym, ck_scheme *gen)
{
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

/* One annotated binding: the annotation types the value, the scheme
   generalizes, and the body reconciles. The declaration sugar — the
   annotation's parameters bound over the value as its body — belongs
   to a module member; a nested binding binds a value of the annotated
   type. */
const ek_type *annotated_binding(ck_ctx *c, const ek_node *m,
                                 const ek_node *val, ck_scheme *gen,
                                 int top)
{
    const ek_node *ann = m->u.bind.type;
    const ek_symbol *sym = ck_decl_symbol(c, m);
    const ek_type *bound;
    ck_inst_saved saved;
    int unfed;
    ck_tvars tv;

    inst_save(c, &saved);
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
        install_scheme(c, sym, gen);
    }
    c->open_scope = ck_signature_opens(bound);
    annotated_value(c, ann, val, bound, top);
    inst_restore(c, &saved);
    if (val) {
        ck_declare_bit(c, m, ck_own_bit(bound));
        /* A nested value binding carries no bit: its body's call bits
           answer to the enclosing function, as at a `:=` binding. A
           function annotation and the top level keep the own-bit rule
           (Clauses 7.9.2, 7.1.2). */
        if (!unfed && (top || bound->kind == EK_T_FN)) {
            ck_reconcile(c, m, bound, val);
        }
    }
    return bound;
}

/* The scheme a declaration body runs under: the declared symbol's
   canonical scheme, installed on first sight, or a fresh
   generalization. 0 when the allocation failed and the caller
   returns. */
static int install_decl_scheme(ck_ctx *c, const ek_symbol *sym,
                               const ek_type *t)
{
    ck_scheme *gen = NULL;

    if (sym && sym->type) {
        ck_scheme *have = (ck_scheme *)ck_map_get(&c->schemes, sym);

        if (!have) {
            have = ek_arena_alloc(c->arena, sizeof *have);
            if (!have) {
                c->oom = 1;
                return 0;
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
            return 0;
        }
        *gen = ck_generalize(c, t);
        c->inst_decl = gen;
    }
    if (gen) {
        c->inst_vars = gen->vars;
        c->inst_nvars = gen->nvars;
    }
    return 1;
}

/* A namespace member or extern: the annotation types the body and the
   surface symbol alike. A namespace member is an annotated binding; an
   extern carries no body and its rules are the extern.c set. */
const ek_type *ck_annotated_decl(ck_ctx *c, const ek_node *m,
                                 const ek_node *ann, const ek_node *body,
                                 int binding)
{
    ck_tvars tv;
    const ek_type *t;
    ck_inst_saved saved;
    const ek_symbol *sym = ck_decl_symbol(c, m);

    inst_save(c, &saved);
    memset(&tv, 0, sizeof tv);
    t = ck_elaborate(c, ann, &tv);
    if (binding) {
        ck_check_phantom(c, ann, t);
        ck_annotation_bits(c, ann, t);
        c->open_scope = ck_signature_opens(t);
    }
    if (binding && body && c->inst_body) {
        if (!install_decl_scheme(c, sym, t)) {
            return t;
        }
    }
    if (body && ann->kind == EK_N_T_FN && t && t->kind == EK_T_FN) {
        annotated_fn_body(c, ann, body, t);
    } else if (body) {
        ck_expr(c, body, t);
    }
    inst_restore(c, &saved);
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

/* The namespace member of ck_stmt: the annotation types the member
   under a fresh instantiation state, and the declared services
   answer for their signatures. */
const ek_type *ck_namespace_member(ck_ctx *c, const ek_node *m)
{
    ck_inst_saved saved;
    const ek_type *t;

    inst_save(c, &saved);
    inst_reset(c, 1);
    t = ck_annotated_decl(c, m, m->u.namespace.type, m->u.namespace.value,
                          1);
    inst_restore(c, &saved);
    service_signature(c, m, t);
    return t;
}

/* The test member of ck_stmt: its body is checked for its rows and
   diagnostics alone, and the member is a unit value. */
void ck_test_decl(ck_ctx *c, const ek_node *m)
{
    ck_inst_saved saved;

    inst_save(c, &saved);
    inst_reset(c, 1);
    (void)decl_body(c, m->u.decl.body, NULL);
    inst_restore(c, &saved);
}
