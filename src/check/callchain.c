#include <string.h>

#include "check/internal.h"

/* The receiver chains of a call: the type-head and module-path walks
   that fix a member link's reading, the `with` service, the member
   and constructor dispatch, and the free-call form of a compiler
   service. */

/* A type namespace at the head of a callee: `T.f` or `Mod.T.f`. The
   head names a type, so it is not a value and supplies no receiver;
   its members are the type's namespace. */
int type_head(ck_ctx *c, const ek_node *recv, const ek_type **out)
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
    if (r->station == EK_ST_MODULE_PATH) {
        /* A type reached through a receiver is a namespace segment of
           that receiver's chain, not a type-position head. */
        return 0;
    }
    *out = r->symbol ? ck_nominal(c, r->symbol, NULL, 0) : NULL;
    return 1;
}

/* The receiver walk of a receiver-form chain. A module segment peels
   away unless the receiver's type carries a record field of the same
   spelling: the field wins the link, and the rest of the chain reads
   as member links off the field's value. `module` names the module
   while the link inside the callee is an unshadowed module segment. */
typedef struct {
    const ek_type *recv;
    const ek_symbol *module;
    int shadowed;
} chain_recv;

static void chain_receiver(ck_ctx *c, const ek_node *recv, chain_recv *w);

/* One link of the walk, innermost first. */
static void chain_link(ck_ctx *c, const ek_node *link, chain_recv *w)
{
    const ek_ref *r = NULL;
    const ek_type *field;
    ek_reading rd = EK_RD_NONE;

    if (ck_ref_at(c, link, EK_SYM_MODULE, &r)
        || (ck_ref_at(c, link, EK_SYM_TYPE, &r)
            && r->station == EK_ST_MODULE_PATH)) {
        field = ck_field_of(c, w->recv, link->u.field.name);
        if (field) {
            /* Station one wins the segment: it reads as the field,
               and the chain continues as member links. */
            w->shadowed = 1;
            w->module = NULL;
            w->recv = ck_link(c, link, w->recv, link->u.field.name, &rd);
            return;
        }
        if (r->kind == EK_SYM_MODULE) {
            w->module = r->symbol;
        }
        return; /* a module or type segment contributes no value */
    }
    w->module = NULL;
    w->recv = ck_link(c, link, w->recv, link->u.field.name, &rd);
}

