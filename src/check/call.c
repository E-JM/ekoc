#include <string.h>

#include "check/internal.h"

/* Calls, lambdas, and constructor application. A call needs a
   function type with exactly n parameters; the receiver of a member
   call is the first argument. An unknown callee unifies with the
   function type of its synthesized arguments. Every call records its
   world bit and a row. */

/* An unknown callee: the arguments synthesize and the callee unifies
   with their function type (opacity). */
static const ek_type *call_opaque(ck_ctx *c, const ek_node *e,
                                  const ek_type *f, const char *member,
                                  const ek_type *recv,
                                  const ek_type *bit_recv,
                                  const ek_nlist *args,
                                  const ek_node **nodes,
                                  const ek_type **types, size_t nargs,
                                  size_t off, int synthesize, int implicit)
{
    const ek_type **ps = NULL;
    const ek_type *ret = ck_ivar(c);
    const ek_type *built;
    size_t i;

    if (nargs) {
        ps = ek_arena_alloc(c->arena, nargs * sizeof *ps);
        if (!ps) {
            c->oom = 1;
            return ret;
        }
        if (recv) {
            ps[0] = recv;
        }
        for (i = 0; i < args->count; i++) {
            /* A fresh expectation per argument: literals and
               composites stay opaque under it (the opacity rule). */
            nodes[off + i] = args->data[i];
            ps[off + i] = ck_expr(c, args->data[i], ck_ivar(c));
            types[off + i] = ps[off + i];
        }
        if (implicit) {
            size_t slot = nargs - 1;

            nodes[slot] = NULL;
            ps[slot] = ck_ivar(c);
            types[slot] = ps[slot];
        }
    }
    built = ck_fn(c, ps, nargs, ret, EK_ARROW_PURE);
    if (f && f->kind == EK_T_IVAR) {
        ck_conforms(c, built, f, 0);
    } else if (f && !synthesize) {
        ck_error(c, e, "E-not-callable",
                 "a value of type %s is not called (Clause 7.8.1)",
                 ck_type_name(c, f));
    }
    ck_call_bit(c, e, ck_call_bit_of(c, f, member, bit_recv ? bit_recv : recv,
                                     nodes, types, nargs, off));
    ck_row(c, e, ret, EK_RD_NONE);
    return ret;
}

/* Checks one argument against its parameter, reporting the standard
   mismatch and keeping the observed type for the world-bit judgment. */
static const ek_type *arg_conform(ck_ctx *c, const ek_node *arg,
                                  const ek_type *ptype)
{
    const ek_type *at = ck_expr(c, arg, ptype);
    ck_conform k = ck_conforms(c, at, ptype, 1);

    if (k != CK_OK) {
        ck_error(c, arg, ck_conform_code(k),
                 "the argument is %s, the parameter is %s "
                 "(Clause 7.8.1)", ck_type_name(c, at),
                 ck_type_name(c, ptype));
    }
    return at;
}

/* Checks each argument against its parameter, recording the observed
   types for the world-bit judgment. */
static void call_args(ck_ctx *c, const ek_type *f, const ek_nlist *args,
                      const ek_node **nodes, const ek_type **types,
                      size_t off)
{
    size_t i;

    for (i = 0; i < args->count && off + i < f->u.fn.count; i++) {
        const ek_node *arg = args->data[i];

        nodes[off + i] = arg;
        types[off + i] = arg_conform(c, arg, f->u.fn.params[off + i]);
    }
}

