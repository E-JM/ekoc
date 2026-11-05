#include <string.h>

#include "lower/internal.h"

/* The capture set of a closure body: the free name reads that the
   creation site can see, the parameter names of lambdas and of
   inverted-binding patterns, and the shared small helpers. */

/* One free read: a name the body uses that the creation site can
   see. A name bound only inside the body never captures, and a name
   visible here captures even when the body shadows it, since the
   body's own binding outlives the capture read. */
void lower_read_put(lower_reads *r, const ek_node *e)
{
    const ek_ref *ref;
    const char *name;
    const char **slots;
    size_t i;

    if (e->kind != EK_N_NAME || e->u.path.count != 1) {
        return;
    }
    ref = lower_ref(r->c, e);
    if (!ref || !ref->symbol
        || (ref->kind != EK_SYM_LOCAL && ref->kind != EK_SYM_PARAM)) {
        return;
    }
    name = ref->symbol->name;
    for (i = 0; i < r->n; i++) {
        if (strcmp(r->names[i], name) == 0) {
            return;
        }
    }
    if (!scope_find_local(r->at, name)) {
        return;
    }
    slots = lower_grow(r->c, r->names, &r->cap, r->n + 1, sizeof *slots);
    if (slots) {
        slots[r->n++] = name;
        r->names = slots;
    }
}

/* The child lists one kind holds. */
static void reads_kids(lower_reads *r, ek_node *const *data, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        lower_reads_walk(r, data[i]);
    }
}

/* The operator and binding shapes. */
static void reads_oper(lower_reads *r, const ek_node *e)
{
    switch (e->kind) {
    case EK_N_BINARY:
        lower_reads_walk(r, e->u.binary.lhs);
        lower_reads_walk(r, e->u.binary.rhs);
        return;
    case EK_N_FIELD:
        lower_reads_walk(r, e->u.field.recv);
        return;
    case EK_N_LET:
    case EK_N_REBIND:
    case EK_N_INVERT:
        lower_reads_walk(r, e->u.bind.value);
        lower_reads_walk(r, e->u.bind.pattern);
        return;
    case EK_N_CALL:
        lower_reads_walk(r, e->u.call.callee);
        reads_kids(r, e->u.call.args.data, e->u.call.args.count);
        return;
    default:
        return;
    }
}

/* The control shapes: the scrutinees, the arm patterns, the guard, and
   the arm body. */
static void reads_ctl(lower_reads *r, const ek_node *e)
{
    switch (e->kind) {
    case EK_N_IF:
        lower_reads_walk(r, e->u.ifexpr.cond);
        lower_reads_walk(r, e->u.ifexpr.then);
        lower_reads_walk(r, e->u.ifexpr.els);
        return;
    case EK_N_CASE:
        reads_kids(r, e->u.caseexpr.scrutinees.data,
                   e->u.caseexpr.scrutinees.count);
        reads_kids(r, e->u.caseexpr.arms.data, e->u.caseexpr.arms.count);
        return;
    case EK_N_ARM:
        reads_kids(r, e->u.arm.patterns.data, e->u.arm.patterns.count);
        lower_reads_walk(r, e->u.arm.guard);
        lower_reads_walk(r, e->u.arm.body);
        return;
    default:
        return;
    }
}

/* The name reads of a body. A nested lambda descends as well: a free
   name read only inside it must still reach the creation site's own
   capture set when the site can see the name, or a closure body
   between the read and the binding would strand it. A lambda body
   that shadows the name rebinds it closer, so the extra capture stays
   harmless. */
void lower_reads_walk(lower_reads *r, const ek_node *e)
{
    size_t i;

    if (!e) {
        return;
    }
    switch (e->kind) {
    case EK_N_NAME:
        lower_read_put(r, e);
        return;
    case EK_N_LAMBDA:
        lower_reads_walk(r, e->u.pair.b);
        return;
    case EK_N_BLOCK:
    case EK_N_MODULE:
    case EK_N_LAMBDA_PARAMS:
    case EK_N_P_PAREN:
    case EK_N_P_OR:
    case EK_N_COMPOSITE:
        reads_kids(r, e->u.kids.data, e->u.kids.count);
        return;
    case EK_N_P_BRACE:
        /* the rest name binds; it is not a read */
        reads_kids(r, e->u.p_brace.items.data, e->u.p_brace.items.count);
        return;
    case EK_N_STRING_LIT:
        for (i = 0; i < e->u.str.count; i++) {
            lower_reads_walk(r, e->u.str.segs[i].expr);
        }
        return;
    case EK_N_PAREN:
    case EK_N_UNARY:
    case EK_N_P_LIT:
        lower_reads_walk(r, e->u.wrap.inner);
        return;
    case EK_N_APPLY:
    case EK_N_COMPOSITE_ITEM:
        lower_reads_walk(r, e->u.pair.a);
        lower_reads_walk(r, e->u.pair.b);
        return;
    case EK_N_P_FIELD:
        lower_reads_walk(r, e->u.p_field.pattern);
        return;
    case EK_N_P_CONS:
        lower_reads_walk(r, e->u.cons.payload);
        return;
    default:
        reads_oper(r, e);
        reads_ctl(r, e);
        return;
    }
}

