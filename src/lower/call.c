#include <string.h>

#include "lower/internal.h"

/* Calls: a callee that names a known function goes directly, a member
   call passes its receiver first, a call on a function value goes
   through the closure ABI, and a direct self-call in a tail position
   becomes the frame-local retry jump. Every argument evaluates to a
   temp in source order and releases once the call is emitted. */

/* The panic service: the free-call form of the String member, its
   reading selected by check. */
static int is_panic_call(lower_ctx *c, const ek_node *e)
{
    const ek_node *callee = e->u.call.callee;
    const ek_ck_row *row;

    if (!callee || callee->kind != EK_N_NAME || callee->u.path.count != 1
        || strcmp(callee->u.path.data[0], "panic") != 0) {
        return 0;
    }
    row = lower_row(c, callee);
    return row && row->reading == EK_RD_DERIVED;
}

static lower_val panic_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    const ek_nlist *args = &e->u.call.args;
    lower_val a;
    const char *at;

    if (args->count != 1) {
        lower_error(c, e, "the call of `panic` is malformed");
        return lower_val_unit();
    }
    a = lower_sub(c, scope, args->data[0]);
    at = lower_as_temp(c, scope, &a);
    lower_pad(c);
    lower_emitf(c, "ek_frame_site(%d, %d);\n", e->line, e->col);
    lower_emitf(c, "ek_panic_value(%s);\n", at);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", at);
    return lower_val_unit();
}


/* The C namespace's member call: the receiver names the namespace,
   not a module, and the machine-interface vocabulary routes to the
   runtime. A NULL result hands the decision back. */
lower_val c_ns_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_node *callee,
                           const call_form *form)
{
    const ek_node *recv = callee->u.field.recv;
    const ek_node *cnode = NULL;
    const ek_ref *ref = NULL;
    const char *name = NULL;
    const char *t;

    /* The C shapes only: the namespace itself and its Size type. The
       bridged reading lives on the `C` name; a Size segment between
       carries no ref of its own. Any other head falls through to the
       member paths (a machine-word head is one of those). */
    if (!recv) {
        return (lower_val){ NULL, 0 };
    }
    if (recv->kind == EK_N_NAME && recv->u.path.count == 1
        && strcmp(recv->u.path.data[0], "C") == 0) {
        cnode = recv;
    } else if (recv->kind == EK_N_FIELD && recv->u.field.recv
               && recv->u.field.recv->kind == EK_N_NAME
               && recv->u.field.recv->u.path.count == 1
               && strcmp(recv->u.field.recv->u.path.data[0], "C") == 0) {
        cnode = recv->u.field.recv;
    }
    if (!cnode) {
        return (lower_val){ NULL, 0 };
    }
    name = callee->u.field.name;
    ref = lower_ref(c, cnode);
    if (!ref || ref->symbol
        || (ref->kind != EK_SYM_EXTERNAL && ref->kind != EK_SYM_MODULE)) {
        return (lower_val){ NULL, 0 };
    }
    if (recv->kind == EK_N_NAME) {
        if (strcmp(name, "get_errno") == 0 && e->u.call.args.count == 0
            && !form->extra && !form->has_recv) {
            t = lower_temp(c, scope);
            lower_pad(c);
            lower_emitf(c, "ek_val %s = ek_val_int(ek_get_errno());\n", t);
            return (lower_val){ t, 1 };
        }
        lower_error(c, e, "the member `C.%s` has no lowering", name);
        return lower_discard_call(c, scope, e, form);
    }
    /* `C.Size.of(v)`: the ferry in; the value rides as the unsigned
       payload a size_t marshals from (Clause 10.3.5). */
    if (strcmp(name, "of") == 0
        && strcmp(recv->u.field.name, "Size") == 0
        && e->u.call.args.count == 1 && !form->extra && !form->has_recv) {
        lower_val a = lower_sub(c, scope, e->u.call.args.data[0]);
        const char *at = lower_as_temp(c, scope, &a);
        const char *r = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_val_uint((uint64_t)%s.as.i);\n",
                    r, at);
        return (lower_val){ r, 1 };
    }
    /* `C.Size.to_uint(v)`: the head form of the receiver reading; the
       conversion is total, the payload riding out as the unsigned
       word it marshals as. */
    if (strcmp(name, "to_uint") == 0
        && strcmp(recv->u.field.name, "Size") == 0
        && e->u.call.args.count == 1 && !form->extra && !form->has_recv) {
        lower_val a = lower_sub(c, scope, e->u.call.args.data[0]);
        const char *at = lower_as_temp(c, scope, &a);
        const char *r = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_val_uint((uint64_t)%s.as.i);\n",
                    r, at);
        return (lower_val){ r, 1 };
    }
    lower_error(c, e, "the member `C.%s` has no lowering", name);
    return lower_discard_call(c, scope, e, form);
}