static const ek_type *call_fn(ck_ctx *c, const ek_node *e,
                              const ek_type *fntype, const char *member,
                              const ek_type *recv, const ek_type *bit_recv,
                              const ek_nlist *args, int synthesize,
                              int implicit)
{
    const ek_type *f = ck_prune(fntype);
    const ek_type **types = NULL;
    const ek_node **nodes = NULL;
    size_t nargs = args->count + (recv ? 1 : 0) + (implicit ? 1 : 0);
    size_t off = recv ? 1 : 0;
    const ek_type *ret;

    if (nargs) {
        types = ek_arena_alloc(c->arena, nargs * sizeof *types);
        nodes = ek_arena_alloc(c->arena, nargs * sizeof *nodes);
        if (!types || !nodes) {
            c->oom = 1;
            return ck_ivar(c);
        }
        if (recv) {
            nodes[0] = NULL;
            types[0] = recv;
        }
    }
    if (!f || f->kind != EK_T_FN) {
        return call_opaque(c, e, f, member, recv, bit_recv, args, nodes,
                           types, nargs, off, synthesize, implicit);
    }
    if (implicit) {
        c->invert_fn = f;
        c->invert_cb = f->u.fn.count
                           ? f->u.fn.params[f->u.fn.count - 1] : NULL;
        nodes[nargs - 1] = NULL;
        types[nargs - 1] = c->invert_cb ? c->invert_cb : ck_ivar(c);
    }
    if (f->u.fn.count != nargs) {
        ck_error(c, e, "E-call-arity",
                 "the call supplies %zu arguments, the function takes %zu "
                 "(Clause 7.8.1)", nargs, f->u.fn.count);
    }
    call_args(c, f, args, nodes, types, off);
    if (recv && f->u.fn.count >= 1) {
        ck_conform k = ck_conforms(c, recv, f->u.fn.params[0], 1);

        if (k != CK_OK) {
            ck_error(c, e, ck_conform_code(k),
                     "the receiver is %s, the first parameter is %s "
                     "(Clauses 7.4.3, 7.8.2)", ck_type_name(c, recv),
                     ck_type_name(c, f->u.fn.params[0]));
        }
    }
    ret = f->u.fn.result;
    ck_call_bit(c, e, ck_call_bit_of(c, f, member,
                                     bit_recv ? bit_recv : recv, nodes, types,
                                     args->count, off));
    ck_row(c, e, ret, EK_RD_NONE);
    return ret;
}

/* A type namespace at the head of a callee: `T.f` or `Mod.T.f`. The
   head names a type, so it is not a value and supplies no receiver;
   its members are the type's namespace. */
static int type_head(ck_ctx *c, const ek_node *recv, const ek_type **out)
{
    const ek_ref *r = NULL;

    if (!recv) {
        return 0;
    }
    if (recv->kind != EK_N_NAME && recv->kind != EK_N_FIELD) {
        return 0;
    }
    if (!ck_ref_at(c, recv, EK_SYM_TYPE, &r)) {
        return 0;
    }
    *out = r->symbol ? ck_nominal(c, r->symbol, NULL, 0) : NULL;
    return 1;
}

/* The receiver of a direct (symbol-bearing) callee: `Mod.f(x)` and
   `T.f(x)` have no receiver; `a.Mod.f(x)` is `Mod.f(a, x)`. Returns
   the value to prepend, or NULL. */
static const ek_type *direct_receiver(ck_ctx *c, const ek_node *recv)
{
    const ek_ref *r = NULL;
    const ek_type *head = NULL;

    if (!recv) {
        return NULL;
    }
    if (type_head(c, recv, &head)) {
        return NULL; /* the head is a type namespace */
    }
    if (recv->kind == EK_N_NAME && ck_ref_at(c, recv, EK_SYM_MODULE, &r)) {
        return NULL; /* the head is the module itself */
    }
    if (recv->kind == EK_N_FIELD && ck_ref_at(c, recv, EK_SYM_MODULE, &r)) {
        return ck_expr(c, recv->u.field.recv, NULL);
    }
    return ck_expr(c, recv, NULL);
}

/* The derived `with` service (Clause 9.8.6): the receiver's record
   rebuilt from the partial field literal, the receiver's own type
   coming back. The service's second parameter has no type-syntax name,
   so the argument is checked directly as a partial record literal
   against the receiver's fields, layer by layer; the service is pure,
   like every derived member (Clause 9.8.7). */
static const ek_type *call_with(ck_ctx *c, const ek_node *e,
                                const ek_type *recv)
{
    const ek_type *r = ck_prune(recv);
    const ek_type *rec = r;
    const ek_node *arg;
    const ek_type *result = r;

    if (rec->kind == EK_T_NOMINAL) {
        rec = ck_decl_repr(c, rec);
    }
    if (e->u.call.args.count != 1) {
        ck_error(c, e, "E-call-arity",
                 "the call supplies %zu arguments, `with` takes the one "
                 "partial literal (Clauses 6.6.3, 9.8.6)",
                 e->u.call.args.count);
    }
    arg = e->u.call.args.count ? e->u.call.args.data[0] : NULL;
    if (arg && arg->kind != EK_N_COMPOSITE) {
        ck_error(c, arg, "E-type-mismatch",
                 "the argument of `with` is a partial record literal in "
                 "braces (Clauses 6.6.3, 9.8.6)");
        ck_expr(c, arg, NULL);
    } else if (arg && arg->u.kids.count) {
        /* The empty literal is the empty subset; anything in it goes
           through the record reading, which allows a subset at every
           layer of the `with` argument alone. */
        ck_composite(c, arg, rec, 1);
    }
    ck_call_bit(c, e, EK_BIT_PURE);
    ck_row(c, e, result, EK_RD_NONE);
    return result;
}

