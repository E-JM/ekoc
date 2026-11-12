#include <string.h>

#include "check/internal.h"

/* The application forms beside the call: lambda checking against an
   expected function type, and constructor application by
   juxtaposition, which takes its instantiation from the expected
   type. */

/* The lambda's own arrow: the body bit decides it, and a
   world-touching body does not adapt to a pure expected arrow. */
static const ek_type *lambda_arrow(ck_ctx *c, const ek_node *e,
                                   const ek_type *expected,
                                   const ek_type *pe, const ek_type **ps,
                                   size_t n, const ek_type *ret, ek_bit b)
{
    ck_lambda_bit(c, e, b);
    if (pe && pe->u.fn.arrow == EK_ARROW_PURE && b == EK_BIT_WORLD) {
        ck_error(c, e, "E-world-bit",
                 "the lambda body is world-touching, the expected "
                 "arrow is pure (Clause 7.9.5)");
    }
    if (!pe) {
        pe = ck_fn(c, ps, n, ret,
                   b == EK_BIT_WORLD ? EK_ARROW_WORLD
                       : b == EK_BIT_OPEN ? EK_ARROW_OPEN
                                          : EK_ARROW_PURE);
        if (expected && ck_prune(expected)->kind == EK_T_IVAR) {
            ck_conforms(c, pe, expected, 0);
        }
    }
    return pe;
}

/* The lambda's parameters: names from the surface, types from the
   expected arrow. */
static void lambda_params(ck_ctx *c, const ek_node *params,
                          const ek_type *pe, const ek_type **ps,
                          const char **names, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        const ek_node *p = params->u.kids.data[i];

        names[i] = p->kind == EK_N_NAME && p->u.path.count == 1
                       && strcmp(p->u.path.data[0], "_") != 0
                   ? p->u.path.data[0] : NULL;
        ps[i] = pe && i < pe->u.fn.count ? pe->u.fn.params[i] : ck_opaque(c);
    }
}

/* The expected function type a lambda checks against: anything but a
   function type or an open inference variable refuses, since a lambda
   is check-only and `:=` does not generalize. */
static const ek_type *lambda_expected(ck_ctx *c, const ek_node *e,
                                      const ek_type *pe)
{
    if (pe && pe->kind == EK_T_FN) {
        return pe;
    }
    if (!pe || pe->kind != EK_T_IVAR) {
        ck_error(c, e, "E-lambda-check-only",
                 "a lambda is check-only and `:=` does not generalize: "
                 "no expected type supplies the parameter types "
                 "(Clause 7.6.3)");
    }
    return NULL;
}

/* The lambda's parameter and name arrays, allocated and filled from
   the surface and the expected arrow. Returns 0 when an allocation
   failed. */
static int lambda_tables(ck_ctx *c, const ek_node *params,
                         const ek_type *pe, const ek_type ***ps,
                         const char ***names, size_t n)
{
    if (!n) {
        return 1;
    }
    *ps = ek_arena_alloc(c->arena, n * sizeof **ps);
    *names = ek_arena_alloc(c->arena, n * sizeof **names);
    if (!*ps || !*names) {
        c->oom = 1;
        return 0;
    }
    lambda_params(c, params, pe, *ps, *names, n);
    return 1;
}

/* The parameters bound in their own scope around the body. */
static const ek_type *lambda_body(ck_ctx *c, const ek_node *body,
                                  const ek_type *pe, const char **names,
                                  const ek_type **ps, size_t n)
{
    const ek_type *ret;
    size_t i;

    ck_scope_push(c);
    for (i = 0; i < n; i++) {
        if (names[i]) {
            ck_scope_bind(c, names[i], ps[i]);
        }
    }
    ret = ck_expr(c, body, pe ? pe->u.fn.result : NULL);
    ck_scope_pop(c);
    return ret;
}

/* The body flows into the result at a value position: it conforms, so
   an unbound result slot of the expectation binds from the body and a
   mismatched body is reported at the lambda, not far away at the
   use. */
