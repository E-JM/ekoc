#include "lower/internal.h"

/* The invocations over the filled argument slots: the direct call of a
   known function -- a generic callee instantiates at the applied type
   its row carries -- and the closure call of a function value. */

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
lower_val direct_call(lower_ctx *c, lower_scope *scope,
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
    if (sym->decl) {
        /* The caller marks its frame with the call site, so a panic
           below points at the line that asked for it (Clause B.6). */
        lower_emitf(c, "    ek_frame_site(%d, %d);\n", e->line, e->col);
    }
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

/* The written arguments and the appended callback, borrowed where the
   read names a frame-owned local no later argument rebinds. */
static void fill_values(lower_ctx *c, lower_scope *scope,
                        const ek_nlist *args, const char *extra,
                        const char **argv, int *owned)
{
    size_t i, k = 0;

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
    }
}

/* The C argument array of the closure ABI, over the filled slots;
   NULL at an empty call. */
static const char *emit_arg_array(lower_ctx *c, lower_scope *scope,
                                  const char **argv, size_t n)
{
    const char *av = NULL;
    size_t i;

    if (n) {
        av = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s[] = {", av);
        for (i = 0; i < n; i++) {
            lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
        }
        lower_emitf(c, "};\n");
    }
    return av;
}

/* A call on a function value: the closure ABI over the argument
   array. The entries are borrowed -- the runtime callee retains
   whatever it stores -- so an argument reading a frame-owned local
   passes the local's own name. */
lower_val value_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, lower_val callee,
                            const char *extra)
{
    const ek_nlist *args = &e->u.call.args;
    size_t n = args->count + (extra ? 1 : 0);
    const char **argv = NULL;
    int *owned = NULL;
    size_t cap = 0, ocap = 0;
    const char *ft = lower_as_temp(c, scope, &callee);
    const char *av;
    const char *r = lower_temp(c, scope);

    if (n) {
        argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
        owned = lower_grow(c, NULL, &ocap, n, sizeof *owned);
        if (!argv || !owned) {
            c->oom = 1;
            return lower_val_unit();
        }
    }
    fill_values(c, scope, args, extra, argv, owned);
    av = emit_arg_array(c, scope, argv, n);
    lower_pad(c);
    {
        /* A world-touching call is a cancellation point: a task with
           cancellation pending ends before it, never inside it. */
        const ek_ck_row *row = lower_row(c, e);
        const char *call = row && row->bit == EK_BIT_WORLD
                               ? "ek_world_call" : "ek_closure_call";

        lower_emitf(c, "ek_val %s = %s(%s, %s, %zu);\n", r, call, ft,
                    n ? av : "(ek_val *)0", n);
    }
    release_args(c, argv, owned, n);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", ft);
    return (lower_val){ r, 1 };
}