/* The receiver of a module-path call (`x.Mod.f(a)`): the chain's
   module and type segments carry no value, so the walk peels them
   innermost first, mirroring check's chain walk that fixed the
   reading. NULL when no value segment remains: the plain qualified
   form. */
const ek_node *module_path_recv(lower_ctx *c, const ek_node *recv)
{
    const ek_ref *r;

    while (recv && recv->kind == EK_N_FIELD) {
        r = lower_ref(c, recv);
        if (!r || (r->kind != EK_SYM_MODULE
                   && !(r->kind == EK_SYM_TYPE
                        && r->station == EK_ST_MODULE_PATH))) {
            break;
        }
        recv = recv->u.field.recv;
    }
    if (recv == NULL) {
        return NULL;
    }
    if (recv->kind == EK_N_NAME && (r = lower_ref(c, recv)) != NULL
        && r->kind == EK_SYM_MODULE) {
        return NULL; /* the chain heads at a module: qualified */
    }
    return recv;
}

/* The qualified spelling of a constructor applied as a call: the
   payload is the call's argument, the family the apply form builds.
   A NULL result hands the decision back. */
static lower_val ctor_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_ref *ref,
                           const call_form *form)
{
    const ek_nlist *args = &e->u.call.args;
    lower_gtype *g;
    lower_val a;
    const char *at, *r;

    if (form->extra || form->has_recv || !ref->symbol) {
        return (lower_val){ NULL, 0 };
    }
    g = lower_ctor_of_symbol(c, e, ref->symbol, 1);
    if (!g) {
        return lower_val_unit();
    }
    a = args->count ? lower_sub(c, scope, args->data[0]) : lower_val_unit();
    at = lower_as_temp(c, scope, &a);
    r = lower_temp(c, scope);
    lower_pad(c);
    if (g->nf) {
        lower_emitf(c, "ek_val %s = %s(%s);\n", r, g->helper, at);
    } else {
        /* check diagnosed the arity; the argument still evaluates */
        lower_emitf(c, "ek_val %s = %s();\n", r, g->helper);
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", at);
    return (lower_val){ r, 1 };
}

/* A call whose callee names a function or namespace: the injected
   identity, the module-path receiver re-route, the frame-local retry
   of a self tail-call, or the direct form. */
static lower_val func_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_ref *ref,
                           const call_form *form)
{
    const ek_node *callee = e->u.call.callee;

    if (ref->kind == EK_SYM_FUNC && is_injected(ref->symbol)) {
        return identity_call(c, scope, e, ref->symbol, form);
    }
    if (ref->station == EK_ST_MODULE_PATH && !form->has_recv
        && callee && callee->kind == EK_N_FIELD) {
        const ek_node *rv = module_path_recv(c, callee->u.field.recv);

        if (rv) {
            call_form modpath = *form;

            /* `x.Mod.f(a)` passes the receiver first, exactly as
               the member form does */
            set_recv(c, scope, &modpath, rv, &e->u.call.args,
                     lower_row_type(c, callee));
            return direct_call(c, scope, e, ref->symbol, &modpath);
        }
    }
    if (!form->has_recv && !form->extra && c->in_tail && c->tce_self
        && c->tce_label && ref->symbol == c->tce_self) {
        lower_emit_self_tail(c, scope, e, ref);
        return (lower_val){ NULL, 0 }; /* the tail jumped to retry */
    }
    return direct_call(c, scope, e, ref->symbol, form);
}