/* One parameter name of an inverted pattern, in order; a wildcard or
   a malformed form is unnamed. */
void lower_pat_param(lower_ctx *c, const ek_node *pat, const char ***names,
                     size_t *n, size_t *cap)
{
    const char **slots;
    const char *name = NULL;

    if (!pat) {
        return;
    }
    if (pat->kind == EK_N_P_PAREN) {
        size_t i;

        for (i = 0; i < pat->u.kids.count; i++) {
            lower_pat_param(c, pat->u.kids.data[i], names, n, cap);
        }
        return;
    }
    if (pat->kind == EK_N_P_VAR && pat->u.path.count == 1
        && strcmp(pat->u.path.data[0], "_") != 0) {
        name = pat->u.path.data[0];
    }
    slots = lower_grow(c, *names, cap, *n + 1, sizeof *slots);
    if (slots) {
        slots[*n] = name;
        *names = slots;
        (*n)++;
    }
}

/* The parameter names of a lambda's parameter list. */
void lower_lambda_param_names(lower_ctx *c, const ek_node *params,
                               const char ***names, size_t *n, size_t *cap)
{
    size_t i;

    if (!params) {
        return;
    }
    for (i = 0; i < params->u.kids.count; i++) {
        const ek_node *kid = params->u.kids.data[i];
        const char *name = NULL;
        const char **slots;

        if (kid->kind == EK_N_NAME && kid->u.path.count == 1
            && strcmp(kid->u.path.data[0], "_") != 0) {
            name = kid->u.path.data[0];
        }
        slots = lower_grow(c, *names, cap, *n + 1, sizeof *slots);
        if (slots) {
            slots[*n] = name;
            *names = slots;
            (*n)++;
        }
    }
}

/* The members one body lowers from: a block's kids, or the one
   expression. */
void lower_body_members(lower_ctx *c, const ek_node *body,
                         const ek_node ***members, size_t *nmembers)
{
    if (body && body->kind == EK_N_BLOCK) {
        *members = (const ek_node **)body->u.kids.data;
        *nmembers = body->u.kids.count;
        return;
    }
    {
        const ek_node **one = ek_arena_alloc(c->arena, sizeof *one);

        if (one) {
            one[0] = body;
            *members = one;
            *nmembers = body ? 1 : 0;
        }
    }
}

/* Stores one forward declaration with the emitted signatures. */
/* The creation text of one closure: the captured names keep into the
   environment struct, the struct moves into the closure. */
const char *lower_emit_closure_new(lower_ctx *c, lower_scope *scope,
                                    const ek_node *at, lower_gtype *g,
                                    const char **names, size_t n,
                                    const char *fnname)
{
    const char **keep = NULL;
    size_t cap = 0, i;
    const char *envt, *clo;

    if (n) {
        keep = lower_grow(c, NULL, &cap, n, sizeof *keep);
        if (!keep) {
            c->oom = 1;
            return "ek_val_unit()";
        }
    }
    for (i = 0; i < n; i++) {
        const lower_local *loc = scope_find_local(scope, names[i]);
        lower_val v;

        if (loc) {
            v = (lower_val){ lower_str(c, "ek_keep(%s)", loc->cname), 0 };
        } else {
            lower_error(c, at, "the capture of `%s` has no lowering",
                        names[i]);
            v = lower_val_unit();
        }
        keep[i] = lower_as_temp(c, scope, &v);
    }
    envt = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", envt, g->helper);
    for (i = 0; i < n; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", keep[i]);
    }
    lower_emitf(c, ");\n");
    for (i = n; i-- > 0;) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", keep[i]);
    }
    clo = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_closure_new(%s, %s);\n", clo, fnname,
                envt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", envt);
    return clo;
}
void lower_push_sig(lower_ctx *c, const char *sig)
{
    const char **slot = lower_grow(c, c->sigs, &c->csig, c->nsig + 1,
                                   sizeof *slot);

    if (slot) {
        slot[c->nsig++] = sig;
        c->sigs = slot;
    }
}

