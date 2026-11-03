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
    lower_emitf(c, "ek_panic_value(%s);\n", at);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", at);
    return lower_val_unit();
}


/* The scan behind the member lookup: the member of one namespace
   owner, in the owning module's surface, with the capability module's
   surface as the fallback beneath it -- the namespaces of the
   capability types live there, while their type names stay std
   placeholder rows. */
static const ek_symbol *member_scan(lower_ctx *c, const char *owner,
                                    const char *mod, const ek_symbol *tsym,
                                    const char *name)
{
    int pass;

    for (pass = 0; pass < 2; pass++) {
        const char *want = pass == 0 ? mod : "cap";
        size_t k, i;

        if (pass == 1 && (tsym ? tsym->decl != NULL
                               : strcmp(mod, "std") != 0)) {
            return NULL;
        }
        for (k = 0; k < c->resolved.module_count; k++) {
            const ek_surface *s = &c->resolved.modules[k];

            if (strcmp(s->module, want) != 0) {
                continue;
            }
            for (i = 0; i < s->count; i++) {
                const ek_symbol *m = &s->syms[i];

                if (m->kind == EK_SYM_NAMESPACE && m->owner
                    && strcmp(m->owner, owner) == 0
                    && strcmp(m->name, name) == 0) {
                    return m;
                }
            }
        }
    }
    return NULL;
}

/* The namespace member a receiver-form link denotes, found in the
   declaring module's surface the way check finds it. */
static const ek_symbol *member_symbol(lower_ctx *c, const ek_type *recv,
                                      const char *name)
{
    const char *owner = NULL;
    const char *mod = NULL;
    const ek_symbol *tsym = NULL;

    if (recv && recv->kind == EK_T_NOMINAL && recv->u.nominal.sym) {
        tsym = recv->u.nominal.sym;
        owner = tsym->name;
        mod = tsym->module;
    } else if (recv && recv->kind == EK_T_LIST) {
        owner = "List";
        mod = "std";
    } else if (recv && recv->kind == EK_T_DICT) {
        owner = "Dict";
        mod = "std";
    }
    if (!owner) {
        return NULL;
    }
    return member_scan(c, owner, mod, tsym, name);
}

/* The declared member of one receiver type's namespace, the way the
   member-call path finds it; the display slots consult it before the
   derived services, since a declared `show` outranks the derived one. */
const ek_symbol *lower_member_symbol(lower_ctx *c, const ek_type *recv,
                                     const char *name)
{
    return member_symbol(c, recv, name);
}

/* The unsupported-call balance: the receiver and the arguments still
   evaluate and die. */
static lower_val discard_call(lower_ctx *c, lower_scope *scope,
                              const ek_node *e, const call_form *form)
{
    const ek_nlist *args = &e->u.call.args;
    lower_val a;
    const char *t;
    size_t i;

    if (form->has_recv) {
        a = lower_sub(c, scope, e->u.call.callee->u.field.recv);
        t = lower_as_temp(c, scope, &a);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    for (i = 0; i < args->count; i++) {
        a = lower_sub(c, scope, args->data[i]);
        t = lower_as_temp(c, scope, &a);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    if (form->extra) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", form->extra);
    }
    return lower_val_unit();
}

/* The call of an injected binding: the identity receives the argument
   and the result carries the same value, so the argument temp's
   reference moves into the result and no release runs on it. The
   rows are leaves: reachability does not descend into them. */
static lower_val identity_call(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_symbol *sym,
                               const call_form *form)
{
    const ek_nlist *args = &e->u.call.args;
    lower_val a;
    const char *at, *t;

    if (form->extra || args->count != 1) {
        lower_error(c, e, "the call of `%s` is malformed", sym->name);
        return discard_call(c, scope, e, form);
    }
    a = lower_sub(c, scope, args->data[0]);
    at = lower_as_temp(c, scope, &a);
    t = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(%s);\n", t, injected_identity(c, sym),
                at);
    return (lower_val){ t, 1 };
}

