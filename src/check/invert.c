#include <string.h>

#include "check/internal.h"

/* The inverted binding: `<-` places the remaining block in the last
   parameter position of a call. The admitted patterns are the
   parameter-position forms, the callback's shape is judged against
   them, and the continuation is checked at the callback's result
   type. */
/* The parameter count of an inverted pattern: the zero form has no
   pattern, a name or wildcard is one parameter, and a group is one per
   member. */
static size_t invert_arity(const ek_node *pat)
{
    size_t i, n = 0;

    if (!pat) {
        return 0;
    }
    if (pat->kind != EK_N_P_PAREN) {
        return 1;
    }
    for (i = 0; i < pat->u.kids.count; i++) {
        n += invert_arity(pat->u.kids.data[i]);
    }
    return n;
}

/* The call of an inverted binding: it carries one implicit callback in
   its last argument slot. */
static const ek_type *invert_call(ck_ctx *c, const ek_node *m,
                                  const ek_type **fn, const ek_type **cb)
{
    const ek_type *t;

    c->invert_fn = NULL;
    c->invert_cb = NULL;
    c->invert_call = 1;
    t = ck_expr(c, m->u.bind.value, NULL);
    c->invert_call = 0;
    *fn = c->invert_fn;
    *cb = c->invert_cb;
    return t;
}

/* The callback's result type, or NULL when its shape is opaque. */
static const ek_type *callback_result(const ek_type *cb)
{
    const ek_type *f = ck_prune(cb);

    return f && f->kind == EK_T_FN ? f->u.fn.result : NULL;
}

/* The callback's parameter list must match the inverted pattern. */
static void invert_shape(ck_ctx *c, const ek_node *m, const ek_node *pat,
                         const ek_type *cb)
{
    const ek_type *f = ck_prune(cb);
    size_t want = invert_arity(pat);

    if (!f || f->kind == EK_T_IVAR || f->kind == EK_T_TVAR) {
        return; /* the bridge leaves the callback opaque */
    }
    if (f->kind != EK_T_FN) {
        ck_error(c, m, "E-type-mismatch",
                 "the last parameter is %s, not a callback (Clause 7.8.1)",
                 ck_type_name(c, cb));
        return;
    }
    if (f->u.fn.count != want) {
        ck_error(c, m, "E-call-arity",
                 "the callback takes %zu parameters, the dispatcher "
                 "expects %zu (Clause 6.3.4)", want, f->u.fn.count);
    }
}

/* Binds an inverted pattern's names at the callback's parameters. */
static void bind_invert(ck_ctx *c, const ek_node *pat, const ek_type *cb,
                        size_t *idx)
{
    const ek_type *f = ck_prune(cb);
    size_t i;

    if (!pat) {
        return;
    }
    if (pat->kind == EK_N_P_PAREN) {
        for (i = 0; i < pat->u.kids.count; i++) {
            bind_invert(c, pat->u.kids.data[i], cb, idx);
        }
        return;
    }
    if (pat->kind == EK_N_P_VAR && pat->u.path.count == 1
        && strcmp(pat->u.path.data[0], "_") != 0) {
        ck_scope_bind_scheme(
            c, pat->u.path.data[0],
            ck_mono(c, f && f->kind == EK_T_FN && *idx < f->u.fn.count
                          ? f->u.fn.params[*idx] : ck_opaque(c)));
    }
    (*idx)++;
}

static ek_bit bit_or(ek_bit a, ek_bit b)
{
    if (a == EK_BIT_WORLD || b == EK_BIT_WORLD) {
        return EK_BIT_WORLD;
    }
    if (a == EK_BIT_OPEN || b == EK_BIT_OPEN) {
        return EK_BIT_OPEN;
    }
    return EK_BIT_PURE;
}

