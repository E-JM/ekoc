#include <stdio.h>
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

/* An injected binding: a function row of the capability module with no
   declaration behind it. The compiler owns these rows -- the unwrap
   and wrap of each session bundle -- and no source form declares
   them. */
static int is_injected(const ek_symbol *sym)
{
    return sym && sym->kind == EK_SYM_FUNC && sym->module
        && strcmp(sym->module, "cap") == 0 && sym->decl == NULL;
}

/* The identity of one injected binding, defined once per row: the
   tiny static function returns its argument unchanged. */
static const char *identity_text(lower_ctx *c, const char *name)
{
    ek_buf b;

    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "static ek_val %s(ek_val v)\n{\n"
                           "    return v;\n}\n\n", name)) {
        c->oom = 1;
    }
    return lower_take(c, &b);
}

static const char *injected_identity(lower_ctx *c, const ek_symbol *sym)
{
    const char *name;
    lower_pending *p;
    lower_tramp *slots;
    size_t i;

    for (i = 0; i < c->nident; i++) {
        if (c->idents[i].sym == sym) {
            return c->idents[i].name;
        }
    }
    name = lower_str(c, "eki_%s", sym->name + 1);
    lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val v)", name));
    p = ek_arena_alloc(c->arena, sizeof *p);
    if (p) {
        memset(p, 0, sizeof *p);
        p->text = identity_text(c, name);
        lower_pending_add(c, p);
    } else {
        c->oom = 1;
    }
    slots = lower_grow(c, c->idents, &c->cident, c->nident + 1,
                       sizeof *slots);
    if (slots) {
        slots[c->nident].sym = sym;
        slots[c->nident].name = name;
        c->nident++;
        c->idents = slots;
    }
    return name;
}

/* The shape of one call site: an optional receiver argument and one
   appended trailing argument (the callback of a `<-` chain). */
typedef struct {
    int has_recv;
    lower_val recv;
    const char *extra;
} call_form;

/* Releases the argument temps, most recent first. */
static void release_args(lower_ctx *c, const char **argv, size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i]);
    }
}

/* The call line over the argument slots. */
static const char *emit_invoke(lower_ctx *c, lower_scope *scope,
                               const ek_symbol *sym, const char **argv,
                               size_t n)
{
    const char *t = lower_temp(c, scope);
    size_t i;

    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", t, lower_mangled(c, sym));
    for (i = 0; i < n; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
    }
    lower_emitf(c, ");\n");
    return t;
}

/* The receiver slot of a member call, in source order. */
static void fill_recv(lower_ctx *c, lower_scope *scope, const call_form *form,
                      const char **argv, size_t n)
{
    lower_val recv = form->recv;
    const char *t;

    if (n) {
        argv[0] = lower_as_temp(c, scope, &recv);
        return;
    }
    t = lower_as_temp(c, scope, &recv); /* a malformed arity */
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
}

/* The written arguments, the appended callback, and the arity pads. */
static void fill_written(lower_ctx *c, lower_scope *scope,
                         const ek_nlist *args, const call_form *form,
                         const char **argv, size_t n, size_t first)
{
    size_t written_end = first + args->count;
    int extra_fits = form->extra && n > written_end;
    size_t i;

    for (i = first; i < n; i++) {
        lower_val a;

        if (i < written_end) {
            a = lower_sub(c, scope, args->data[i - first]);
        } else if (extra_fits && i == n - 1) {
            argv[i] = form->extra; /* the callback moves in */
            continue;
        } else {
            a = lower_val_unit(); /* a malformed arity check diagnosed */
        }
        argv[i] = lower_as_temp(c, scope, &a);
    }
    for (i = n; i < written_end; i++) {
        lower_val a = lower_sub(c, scope, args->data[i - first]);
        const char *t = lower_as_temp(c, scope, &a);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    if (form->extra && !extra_fits) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", form->extra);
    }
}

/* A direct call to a known function: the receiver of a member form is
   the first argument, the appended callback the last. */
