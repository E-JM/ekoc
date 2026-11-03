#include <string.h>

#include "lower/internal.h"

/* The call machinery below the dispatch: the injected identities, the
   borrowed-argument reading, the call site's shape, and the direct and
   closure invocations over the filled argument slots. */

/* An injected binding: a function row of the capability module with no
   declaration behind it. The compiler owns these rows -- the unwrap
   and wrap of each session bundle -- and no source form declares
   them. */
int is_injected(const ek_symbol *sym)
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

const char *injected_identity(lower_ctx *c, const ek_symbol *sym)
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
int args_rebind_from(const ek_nlist *args, size_t i)
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
const ek_type *fn_param(const ek_type *ft, size_t i)
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
void set_recv(lower_ctx *c, lower_scope *scope, call_form *member,
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