/* The callback body's bit: the OR over the continuation members. */
static ek_bit rest_bit(ck_ctx *c, const ek_node *block, size_t start)
{
    ek_bit b = EK_BIT_PURE;
    size_t i;

    for (i = start; i < block->u.kids.count; i++) {
        b = bit_or(b, ck_body_bit(c, block->u.kids.data[i]));
    }
    return b;
}

/* A callback body runs inside the dispatcher call: an open dispatcher
   instantiates from it, and an opaque dispatcher takes it by OR. */
static void invert_call_bit(ck_ctx *c, const ek_node *call,
                            const ek_type *fn, ek_bit body)
{
    ek_bit b = ck_bit_of(c, call);
    const ek_type *f = ck_prune(fn);

    if (!f || f->kind != EK_T_FN || f->u.fn.arrow == EK_ARROW_OPEN) {
        b = bit_or(b, body);
    }
    ck_call_bit(c, call, b);
}

/* The inverted form outside a block: the call is checked and the
   pattern's callback shape is judged, with no continuation. */
const ek_type *ck_invert_binding(ck_ctx *c, const ek_node *m,
                                     const ek_node *pat, int top)
{
    const ek_type *fn, *cb, *vt;

    vt = invert_call(c, m, &fn, &cb);
    if (!ck_invert_pattern(pat)) {
        ck_error(c, pat, "E-refutable-binding",
                 "an inverted binding takes a parameter-position "
                 "pattern (Clauses 7.7.1, 7.8.1)");
    }
    invert_shape(c, m, pat, cb);
    if (!top && cb) {
        size_t idx = 0;

        ck_scope_push(c);
        bind_invert(c, pat, cb, &idx);
        ck_scope_pop(c);
    }
    ck_row(c, m, vt, EK_RD_NONE);
    return vt;
}

/* The inverted form inside a block: the remaining members are the
   callback body, checked at the callback's result type. When the
   dispatcher is opaque the callback carries no result type and the
   block's own expectation is the last member's expectation. */
const ek_type *ck_invert_block(ck_ctx *c, const ek_node *m,
                               const ek_node *block, size_t start,
                               const ek_type *expected)
{
    const ek_node *pat = m->u.bind.pattern;
    const ek_node *call = m->u.bind.value;
    const ek_type *fn, *cb, *vt, *res, *body;
    const ek_node *last = NULL;
    ek_bit b = EK_BIT_PURE;
    size_t idx = 0, n = block ? block->u.kids.count : 0;
    int has_body = block && start < n;

    vt = invert_call(c, m, &fn, &cb);
    if (!ck_invert_pattern(pat)) {
        ck_error(c, pat, "E-refutable-binding",
                 "an inverted binding takes a parameter-position "
                 "pattern (Clauses 7.7.1, 7.8.1)");
    }
    invert_shape(c, m, pat, cb);
    res = callback_result(cb);
    ck_scope_push(c);
    if (cb) {
        bind_invert(c, pat, cb, &idx);
    }
    if (has_body) {
        last = block->u.kids.data[n - 1];
        body = ck_members(c, block, start, res ? res : expected);
        b = rest_bit(c, block, start);
        if (res) {
            ck_conform k = ck_conforms(c, body, res, 1);

            if (k != CK_OK) {
                ck_error(c, last, ck_conform_code(k),
                         "the callback body is %s, the callback result is "
                         "%s (Clause 6.3.4)", ck_type_name(c, body),
                         ck_type_name(c, res));
            }
        }
        if (cb && ck_prune(cb)->kind == EK_T_FN
            && ck_prune(cb)->u.fn.arrow == EK_ARROW_PURE
            && b == EK_BIT_WORLD) {
            ck_error(c, last, "E-world-bit",
                     "the callback body is world-touching, the callback "
                     "parameter is pure (Clause 7.9.5)");
        }
        invert_call_bit(c, call, fn, b);
    }
    ck_scope_pop(c);
    ck_row(c, m, vt, EK_RD_NONE);
    return vt;
}
