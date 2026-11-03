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
   appended trailing argument (the callback of a `<-` chain). The
   receiver borrows the frame local it reads when the written
   arguments cannot rebind it, and lowers only when it keeps. */
typedef struct {
    int has_recv;
    lower_val recv;
    const char *recv_borrow;
    const char *extra;
} call_form;

/* The frame-owned local the argument expression reads, when the call
   may pass it borrowed: a name read of a binding the emitting frame
   owns -- a capture-environment read never borrows, the scope does
   not own it -- and the same reading through an injected identity
   call, whose result is its argument. NULL keeps the kept-temp
   shape. */
const char *lower_borrow_local(lower_ctx *c, lower_scope *scope,
                               const ek_node *e)
{
    const ek_ref *ref;
    const lower_local *loc;

    if (!e) {
        return NULL;
    }
    if (e->kind == EK_N_CALL) {
        const ek_node *callee = e->u.call.callee;
        const ek_node *arg = NULL;

        ref = lower_ref(c, e);
        if (!ref && callee) {
            ref = lower_ref(c, callee);
        }
        if (!ref || ref->kind != EK_SYM_FUNC
            || !is_injected(ref->symbol)) {
            return NULL;
        }
        if (callee && callee->kind == EK_N_FIELD
            && e->u.call.args.count == 0) {
            arg = callee->u.field.recv; /* the member form */
        } else if (e->u.call.args.count == 1
                   && (!callee || callee->kind != EK_N_FIELD)) {
            arg = e->u.call.args.data[0]; /* the free-call form */
        }
        return arg ? lower_borrow_local(c, scope, arg) : NULL;
    }
    if (e->kind == EK_N_NAME && e->u.path.count == 1
        && strcmp(e->u.path.data[0], "_") != 0) {
        ref = lower_ref(c, e);
        if (ref && (ref->kind == EK_SYM_LOCAL || ref->kind == EK_SYM_PARAM)
            && ref->symbol) {
            loc = scope_find_local(scope, ref->symbol->name);
            if (loc && !loc->is_param) {
                return loc->cname;
            }
        }
    }
    return NULL;
}

/* The kid lists one argument kind carries: any element may rebind. */
static int rebind_kids(ek_node *const *data, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (lower_arg_may_rebind(data[i])) {
            return 1;
        }
    }
    return 0;
}

/* True when evaluating the expression can rebind a local of the
   emitting frame: the block forms and the chain form run statements
   of an enclosing scope, and any of them may carry a rebind. A
   lambda's body runs later and never rebinds a capture, so the walk
   stops there. */
int lower_arg_may_rebind(const ek_node *e)
{
    size_t i;

    if (!e) {
        return 0;
    }
    switch (e->kind) {
    case EK_N_REBIND:
        return 1;
    case EK_N_BLOCK:
    case EK_N_IF:
    case EK_N_CASE:
    case EK_N_INVERT:
        return 1; /* the statements of an enclosing scope may rebind */
    case EK_N_LAMBDA:
        return 0; /* the body runs later */
    case EK_N_PAREN:
    case EK_N_UNARY:
        return lower_arg_may_rebind(e->u.wrap.inner);
    case EK_N_BINARY:
        return lower_arg_may_rebind(e->u.binary.lhs)
            || lower_arg_may_rebind(e->u.binary.rhs);
    case EK_N_FIELD:
        return lower_arg_may_rebind(e->u.field.recv);
    case EK_N_CALL:
        return lower_arg_may_rebind(e->u.call.callee)
            || rebind_kids(e->u.call.args.data, e->u.call.args.count);
    case EK_N_APPLY:
        return lower_arg_may_rebind(e->u.pair.a)
            || lower_arg_may_rebind(e->u.pair.b);
    case EK_N_COMPOSITE:
        return rebind_kids(e->u.kids.data, e->u.kids.count);
    case EK_N_COMPOSITE_ITEM:
        return lower_arg_may_rebind(e->u.pair.b);
    case EK_N_STRING_LIT:
        for (i = 0; i < e->u.str.count; i++) {
            if (e->u.str.segs[i].expr
                && lower_arg_may_rebind(e->u.str.segs[i].expr)) {
                return 1;
            }
        }
        return 0;
    default:
        return 0;
    }
}

/* True when any written argument from index i on rebinds a local of
   the emitting frame when evaluated: a borrow taken before it would
   read the rebound value at the call. */
static int args_rebind_from(const ek_nlist *args, size_t i)
{
    size_t k;

    for (k = i; k < args->count; k++) {
        if (lower_arg_may_rebind(args->data[k])) {
            return 1;
        }
    }
    return 0;
}