/* The intrinsic dispatch of a member call the std source does not
   declare: the reading check recorded gates it, and the family comes
   from the receiver's type or from the qualified head's type symbol.
   A NULL result hands the decision back. */
static lower_val intrinsic_member(lower_ctx *c, lower_scope *scope,
                                  const ek_node *e, const ek_node *callee,
                                  const ek_ck_row *frow,
                                  const ek_ck_row *rrow, const ek_ref *head,
                                  const call_form *form)
{
    const char *family = NULL;
    const ek_type *rt = rrow ? lower_type_in(c, rrow->type) : NULL;
    int has_recv = 0;
    lower_val v;

    if (!frow || frow->reading != EK_RD_NAMESPACE || form->extra) {
        return (lower_val){ NULL, 0 };
    }
    if (rt) {
        family = lower_intrinsic_family(rt);
        has_recv = 1;
    } else if (head && head->symbol && head->symbol->module
               && (strcmp(head->symbol->module, "std") == 0
                   || strcmp(head->symbol->module, "sys") == 0
                   || strcmp(head->symbol->module, "cap") == 0)) {
        family = head->symbol->name;
    }
    if (!family) {
        return (lower_val){ NULL, 0 };
    }
    v = lower_intrinsic_call(c, scope, e, callee, family, has_recv);
    return v.text ? v : (lower_val){ NULL, 0 };
}

/* The re-routed call at an open receiver the instantiation substituted
   concrete: the site means what the same member over the applied type
   means, so the declared namespace member wins, then the compiler
   rows, then the derived services, in the order check reads them. */
static lower_val open_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_node *callee,
                           const ek_type *rs, const call_form *form)
{
    const char *name = callee->u.field.name;
    const ek_symbol *ms = member_symbol(c, rs, name);
    const ek_ck_row *row = lower_row(c, callee);

    if (ms) {
        call_form member = *form;

        set_recv(c, scope, &member, callee->u.field.recv,
                 &e->u.call.args, lower_row_type(c, callee));
        return direct_call(c, scope, e, ms, &member);
    }
    if (strcmp(name, "with") == 0) {
        return lower_derived_with(c, scope, e, callee->u.field.recv);
    }
    if (row) {
        ek_ck_row ns = *row;
        ek_ck_row recv_row = *row;
        lower_val v;

        ns.reading = EK_RD_NAMESPACE;
        recv_row.type = rs;
        v = intrinsic_member(c, scope, e, callee, &ns, &recv_row, NULL,
                             form);
        if (v.text) {
            return v;
        }
    }
    return lower_derived_call(c, scope, e, callee, row, CF_MEMBER);
}

/* The result type of a derived-service call at a receiver the
   instantiation substituted concrete: the fixed result of the service,
   the receiver itself for `with`. NULL when the site is not such a
   call. */
const ek_type *lower_open_call_result(lower_ctx *c, const ek_node *e)
{
    const ek_node *callee;
    const char *name = NULL;
    const ek_node *recv = NULL;
    lower_dsvc_kind kind;
    const ek_type *rs;

    if (!e || e->kind != EK_N_CALL) {
        return NULL;
    }
    callee = e->u.call.callee;
    if (callee && callee->kind == EK_N_FIELD) {
        name = callee->u.field.name;
        recv = callee->u.field.recv;
    } else if (callee && callee->kind == EK_N_NAME
               && callee->u.path.count == 1) {
        name = callee->u.path.data[0];
        recv = e->u.call.args.count ? e->u.call.args.data[0] : NULL;
    }
    if (!name || !recv) {
        return NULL;
    }
    rs = lower_row_type(c, recv);
    if (!rs || lower_type_has_var(rs)) {
        return NULL;
    }
    if (lower_service_kind(name, &kind)) {
        switch (kind) {
        case DSV_EQ:
        case DSV_ORD:
            return lower_std_nominal(c, "Bool");
        case DSV_SHOW:
        case DSV_INSPECT:
            return lower_std_nominal(c, "String");
        case DSV_TOJSON:
            return lower_std_nominal(c, "Json");
        default:
            return NULL;
        }
    }
    return strcmp(name, "with") == 0 ? rs : NULL;
}