/* A receiver-chain call: the link is typed at the receiver, and a
   function-valued field calls its value. */
static const ek_type *call_member(ck_ctx *c, const ek_node *e,
                                  const ek_node *callee, int implicit)
{
    ek_reading rd = EK_RD_NONE;
    const ek_type *head = NULL;
    int qualified = type_head(c, callee->u.field.recv, &head);
    const ek_type *recv = qualified ? NULL
                          : direct_receiver(c, callee->u.field.recv);
    const ek_type *t = ck_link(c, callee, qualified ? head : recv,
                               callee->u.field.name, &rd);
    const ek_type *pt = rd == EK_RD_FIELD ? ck_prune(t) : NULL;

    if (rd == EK_RD_DERIVED && !qualified
        && strcmp(callee->u.field.name, "with") == 0) {
        /* The with service takes its literal argument directly; its
           link type is only the expressible stand-in. */
        return call_with(c, e, recv);
    }
    if (rd != EK_RD_FIELD) {
        /* Namespace, module, derived, and opaque readings all take
           the receiver first; a type head supplies namespace context,
           not an argument. */
        return call_fn(c, e, t, callee->u.field.name, recv,
                       qualified ? head : NULL, &e->u.call.args, 0, implicit);
    }
    if (pt && pt->kind == EK_T_FN) {
        return call_fn(c, e, t, callee->u.field.name, NULL, NULL,
                       &e->u.call.args, 0, implicit);
    }
    ck_error(c, e, "E-not-callable",
             "the field `%s` is not a function value (Clause 7.4.3)",
             callee->u.field.name);
    return call_fn(c, e, NULL, callee->u.field.name, NULL, NULL,
                   &e->u.call.args, 1, implicit);
}

/* The free-call form of a compiler service: its first argument is the
   receiver, exactly as the receiver form passes it, so the same member
   machinery selects the reading. Without an argument nothing supplies
   a receiver and the call stays opaque. */
static const ek_type *call_service(ck_ctx *c, const ek_node *e,
                                   const ek_node *callee, int implicit)
{
    const char *name = callee->u.path.data[0];
    ek_reading rd = EK_RD_NONE;
    ek_nlist rest;
    const ek_type *recv;
    const ek_type *t;

    if (e->u.call.args.count == 0) {
        return call_fn(c, e, NULL, name, NULL, NULL, &e->u.call.args, 1,
                       implicit);
    }
    recv = ck_expr(c, e->u.call.args.data[0], NULL);
    t = ck_link(c, callee, recv, name, &rd);
    rest.data = e->u.call.args.data + 1;
    rest.count = e->u.call.args.count - 1;
    return call_fn(c, e, t, name, recv, NULL, &rest, 0, implicit);
}

/* The constructor application over an argument list, defined with the
   juxtaposition path below. */
static const ek_type *ctor_apply(ck_ctx *c, const ek_node *e,
                                 const ck_ctor *ct, const ek_type *expected,
                                 ek_node *const *args, size_t nargs);

/* A qualified constructor applied as a call on its dotted head:
   the application is typed by the constructor, not by the opaque
   member reading. */
static const ek_type *call_ctor(ck_ctx *c, const ek_node *e,
                                const ek_node *callee, const ek_ref *ref,
                                const ek_type *expected, int implicit)
{
    const ck_ctor *ct = ref->symbol ? ck_ctor_of(c, ref->symbol) : NULL;

    if (!ct || !ct->owner) {
        return call_member(c, e, callee, implicit);
    }
    return ctor_apply(c, e, ct, expected, e->u.call.args.data,
                      e->u.call.args.count);
}

/* Typing one EK_N_CALL: a plain head, or the last link of a receiver
   chain. */