/* The parameter type of one written position: the arrow's parameter,
   NULL past it or where no row carries the arrow. */
static const ek_type *fn_param(const ek_type *ft, size_t i)
{
    return ft && ft->kind == EK_T_FN && i < ft->u.fn.count
        ? ft->u.fn.params[i] : NULL;
}

/* The receiver slot of a member call, decided before the receiver
   lowers: the frame local it reads borrows, and the receiver lowers
   only when it keeps. A receiver whose parameter is an anonymous
   record adapts -- the adaptation rebuilds the value, so the read is
   no longer a bare local read and travels as an owned temp. ft is the
   callee's function type: its first parameter is the receiver's
   expected type, exactly the type the argument positions adapt
   against. */
static void set_recv(lower_ctx *c, lower_scope *scope, call_form *member,
                     const ek_node *recv, const ek_nlist *args,
                     const ek_type *ft)
{
    const ek_type *pt = fn_param(ft, 0);

    member->has_recv = 1;
    if (!pt || pt->kind != EK_T_RECORD) {
        member->recv_borrow = lower_borrow_local(c, scope, recv);
        if (member->recv_borrow && args_rebind_from(args, 0)) {
            member->recv_borrow = NULL;
        }
    }
    if (member->recv_borrow) {
        member->recv = lower_val_unit(); /* the read is pure */
        return;
    }
    member->recv = lower_sub(c, scope, recv);
    if (pt) {
        member->recv = lower_shape_adapt(c, scope, member->recv, recv,
                                         lower_row_type(c, recv), pt);
    }
}

/* Releases the argument temps, most recent first; a borrowed
   argument holds no reference and releases nothing. */
static void release_args(lower_ctx *c, const char **argv, const int *owned,
                         size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        if (!owned[i]) {
            continue;
        }
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i]);
    }
}

/* The call line over the argument slots. */
static const char *emit_invoke(lower_ctx *c, lower_scope *scope,
                               const char *name, const char **argv,
                               size_t n)
{
    const char *t = lower_temp(c, scope);
    size_t i;

    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", t, name);
    for (i = 0; i < n; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
    }
    lower_emitf(c, ");\n");
    return t;
}