/* The free-call forms of a derived service: its first argument is the
   receiver, exactly as the member form passes it; an open receiver the
   instantiation substituted concrete re-routes the same way. A NULL
   result hands the decision back. */
static lower_val free_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_node *callee,
                           const ek_ref *ref, const call_form *form)
{
    const ek_ck_row *nrow;
    lower_val v = { NULL, 0 };

    if (!callee || callee->kind != EK_N_NAME) {
        return v;
    }
    nrow = lower_row(c, callee);
    if (ref && ref->kind == EK_SYM_MEMBER && nrow
        && nrow->reading == EK_RD_NAMESPACE && !form->extra) {
        v = free_member_call(c, scope, e, callee, form);

        if (v.text) {
            return v;
        }
    }
    if (nrow && nrow->reading == EK_RD_DERIVED && !form->extra) {
        return lower_derived_call(c, scope, e, callee, nrow, CF_FREE);
    }
    if (nrow && nrow->reading == EK_RD_UNKNOWN && !form->extra
        && lower_open_call_result(c, e)) {
        return lower_derived_call(c, scope, e, callee, nrow, CF_FREE);
    }
    return v;
}

/* The member readings of a field callee: the C namespace's member
   routes to the runtime first, the namespace member scan second. A
   NULL result hands the decision back. */
static lower_val field_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_node *callee,
                            const call_form *form)
{
    lower_val v;

    if (!callee || callee->kind != EK_N_FIELD) {
        return (lower_val){ NULL, 0 };
    }
    v = c_ns_call(c, scope, e, callee, form);
    if (v.text) {
        return v;
    }
    return member_call(c, scope, e, callee, form);
}

/* One call: the callee's reading decides the form. */
static lower_val emit_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const call_form *form)
{
    const ek_node *callee = e->u.call.callee;
    const ek_ref *ref = lower_ref(c, e);
    lower_val v;

    if (!ref && callee) {
        ref = lower_ref(c, callee);
    }
    if (ref && ref->symbol && ref->kind == EK_SYM_EXTERN) {
        return lower_extern_call(c, scope, e, ref->symbol, form->extra);
    }
    if (ref && ref->symbol
        && (ref->kind == EK_SYM_FUNC || ref->kind == EK_SYM_NAMESPACE)) {
        return func_call(c, scope, e, ref, form);
    }
    if (ref && ref->symbol && ref->kind == EK_SYM_CONSTRUCTOR) {
        v = ctor_call(c, scope, e, ref, form);

        if (v.text) {
            return v;
        }
    }
    v = field_call(c, scope, e, callee, form);
    if (v.text) {
        return v;
    }
    v = free_call(c, scope, e, callee, ref, form);
    if (v.text) {
        return v;
    }
    {
        const ek_type *ct = callee ? lower_row_type(c, callee) : NULL;

        if (ct && ct->kind == EK_T_FN) {
            lower_val f = lower_sub(c, scope, callee);

            return value_call(c, scope, e, f, form->extra);
        }
    }
    if (ref) {
        lower_error(c, e, "the call is not supported by this lowerer");
        return lower_discard_call(c, scope, e, form);
    }
    return lower_val_unit(); /* resolve diagnosed the callee */
}

lower_val lower_call_expr(lower_ctx *c, lower_scope *scope,
                          const ek_node *e)
{
    call_form form;

    memset(&form, 0, sizeof form);
    if (is_panic_call(c, e)) {
        return panic_call(c, scope, e);
    }
    return emit_call(c, scope, e, &form);
}

/* The appended-argument form of a call: the `<-` chains pass their
   continuation as the last argument. */
lower_val lower_call_extra(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const char *extra)
{
    call_form form;

    memset(&form, 0, sizeof form);
    form.extra = extra;
    return emit_call(c, scope, e, &form);
}