static lower_val direct_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_symbol *sym,
                             const call_form *form)
{
    const ek_node *ann = sym->type;
    size_t n = ann && ann->kind == EK_N_T_FN ? ann->u.t_fn.params.count : 0;
    size_t first = form->has_recv ? 1 : 0;
    const char **argv = NULL;
    size_t cap = 0;
    const char *t;

    if (n) {
        argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
        if (!argv) {
            c->oom = 1;
            return lower_val_unit();
        }
    }
    if (form->has_recv) {
        fill_recv(c, scope, form, argv, n);
    }
    fill_written(c, scope, &e->u.call.args, form, argv, n, first);
    lower_enqueue(c, sym);
    t = emit_invoke(c, scope, sym, argv, n);
    release_args(c, argv, n);
    return (lower_val){ t, 1 };
}

/* A call on a function value: the closure ABI over the argument
   array. */
static lower_val value_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, lower_val callee,
                            const char *extra)
{
    const ek_nlist *args = &e->u.call.args;
    size_t n = args->count + (extra ? 1 : 0);
    const char **argv = NULL;
    size_t cap = 0, i, k = 0;
    const char *ft = lower_as_temp(c, scope, &callee);
    const char *av = NULL;
    const char *r = lower_temp(c, scope);

    if (n) {
        argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
        if (!argv) {
            c->oom = 1;
            return lower_val_unit();
        }
    }
    for (i = 0; i < args->count; i++) {
        lower_val a = lower_sub(c, scope, args->data[i]);

        argv[k++] = lower_as_temp(c, scope, &a);
    }
    if (extra) {
        argv[k++] = extra;
    }
    if (n) {
        av = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s[] = {", av);
        for (i = 0; i < n; i++) {
            lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
        }
        lower_emitf(c, "};\n");
    }
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_closure_call(%s, %s, %zu);\n", r, ft,
                n ? av : "(ek_val *)0", n);
    release_args(c, argv, n);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", ft);
    return (lower_val){ r, 1 };
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
    int has_recv = 0;
    lower_val v;

    if (!frow || frow->reading != EK_RD_NAMESPACE || form->extra) {
        return (lower_val){ NULL, 0 };
    }
    if (rrow && rrow->type) {
        family = lower_intrinsic_family(rrow->type);
        has_recv = 1;
    } else if (head && head->symbol && head->symbol->module
               && (strcmp(head->symbol->module, "std") == 0
                   || strcmp(head->symbol->module, "cap") == 0)) {
        family = head->symbol->name;
    }
    if (!family) {
        return (lower_val){ NULL, 0 };
    }
    v = lower_intrinsic_call(c, scope, e, callee, family, has_recv);
    return v.text ? v : (lower_val){ NULL, 0 };
}

/* A receiver-form member call: a function-valued field is the callee
   value, a derived reading lowers to its service, and any other link
   names a std or declared member found the way check finds it. A NULL
   result hands the decision back. */
static lower_val member_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const call_form *form)
{
    const ek_ck_row *frow = lower_row(c, callee);
    const ek_node *recv = callee->u.field.recv;
    const ek_ck_row *rrow = recv ? lower_row(c, recv) : NULL;
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
                                  rrow && rrow->type ? CF_MEMBER
                                                     : CF_QUALIFIED);
    }
    if (rrow && rrow->type) {
        ms = member_symbol(c, rrow->type, callee->u.field.name);
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
        if (rrow && rrow->type) {
            call_form member = *form;

            member.has_recv = 1;
            member.recv = lower_sub(c, scope, recv);
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
        if (!form->has_recv && !form->extra && ref->kind == EK_SYM_FUNC
            && c->in_tail && c->tce_self && c->tce_label
            && ref->symbol == c->tce_self) {
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
           is the receiver, exactly as the member form passes it */
        const ek_ck_row *nrow = lower_row(c, callee);

        if (nrow && nrow->reading == EK_RD_DERIVED && !form->extra) {
            return lower_derived_call(c, scope, e, callee, nrow, CF_FREE);
        }
    }
    {
        const ek_ck_row *crow = callee ? lower_row(c, callee) : NULL;

        if (crow && crow->type && crow->type->kind == EK_T_FN) {
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