/* The free-call form of a declared namespace member: `m(recv, rest)`
   means `recv.m(rest)`, with the link's reading the receiver form's.
   A NULL result hands the decision back. */
static lower_val free_member_call(lower_ctx *c, lower_scope *scope,
                                  const ek_node *e, const ek_node *callee,
                                  const call_form *form)
{
    const ek_nlist *args = &e->u.call.args;
    const ek_node *recv = args->count ? args->data[0] : NULL;
    const ek_type *rt = recv ? lower_row_type(c, recv) : NULL;
    const ek_symbol *ms;
    ek_nlist rest;
    ek_node site;
    call_form member;

    if (!recv || !rt || form->extra) {
        return (lower_val){ NULL, 0 };
    }
    ms = member_symbol(c, rt, callee->u.path.data[0]);
    if (!ms) {
        return (lower_val){ NULL, 0 };
    }
    rest.data = args->data + 1;
    rest.count = args->count - 1;
    site = *e;
    site.u.call.args = rest;
    member = *form;
    set_recv(c, scope, &member, recv, &rest, lower_row_type(c, callee));
    return direct_call(c, scope, &site, ms, &member);
}

/* The current-module free function a receiver-form link selected: the
   function of the name in the module whose body is being lowered, the
   same module check searched. */
static const ek_symbol *root_module_fn(lower_ctx *c, const char *name)
{
    const ek_surface *owner = NULL;
    size_t i;

    for (i = 0; i < c->resolved.module_count; i++) {
        const ek_surface *s = &c->resolved.modules[i];

        if (s->file == c->file || strcmp(s->file, c->file) == 0) {
            owner = s;
            break;
        }
    }
    if (!owner) {
        return NULL;
    }
    for (i = 0; i < owner->count; i++) {
        const ek_symbol *m = &owner->syms[i];

        if (m->kind == EK_SYM_FUNC && strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

/* A receiver-form member call: a function-valued field is the callee
   value, a derived reading lowers to its service, an open receiver the
   instantiation substituted concrete re-routes at the applied type,
   and any other link names a std or declared member found the way
   check finds it. A NULL result hands the decision back. */
static lower_val member_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const call_form *form)
{
    const ek_ck_row *frow = lower_row(c, callee);
    const ek_node *recv = callee->u.field.recv;
    const ek_ck_row *rrow = recv ? lower_row(c, recv) : NULL;
    const ek_type *rt = rrow ? lower_type_in(c, rrow->type) : NULL;
    const ek_ref *head = NULL;
    const ek_symbol *ms;

    if (frow && frow->reading == EK_RD_FIELD) {
        lower_val f = lower_sub(c, scope, callee);

        return value_call(c, scope, e, f, form->extra);
    }
    if (frow && frow->reading == EK_RD_DERIVED && !form->extra) {
        if (strcmp(callee->u.field.name, "with") == 0) {
            return lower_derived_with(c, scope, e, recv);
        }
        return lower_derived_call(c, scope, e, callee, frow,
                                  rt ? CF_MEMBER : CF_QUALIFIED);
    }
    if (frow && frow->reading == EK_RD_MODULE_FN && recv && !form->extra) {
        const ek_symbol *fn = root_module_fn(c, callee->u.field.name);

        if (fn) {
            call_form member = *form;

            set_recv(c, scope, &member, recv, &e->u.call.args,
                     lower_row_type(c, callee));
            return direct_call(c, scope, e, fn, &member);
        }
    }
    if (frow && frow->reading == EK_RD_UNKNOWN && !form->extra && rt) {
        lower_val v = open_call(c, scope, e, callee, rt, form);

        if (v.text) {
            return v;
        }
    }
    if (rt) {
        ms = member_symbol(c, rt, callee->u.field.name);
    } else {
        /* a qualified head names its type: the receiver node carries
           no value row, and the type ref names the namespace owner */
        head = recv ? lower_ref(c, recv) : NULL;
        ms = head && head->kind == EK_SYM_TYPE && head->symbol
                 && head->station == EK_ST_QUALIFIED
                 ? member_scan(c, head->symbol->name, head->symbol->module,
                               head->symbol, callee->u.field.name)
                 : NULL;
    }
    if (ms) {
        if (rt) {
            call_form member = *form;

            set_recv(c, scope, &member, recv, &e->u.call.args,
                     lower_row_type(c, callee));
            return direct_call(c, scope, e, ms, &member);
        }
        /* the head supplies namespace context, not a receiver value:
           the written arguments fill the signature's slots, receiver
           first */
        return direct_call(c, scope, e, ms, form);
    }
    /* a compiler row of the std namespaces: the reading check
       recorded selects the runtime dispatch */
    return intrinsic_member(c, scope, e, callee, frow, rrow, head, form);
}

/* The C namespace's member call: the receiver names the namespace,
   not a module, and the machine-interface vocabulary routes to the
   runtime. A NULL result hands the decision back. */
static lower_val c_ns_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const ek_node *callee,
                           const call_form *form)
{
    const ek_node *recv = callee->u.field.recv;
    const ek_ref *ref = recv ? lower_ref(c, recv) : NULL;
    const char *name;
    const char *t;

    /* The namespace head reads as a bridged external or as a module
       without a surface; either way it names no value. */
    if (!ref || ref->symbol || !recv || recv->kind != EK_N_NAME
        || recv->u.path.count != 1
        || strcmp(recv->u.path.data[0], "C") != 0
        || (ref->kind != EK_SYM_EXTERNAL
            && ref->kind != EK_SYM_MODULE)) {
        return (lower_val){ NULL, 0 };
    }
    name = callee->u.field.name;
    if (strcmp(name, "get_errno") == 0 && e->u.call.args.count == 0
        && !form->extra && !form->has_recv) {
        t = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_val_int(ek_get_errno());\n", t);
        return (lower_val){ t, 1 };
    }
    lower_error(c, e, "the member `C.%s` has no lowering", name);
    return discard_call(c, scope, e, form);
}

/* The receiver of a module-path call (`x.Mod.f(a)`): the chain's
   module and type segments carry no value, so the walk peels them
   innermost first, mirroring check's chain walk that fixed the
   reading. NULL when no value segment remains: the plain qualified
   form. */
static const ek_node *module_path_recv(lower_ctx *c, const ek_node *recv)
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

/* One call: the callee's reading decides the form. */
static lower_val emit_call(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const call_form *form)
{
    const ek_node *callee = e->u.call.callee;
    const ek_ref *ref = lower_ref(c, e);

    if (!ref && callee) {
        ref = lower_ref(c, callee);
    }
    if (ref && ref->symbol && ref->kind == EK_SYM_EXTERN) {
        return lower_extern_call(c, scope, e, ref->symbol, form->extra);
    }
    if (ref && ref->symbol
        && (ref->kind == EK_SYM_FUNC || ref->kind == EK_SYM_NAMESPACE)) {
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
    if (callee && callee->kind == EK_N_FIELD) {
        lower_val v = c_ns_call(c, scope, e, callee, form);

        if (v.text) {
            return v;
        }
        v = member_call(c, scope, e, callee, form);

        if (v.text) {
            return v;
        }
    }
    if (callee && callee->kind == EK_N_NAME) {
        /* the free-call form of a derived service: its first argument
           is the receiver, exactly as the member form passes it; an
           open receiver the instantiation substituted concrete
           re-routes the same way */
        const ek_ck_row *nrow = lower_row(c, callee);

        if (ref && ref->kind == EK_SYM_MEMBER && nrow
            && nrow->reading == EK_RD_NAMESPACE && !form->extra) {
            lower_val v = free_member_call(c, scope, e, callee, form);

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
        return discard_call(c, scope, e, form);
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