/* The receiver slot of a member call, in source order. */
static void fill_recv(lower_ctx *c, lower_scope *scope, const call_form *form,
                      const char **argv, int *owned, size_t n)
{
    lower_val recv = form->recv;

    if (n) {
        if (form->recv_borrow) {
            argv[0] = form->recv_borrow;
            owned[0] = 0;
            return;
        }
        argv[0] = lower_as_temp(c, scope, &recv);
        owned[0] = 1;
        return;
    }
    /* a malformed arity: a borrowed read is pure and dies with
       nothing, a kept receiver still evaluates and dies */
    if (!form->recv_borrow) {
        const char *t = lower_as_temp(c, scope, &recv);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
}

/* The borrowed reading of one written argument: the frame local it
   names, when the position's parameter is not an anonymous record --
   the shape adaptation rebuilds the value, so the position keeps the
   temp -- and no later argument's evaluation can rebind it. */
static const char *arg_borrow(lower_ctx *c, lower_scope *scope,
                              const ek_nlist *args, size_t i,
                              const ek_type *pt)
{
    const char *bn;

    if (pt && pt->kind == EK_T_RECORD) {
        return NULL;
    }
    bn = lower_borrow_local(c, scope, args->data[i]);
    if (bn && args_rebind_from(args, i + 1)) {
        return NULL;
    }
    return bn;
}

/* The written arguments, the appended callback, and the arity pads.
   An argument reading a frame-owned local passes the local's own
   name -- the caller's frame keeps it alive across the whole call --
   unless a later argument's evaluation can rebind it, or the callee's
   type adapts the argument's shape. ft is the callee's function type
   when a row carries it: a nominal record argument adapts into an
   anonymous record parameter. */
static void fill_written(lower_ctx *c, lower_scope *scope,
                         const ek_nlist *args, const call_form *form,
                         const char **argv, int *owned, size_t n, size_t first,
                         const ek_type *ft)
{
    size_t written_end = first + args->count;
    int extra_fits = form->extra && n > written_end;
    size_t i;

    for (i = first; i < n; i++) {
        lower_val a;

        if (i < written_end) {
            const ek_node *an = args->data[i - first];
            const ek_type *pt = fn_param(ft, i);
            const char *bn = arg_borrow(c, scope, args, i - first, pt);

            if (bn) {
                argv[i] = bn;
                owned[i] = 0;
                continue;
            }
            a = lower_sub(c, scope, an);
            if (pt) {
                a = lower_shape_adapt(c, scope, a, an, lower_row_type(c, an),
                                      pt);
            }
        } else if (extra_fits && i == n - 1) {
            argv[i] = form->extra; /* the callback moves in */
            owned[i] = 1;
            continue;
        } else {
            a = lower_val_unit(); /* a malformed arity check diagnosed */
        }
        argv[i] = lower_as_temp(c, scope, &a);
        owned[i] = 1;
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

/* The applied instantiation of a generic callee: the callee row's
   function type under the substitution of the body being emitted,
   when the callee's own annotation carries rigid variables and the
   row answers with a concrete function of the same shape. NULL keeps
   the plain emission. */
static const ek_type *callee_inst(lower_ctx *c, const ek_node *e,
                                  const ek_symbol *sym, size_t nparams)
{
    const ek_ck_row *row;
    const ek_type *f;

    if (!nparams || !sym->type) {
        return NULL;
    }
    row = lower_row(c, sym->type);
    if (!row || !row->type || !lower_type_has_var(row->type)) {
        return NULL;
    }
    f = lower_row_type(c, e->u.call.callee);
    if (!f || f->kind != EK_T_FN || f->u.fn.count != nparams
        || lower_type_has_ivar(f)) {
        return NULL;
    }
    return f;
}

/* A direct call to a known function: the receiver of a member form is
   the first argument, the appended callback the last. A generic
   callee instantiates at the applied type its row carries, keyed by
   the applied arguments, one per binder. */
static lower_val direct_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_symbol *sym,
                             const call_form *form)
{
    const ek_node *ann = sym->type;
    size_t n = ann && ann->kind == EK_N_T_FN ? ann->u.t_fn.params.count : 0;
    size_t first = form->has_recv ? 1 : 0;
    const ek_type *inst = callee_inst(c, e, sym, n);
    const ek_type **iargs = NULL;
    size_t ni = 0;
    const char **argv = NULL;
    int *owned = NULL;
    size_t cap = 0, ocap = 0;
    const char *t;

    if (inst) {
        ni = lower_inst_args(c, sym, inst, &iargs);
        if (!ni) {
            inst = NULL;
        }
    }
    if (n) {
        argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
        owned = lower_grow(c, NULL, &ocap, n, sizeof *owned);
        if (!argv || !owned) {
            c->oom = 1;
            return lower_val_unit();
        }
    }
    if (form->has_recv) {
        fill_recv(c, scope, form, argv, owned, n);
    }
    fill_written(c, scope, &e->u.call.args, form, argv, owned, n, first,
                 inst ? inst : lower_row_type(c, e->u.call.callee));
    if (inst) {
        lower_enqueue_item(c, sym, iargs, ni, inst);
        t = emit_invoke(c, scope, lower_inst_name(c, sym, iargs, ni),
                        argv, n);
    } else {
        lower_enqueue(c, sym);
        t = emit_invoke(c, scope, lower_mangled(c, sym), argv, n);
    }
    release_args(c, argv, owned, n);
    return (lower_val){ t, 1 };
}

/* A call on a function value: the closure ABI over the argument
   array. The entries are borrowed -- the runtime callee retains
   whatever it stores -- so an argument reading a frame-owned local
   passes the local's own name. */
static lower_val value_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, lower_val callee,
                            const char *extra)
{
    const ek_nlist *args = &e->u.call.args;
    size_t n = args->count + (extra ? 1 : 0);
    const char **argv = NULL;
    int *owned = NULL;
    size_t cap = 0, ocap = 0, i, k = 0;
    const char *ft = lower_as_temp(c, scope, &callee);
    const char *av = NULL;
    const char *r = lower_temp(c, scope);

    if (n) {
        argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
        owned = lower_grow(c, NULL, &ocap, n, sizeof *owned);
        if (!argv || !owned) {
            c->oom = 1;
            return lower_val_unit();
        }
    }
    for (i = 0; i < args->count; i++) {
        const char *bn = lower_borrow_local(c, scope, args->data[i]);

        if (bn && args_rebind_from(args, i + 1)) {
            bn = NULL;
        }
        if (bn) {
            argv[k] = bn;
            owned[k] = 0;
        } else {
            lower_val a = lower_sub(c, scope, args->data[i]);

            argv[k] = lower_as_temp(c, scope, &a);
            owned[k] = 1;
        }
        k++;
    }
    if (extra) {
        argv[k] = extra;
        owned[k] = 1;
        k++;
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
    release_args(c, argv, owned, n);
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