static void lambda_result(ck_ctx *c, const ek_node *body, const ek_type *pe,
                          const ek_type *ret)
{
    ck_conform k;

    if (!pe || !ret) {
        return;
    }
    k = ck_conforms(c, ret, pe->u.fn.result, 1);
    if (k != CK_OK) {
        ck_error(c, body, ck_conform_code(k),
                 "the lambda body is %s, the result is %s "
                 "(Clause 7.8.1)", ck_type_name(c, ret),
                 ck_type_name(c, pe->u.fn.result));
    }
}

/* Lambda checking: check-only against an expected function type; the
   parameters come from it. */
const ek_type *ck_lambda(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_node *params = e->u.pair.a;
    const ek_node *body = e->u.pair.b;
    const ek_type *pe = ck_prune(expected);
    const ek_type **ps = NULL;
    const char **names = NULL;
    const ek_type *ret;
    size_t n = params ? params->u.kids.count : 0;

    pe = lambda_expected(c, e, pe);
    if (!lambda_tables(c, params, pe, &ps, &names, n)) {
        return expected;
    }
    if (pe && pe->u.fn.count != n) {
        ck_error(c, e, "E-call-arity",
                 "the lambda takes %zu parameters, the expected function "
                 "type has %zu (Clause 7.8.1)", n, pe->u.fn.count);
    }
    ret = lambda_body(c, body, pe, names, ps, n);
    lambda_result(c, body, pe, ret);
    pe = lambda_arrow(c, e, expected, pe, ps, n, ret, ck_body_bit(c, body));
    ck_row(c, e, pe, EK_RD_NONE);
    return pe;
}

/* An opaque constructor application: the std placeholder bridge, and
   the reading an unknown declaration falls back to. The payload binds
   loosely and the value conforms into the expected type. */
static const ek_type *apply_opaque(ck_ctx *c, const ek_node *e,
                                   const ek_type *expected)
{
    const ek_type *iv = ck_opaque(c);
    int saved = c->opaque_arg;

    c->opaque_arg = 1;
    ck_expr(c, e->u.pair.b, ck_opaque(c));
    c->opaque_arg = saved;
    if (expected) {
        ck_conforms(c, iv, expected, 1);
    }
    ck_row(c, e, iv, EK_RD_NONE);
    return iv;
}

/* The constructed type and its payload at the anchor: a zero-binder
   owner self-anchors, an undetermined expectation takes fresh
   variables, and a matching nominal anchor is the instance. NULL when
   no anchor determines the instantiation. */
static const ek_type *apply_inst(ck_ctx *c, const ek_node *e,
                                 const ck_ctor *ct, const ck_decl *d,
                                 const ek_type *exp, size_t nb,
                                 const ek_type **payload)
{
    if (nb == 0) {
        *payload = ct->payload;
        return ck_nominal(c, ct->owner, NULL, 0);
    }
    if (exp && exp->kind == EK_T_IVAR) {
        /* An undetermined expectation: the application supplies fresh
           variables and the expectation takes the instance. */
        const ek_type *out = ck_nominal_fresh(c, ct->owner);

        *payload = ct->payload
                       ? ck_apply_binders(c, ct->payload, d->binders,
                                          out->u.nominal.args, nb)
                       : NULL;
        ck_conforms(c, out, exp, 1);
        return out;
    }
    if (exp && exp->kind == EK_T_NOMINAL
        && exp->u.nominal.sym == ct->owner
        && exp->u.nominal.argc == nb) {
        *payload = ct->payload
                       ? ck_apply_binders(c, ct->payload, d->binders,
                                          exp->u.nominal.args, nb)
                       : NULL;
        return exp;
    }
    ck_error(c, e, "E-undetermined",
             "the constructor `%s` has no unique instantiation; an "
             "expected type determines it (Clause 7.6.4)",
             ct->sym ? ct->sym->name : "?");
    return NULL;
}

/* Typing one constructor application over its argument nodes: the
   anchor fixes a generic instantiation, a payload constructor takes
   exactly one argument, placed and conformed at the instantiated
   payload type, and a zero-payload one takes none. */