static void chain_receiver(ck_ctx *c, const ek_node *recv, chain_recv *w)
{
    const ek_ref *r = NULL;

    w->recv = NULL;
    w->module = NULL;
    w->shadowed = 0;
    if (!recv) {
        return;
    }
    if (recv->kind != EK_N_FIELD) {
        if (recv->kind == EK_N_NAME
            && ck_ref_at(c, recv, EK_SYM_MODULE, &r)) {
            return; /* the chain heads at the module itself */
        }
        w->recv = ck_expr(c, recv, NULL);
        return;
    }
    chain_receiver(c, recv->u.field.recv, w);
    chain_link(c, recv, w);
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
   function-valued field calls its value. `pre` reuses a walk the
   caller already made. */
static const ek_type *call_member_walk(ck_ctx *c, const ek_node *e,
                                       const ek_node *callee, int implicit,
                                       const chain_recv *pre)
{
    ek_reading rd = EK_RD_NONE;
    const ek_type *head = NULL;
    int qualified = type_head(c, callee->u.field.recv, &head);
    chain_recv w;
    const ek_type *t;
    const ek_type *pt;

    if (qualified) {
        memset(&w, 0, sizeof w);
    } else if (pre) {
        w = *pre;
    } else {
        chain_receiver(c, callee->u.field.recv, &w);
    }
    {
        const ek_node *saved = c->inst_call;

        c->inst_call = e;
        t = ck_link_mod(c, callee, qualified ? head : w.recv,
                        callee->u.field.name,
                        qualified ? NULL : w.module, &rd);
        c->inst_call = saved;
    }
    pt = rd == EK_RD_FIELD ? ck_prune(t) : NULL;
    if (rd == EK_RD_DERIVED && !qualified
        && strcmp(callee->u.field.name, "with") == 0) {
        /* The with service takes its literal argument directly; its
           link type is only the expressible stand-in. */
        return call_with(c, e, w.recv);
    }
    if (rd != EK_RD_FIELD) {
        /* Namespace, module, derived, and opaque readings all take
           the receiver first; a type head supplies namespace context,
           not an argument. */
        return call_fn(c, e, t, callee->u.field.name, w.recv,
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

const ek_type *call_member(ck_ctx *c, const ek_node *e,
                                  const ek_node *callee, int implicit)
{
    return call_member_walk(c, e, callee, implicit, NULL);
}

/* The free-call form of a compiler service: its first argument is the
   receiver, exactly as the receiver form passes it, so the same member
   machinery selects the reading. A function-valued field is called
   without a receiver, matching the receiver form. Without an argument
   nothing supplies a receiver and the call stays opaque. */
const ek_type *call_service(ck_ctx *c, const ek_node *e,
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
    {
        const ek_node *saved = c->inst_call;

        c->inst_call = e;
        t = ck_link(c, callee, recv, name, &rd);
        c->inst_call = saved;
    }
    rest.data = e->u.call.args.data + 1;
    rest.count = e->u.call.args.count - 1;
    if (rd == EK_RD_FIELD) {
        /* The field's value is the callee; the receiver is not an
           argument. */
        if (t && ck_prune(t)->kind == EK_T_FN) {
            return call_fn(c, e, t, name, NULL, NULL, &rest, 0, implicit);
        }
        ck_error(c, e, "E-not-callable",
                 "the field `%s` is not a function value "
                 "(Clause 7.4.3)", name);
        return call_fn(c, e, NULL, name, NULL, NULL, &rest, 1, implicit);
    }
    return call_fn(c, e, t, name, recv, NULL, &rest, 0, implicit);
}

/* A qualified constructor applied as a call on its dotted head:
   the application is typed by the constructor, not by the opaque
   member reading. */
const ek_type *call_ctor(ck_ctx *c, const ek_node *e,
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
        c->invert_cb = ck_opaque(c);
    }
    if (callee && callee->kind == EK_N_NAME) {
        /* A head the resolver classified; a compiler service carries a
           receiver-form ref and takes the identity reading. */
        if (ck_ref_at(c, callee, EK_SYM_MEMBER, &ref)
            && ref->station == EK_ST_RECEIVER) {
            return call_service(c, e, callee, implicit);
        }
        {
            const ek_type *t = callee_expr(c, callee, e);

            return call_fn(c, e, t, NULL, NULL, NULL, &e->u.call.args, 0,
                           implicit);
        }
    }
    if (!callee || callee->kind != EK_N_FIELD) {
        const ek_type *t = callee_expr(c, callee, e);

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
    /* A constructor head is qualified; a receiver-form module path
       (`x.Mod.Ctor`) keeps its opaque reading. */
    if (ck_ref_at(c, callee, EK_SYM_CONSTRUCTOR, &ref) && ref->symbol
        && ref->station == EK_ST_QUALIFIED) {
        return call_ctor(c, e, callee, ref, expected, implicit);
    }
    if (ck_ref_at(c, callee, 0, &ref) && ref->symbol != NULL
        && (ref->kind == EK_SYM_FUNC || ref->kind == EK_SYM_NAMESPACE
            || ref->kind == EK_SYM_EXTERN)) {
        /* A qualified or module-path member: the receiver is the
           first argument. A record field of the same spelling wins a
           module segment of the chain, so the walk decides whether
           this reading stands. */
        const ek_type *head = NULL;
        chain_recv w;

        memset(&w, 0, sizeof w);
        if (!type_head(c, callee->u.field.recv, &head)) {
            chain_receiver(c, callee->u.field.recv, &w);
        }
        if (!w.shadowed) {
            const ek_type *t = callee_symbol_type(c, ref->symbol, e);

            return call_fn(c, e, t, callee->u.field.name, w.recv, NULL,
                           &e->u.call.args, 0, implicit);
        }
        return call_member_walk(c, e, callee, implicit, &w);
    }
    return call_member(c, e, callee, implicit);
}
