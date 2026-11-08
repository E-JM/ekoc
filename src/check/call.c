#include <string.h>

#include "check/internal.h"

/* Calls, lambdas, and constructor application. A call needs a
   function type with exactly n parameters; the receiver of a member
   call is the first argument. A bottom callee never evaluates, so
   its call types as `Never` without a not-callable diagnostic. An
   unknown callee unifies with the function type of its synthesized
   arguments. Every call records its world bit and a row. */

/* The callee expression, typed with its call marked so a generic
   instantiation here records an instance edge. */
const ek_type *callee_expr(ck_ctx *c, const ek_node *callee,
                                  const ek_node *call)
{
    const ek_node *saved = c->inst_call;
    const ek_type *t;

    c->inst_call = call;
    t = ck_expr(c, callee, NULL);
    c->inst_call = saved;
    return t;
}

/* A callee symbol's type, marked likewise. */
const ek_type *callee_symbol_type(ck_ctx *c, const ek_symbol *sym,
                                         const ek_node *call)
{
    const ek_node *saved = c->inst_call;
    const ek_type *t;

    c->inst_call = call;
    t = ck_type_of(c, sym);
    c->inst_call = saved;
    return t;
}

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
    /* A bottom callee does not return: the call is `Never`; anything
       else synthesizes opaquely. */
    const ek_type *ret = ck_is_bottom(f) ? ck_never(c) : ck_opaque(c);
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
            int saved = c->opaque_arg;

            nodes[off + i] = args->data[i];
            c->opaque_arg = 1;
            ps[off + i] = ck_expr(c, args->data[i], ck_opaque(c));
            c->opaque_arg = saved;
            types[off + i] = ps[off + i];
        }
        if (implicit) {
            size_t slot = nargs - 1;

            nodes[slot] = NULL;
            ps[slot] = ck_opaque(c);
            types[slot] = ps[slot];
        }
    }
    built = ck_fn(c, ps, nargs, ret, EK_ARROW_PURE);
    if (f && f->kind == EK_T_IVAR) {
        ck_conforms(c, built, f, 0);
    } else if (f && !synthesize && !ck_is_bottom(f)) {
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
const ek_type *arg_conform(ck_ctx *c, const ek_node *arg,
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

/* True when any written argument failed to parse: the site is already
   reported, and no arity or type claim about it lands. A failed
   argument parses to an empty paren. */
static int args_broken(const ek_nlist *args)
{
    size_t i;

    for (i = 0; i < args->count; i++) {
        const ek_node *arg = args->data[i];

        if (!arg || (arg->kind == EK_N_PAREN && !arg->u.wrap.inner)) {
            return 1;
        }
    }
    return 0;
}

/* The call's slot arrays: argument nodes and their observed types,
   with the receiver in the first slot when the call carries one.
   Returns 0 when an allocation failed. */
static int call_slots(ck_ctx *c, size_t nargs, const ek_type *recv,
                      const ek_node ***nodes, const ek_type ***types)
{
    if (!nargs) {
        return 1;
    }
    *types = ek_arena_alloc(c->arena, nargs * sizeof **types);
    *nodes = ek_arena_alloc(c->arena, nargs * sizeof **nodes);
    if (!*types || !*nodes) {
        c->oom = 1;
        return 0;
    }
    if (recv) {
        (*nodes)[0] = NULL;
        (*types)[0] = recv;
    }
    return 1;
}

/* The receiver conforms before the arguments: an argument checked
   under a parameter that shares the receiver's variables sees them
   determined. */
static void recv_conform(ck_ctx *c, const ek_node *e, const ek_type *f,
                         const ek_type *recv)
{
    ck_conform k;

    if (!recv || f->u.fn.count < 1) {
        return;
    }
    k = ck_conforms(c, recv, f->u.fn.params[0], 1);
    if (k != CK_OK) {
        ck_error(c, e, ck_conform_code(k),
                 "the receiver is %s, the first parameter is %s "
                 "(Clauses 7.4.3, 7.8.2)", ck_type_name(c, recv),
                 ck_type_name(c, f->u.fn.params[0]));
    }
}

const ek_type *call_fn(ck_ctx *c, const ek_node *e,
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

    if (!call_slots(c, nargs, recv, &nodes, &types)) {
        return ck_ivar(c);
    }
    if (!f || f->kind != EK_T_FN) {
        return call_opaque(c, e, f, member, recv, bit_recv, args, nodes,
                           types, nargs, off, synthesize, implicit);
    }
    if (args_broken(args)) {
        /* The site is already reported; the call reads as opacity, and
           no claim about the callee lands. */
        return call_opaque(c, e, f, member, recv, bit_recv, args, nodes,
                           types, nargs, off, 1, implicit);
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
    recv_conform(c, e, f, recv);
    call_args(c, f, args, nodes, types, off);
    ret = f->u.fn.result;
    ck_call_bit(c, e, ck_call_bit_of(c, f, member,
                                     bit_recv ? bit_recv : recv, nodes, types,
                                     args->count, off));
    ck_row(c, e, ret, EK_RD_NONE);
    return ret;
}