const ek_type *ctor_apply(ck_ctx *c, const ek_node *e,
                                 const ck_ctor *ct, const ek_type *expected,
                                 ek_node *const *args, size_t nargs)
{
    const ek_type *exp = ck_prune(expected);
    const ck_decl *d = ct->owner ? ck_decl_of(c, ct->owner) : NULL;
    const ek_type *payload = NULL;
    const ek_type *out;
    size_t i, nb = d ? d->nbinders : 0, want = ct->payload ? 1 : 0;

    out = apply_inst(c, e, ct, d, exp, nb, &payload);
    if (!out) {
        for (i = 0; i < nargs; i++) {
            ck_expr(c, args[i], NULL);
        }
        return ck_opaque(c);
    }
    if (nargs != want) {
        ck_error(c, e, "E-call-arity",
                 "the constructor `%s` takes %zu argument%s, the call "
                 "supplies %zu (Clause 7.5.2)", ct->sym->name, want,
                 want == 1 ? "" : "s", nargs);
    }
    for (i = 0; i < nargs; i++) {
        if (i == 0 && payload) {
            arg_conform(c, args[i], payload);
        } else {
            ck_expr(c, args[i], NULL);
        }
    }
    ck_row(c, e, out, EK_RD_NONE);
    return out;
}

/* A constructor with a known declaration: a zero-binder owner
   self-anchors, a generic one takes its instantiation from the
   anchor. NULL when the declaration carries no owner. */
static const ek_type *apply_known(ck_ctx *c, const ek_node *e,
                                  const ck_ctor *ct, const ek_type *expected)
{
    ek_node *arg = e->u.pair.b;

    if (!ct || !ct->owner) {
        return NULL;
    }
    return ctor_apply(c, e, ct, expected, &arg, 1);
}

/* A constructor application over the reference's candidate set. */
static const ek_type *apply_candidates(ck_ctx *c, const ek_node *e,
                                       const ek_ref *ref,
                                       const ek_type *expected)
{
    ck_ctorsel sel = ck_ctor_select(c, e->u.pair.a, ref, expected);
    const ek_type *t;

    if (!sel.sym) {
        const ek_type *iv = ck_opaque(c);

        ck_expr(c, e->u.pair.b, NULL);
        ck_row(c, e, iv, EK_RD_NONE);
        return iv;
    }
    if (sel.sym->decl == NULL) {
        return apply_opaque(c, e, expected);
    }
    t = apply_known(c, e, ck_ctor_of(c, sel.sym), expected);
    return t ? t : apply_opaque(c, e, expected);
}

/* The constructor a type-qualified application head names: the head's
   receiver resolved as the owner type and the link as one of its
   variants. NULL outside the form or when the spelling names none. */
const ek_symbol *qualified_head_ctor(ck_ctx *c, const ek_node *head)
{
    const ek_ref *tr = NULL;
    const ek_symbol *tsym;
    size_t k, i;

    if (!head || head->kind != EK_N_FIELD
        || !ck_ref_at(c, head->u.field.recv, EK_SYM_TYPE, &tr)
        || !tr->symbol) {
        return NULL;
    }
    tsym = tr->symbol;
    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, tsym->module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, tsym->name) == 0
                && strcmp(m->name, head->u.field.name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* Constructor application by juxtaposition is check-only: it takes
   its instantiation from the expected type. The head is a bare
   PascalCase name; a dotted head applies as a call. */
const ek_type *ck_apply(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_node *head = e->u.pair.a;
    const ek_ref *ref = NULL;
    const ek_type *t;

    if (c->oom) {
        return expected;
    }
    if (head && ck_ref_at(c, head, 0, &ref) && ref->ncands) {
        return apply_candidates(c, e, ref, expected);
    }
    (void)ck_ref_at(c, head, EK_SYM_CONSTRUCTOR, &ref);
    t = ref && ref->symbol ? apply_known(c, e, ck_ctor_of(c, ref->symbol),
                                        expected) : NULL;
    return t ? t : apply_opaque(c, e, expected);
}