const ek_type *ck_call(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_node *callee = e->u.call.callee;
    const ek_ref *ref = NULL;
    int implicit = c->invert_call;

    if (c->oom) {
        return NULL;
    }
    c->invert_call = 0;
    if (implicit) {
        c->invert_fn = NULL;
        c->invert_cb = ck_ivar(c);
    }
    if (callee && callee->kind == EK_N_NAME) {
        /* A head the resolver classified; a compiler service carries a
           receiver-form ref and takes the identity reading. */
        if (ck_ref_at(c, callee, EK_SYM_MEMBER, &ref)
            && ref->station == EK_ST_RECEIVER) {
            return call_service(c, e, callee, implicit);
        }
        {
            const ek_type *t = ck_expr(c, callee, NULL);

            return call_fn(c, e, t, NULL, NULL, NULL, &e->u.call.args, 0,
                           implicit);
        }
    }
    if (!callee || callee->kind != EK_N_FIELD) {
        const ek_type *t = ck_expr(c, callee, NULL);

        return call_fn(c, e, t, NULL, NULL, NULL, &e->u.call.args, 0,
                       implicit);
    }
    {
        const ek_type *head = NULL;

        if (type_head(c, callee->u.field.recv, &head)) {
            /* A type namespace selects namespace members and derived
               services, and supplies no receiver value. */
            return call_member(c, e, callee, implicit);
        }
    }
    if (ck_ref_at(c, callee, EK_SYM_CONSTRUCTOR, &ref) && ref->symbol) {
        return call_ctor(c, e, callee, ref, expected, implicit);
    }
    if (ck_ref_at(c, callee, 0, &ref) && ref->symbol != NULL
        && (ref->kind == EK_SYM_FUNC || ref->kind == EK_SYM_NAMESPACE
            || ref->kind == EK_SYM_EXTERN)) {
        /* A qualified or module-path member: the receiver is the
           first argument. */
        const ek_type *recv = direct_receiver(c, callee->u.field.recv);
        const ek_type *t = ck_type_of(c, ref->symbol);

        return call_fn(c, e, t, callee->u.field.name, recv, NULL,
                       &e->u.call.args, 0, implicit);
    }
    return call_member(c, e, callee, implicit);
}

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
        ps[i] = pe && i < pe->u.fn.count ? pe->u.fn.params[i] : ck_ivar(c);
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

    if (!pe || pe->kind != EK_T_FN) {
        if (!pe || pe->kind != EK_T_IVAR) {
            ck_error(c, e, "E-lambda-check-only",
                     "a lambda is check-only and `:=` does not generalize: "
                     "no expected type supplies the parameter types "
                     "(Clause 7.6.3)");
        }
        pe = NULL;
    }
    if (n) {
        ps = ek_arena_alloc(c->arena, n * sizeof *ps);
        names = ek_arena_alloc(c->arena, n * sizeof *names);
        if (!ps || !names) {
            c->oom = 1;
            return expected;
        }
        lambda_params(c, params, pe, ps, names, n);
    }
    if (pe && pe->u.fn.count != n) {
        ck_error(c, e, "E-call-arity",
                 "the lambda takes %zu parameters, the expected function "
                 "type has %zu (Clause 7.8.1)", n, pe->u.fn.count);
    }
    ck_scope_push(c);
    {
        size_t i;

        for (i = 0; i < n; i++) {
            if (names[i]) {
                ck_scope_bind(c, names[i], ps[i]);
            }
        }
    }
    ret = ck_expr(c, body, pe ? pe->u.fn.result : NULL);
    ck_scope_pop(c);
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
    const ek_type *iv = ck_ivar(c);

    ck_expr(c, e->u.pair.b, iv);
    if (expected) {
        ck_conforms(c, iv, expected, 1);
    }
    ck_row(c, e, iv, EK_RD_NONE);
    return iv;
}

/* Typing one constructor application over its argument nodes: the
   anchor fixes a generic instantiation, a payload constructor takes
   exactly one argument, placed and conformed at the instantiated
   payload type, and a zero-payload one takes none. */
static const ek_type *ctor_apply(ck_ctx *c, const ek_node *e,
                                 const ck_ctor *ct, const ek_type *expected,
                                 ek_node *const *args, size_t nargs)
{
    const ek_type *exp = ck_prune(expected);
    const ck_decl *d = ct->owner ? ck_decl_of(c, ct->owner) : NULL;
    const ek_type *payload = NULL;
    const ek_type *out;
    size_t i, nb = d ? d->nbinders : 0, want = ct->payload ? 1 : 0;

    if (nb == 0) {
        out = ck_nominal(c, ct->owner, NULL, 0);
        payload = ct->payload;
    } else if (exp && exp->kind == EK_T_NOMINAL
               && exp->u.nominal.sym == ct->owner
               && exp->u.nominal.argc == nb) {
        out = exp;
        payload = ct->payload
                      ? ck_apply_binders(c, ct->payload, d->binders,
                                         exp->u.nominal.args, nb)
                      : NULL;
    } else {
        ck_error(c, e, "E-undetermined",
                 "the constructor `%s` has no unique instantiation; an "
                 "expected type determines it (Clause 7.6.4)",
                 ct->sym ? ct->sym->name : "?");
        for (i = 0; i < nargs; i++) {
            ck_expr(c, args[i], NULL);
        }
        return ck_ivar(c);
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
        const ek_type *iv = ck_ivar(c);

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

/* Constructor application by juxtaposition is check-only: it takes
   its instantiation from the expected type. */
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
