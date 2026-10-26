#include <string.h>

#include "check/internal.h"

/* The world-bit derivation (Clause 7.9). A call stores its own bit, a
   lambda the bit of its body, an annotated binding its declared bit.
   Every other node's bit is the OR of the call bits in its subtree,
   lambda interiors excluded, because only calls contribute (Clause
   7.9.2); the OR is computed on demand and memoized. */

/* The OR table of Clause 7.9.2. */
static ek_bit fold(ek_bit a, ek_bit b)
{
    if (a == EK_BIT_WORLD || b == EK_BIT_WORLD) {
        return EK_BIT_WORLD;
    }
    if (a == EK_BIT_OPEN || b == EK_BIT_OPEN) {
        return EK_BIT_OPEN;
    }
    return EK_BIT_PURE;
}

/* The bit slots live in the maps as bit + 1, so 0 stays "absent". */
static ek_bit map_bit(const ck_map *m, const ek_node *n)
{
    const void *v = ck_map_get(m, n);

    return v ? (ek_bit)((size_t)v - 1) : EK_BIT_PURE;
}

static void put_bit(ck_ctx *c, ck_map *m, const ek_node *n, ek_bit b)
{
    ck_map_put(c, m, n, (const void *)(size_t)(b + 1));
}

void ck_call_bit(ck_ctx *c, const ek_node *call, ek_bit b)
{
    put_bit(c, &c->bits, call, b);
}

void ck_lambda_bit(ck_ctx *c, const ek_node *lam, ek_bit b)
{
    put_bit(c, &c->bits, lam, b);
}

void ck_declare_bit(ck_ctx *c, const ek_node *decl, ek_bit b)
{
    put_bit(c, &c->bits, decl, b);
}

ek_bit ck_bit_of(ck_ctx *c, const ek_node *n)
{
    return map_bit(&c->bits, n);
}

/* The bit an argument supplies where a `?` is instantiated (Clause
   7.9.6): a lambda carries its body's bit, a world-touching function
   value its arrow's bit, anything else nothing (holding is quiet). */
ek_bit ck_arg_bit(ck_ctx *c, const ek_node *arg, const ek_type *arg_type)
{
    const ek_type *t;

    if (arg && arg->kind == EK_N_LAMBDA) {
        return map_bit(&c->bits, arg);
    }
    t = ck_prune(arg_type);
    if (t && t->kind == EK_T_FN && t->u.fn.arrow == EK_ARROW_WORLD) {
        return EK_BIT_WORLD;
    }
    return EK_BIT_PURE;
}

/* True when the type contains an open arrow. */
int ck_type_opens(const ek_type *t)
{
    size_t i;

    t = ck_prune(t);
    if (!t) {
        return 0;
    }
    if (t->kind == EK_T_FN) {
        if (t->u.fn.arrow == EK_ARROW_OPEN) {
            return 1;
        }
        for (i = 0; i < t->u.fn.count; i++) {
            if (ck_type_opens(t->u.fn.params[i])) {
                return 1;
            }
        }
        return ck_type_opens(t->u.fn.result);
    }
    if (t->kind == EK_T_LIST) {
        return ck_type_opens(t->u.list.elem);
    }
    if (t->kind == EK_T_DICT) {
        return ck_type_opens(t->u.dict.key)
            || ck_type_opens(t->u.dict.val);
    }
    if (t->kind == EK_T_TUPLE) {
        for (i = 0; i < t->u.tuple.count; i++) {
            if (ck_type_opens(t->u.tuple.items[i])) {
                return 1;
            }
        }
        return 0;
    }
    if (t->kind == EK_T_RECORD) {
        for (i = 0; i < t->u.record.count; i++) {
            if (ck_type_opens(t->u.record.fields[i].type)) {
                return 1;
            }
        }
    }
    return 0;
}

/* True when a signature contains the open bit at all: on its own
   arrow, or fed by a parameter (Clause 7.9.6). */
int ck_signature_opens(const ek_type *f)
{
    size_t i;

    f = ck_prune(f);
    if (!f || f->kind != EK_T_FN) {
        return 0;
    }
    if (f->u.fn.arrow == EK_ARROW_OPEN) {
        return 1;
    }
    for (i = 0; i < f->u.fn.count; i++) {
        if (ck_type_opens(f->u.fn.params[i])) {
            return 1;
        }
    }
    return 0;
}

/* A lambda child contributes nothing: its calls belong to its own
   arrow (Clause 7.9.2). */
static ek_bit child(ck_ctx *c, const ek_node *e)
{
    if (!e || e->kind == EK_N_LAMBDA) {
        return EK_BIT_PURE;
    }
    return ck_body_bit(c, e);
}

ek_bit ck_body_bit(ck_ctx *c, const ek_node *e)
{
    const void *memo;
    ek_bit b;
    size_t i;

    if (!e) {
        return EK_BIT_PURE;
    }
    memo = ck_map_get(&c->contrib, e);
    if (memo) {
        return (ek_bit)((size_t)memo - 1);
    }
    b = e->kind == EK_N_CALL ? map_bit(&c->bits, e) : EK_BIT_PURE;
    switch (e->kind) {
    case EK_N_MODULE:
    case EK_N_BLOCK:
    case EK_N_COMPOSITE:
    case EK_N_LAMBDA_PARAMS:
        for (i = 0; i < e->u.kids.count; i++) {
            b = fold(b, child(c, e->u.kids.data[i]));
        }
        break;
    case EK_N_LET:
    case EK_N_REBIND:
    case EK_N_INVERT:
        b = fold(b, child(c, e->u.bind.value));
        break;
    case EK_N_NAMESPACE:
        b = fold(b, child(c, e->u.namespace.value));
        break;
    case EK_N_TEST_DECL:
    case EK_N_EXTERN_DECL:
    case EK_N_CTYPE_DECL:
        b = fold(b, child(c, e->u.decl.body));
        break;
    case EK_N_STRING_LIT:
        for (i = 0; i < e->u.str.count; i++) {
            b = fold(b, child(c, e->u.str.segs[i].expr));
        }
        break;
    case EK_N_FIELD:
        b = fold(b, child(c, e->u.field.recv));
        break;
    case EK_N_CALL:
        b = fold(b, child(c, e->u.call.callee));
        for (i = 0; i < e->u.call.args.count; i++) {
            b = fold(b, child(c, e->u.call.args.data[i]));
        }
        break;
    case EK_N_APPLY:
    case EK_N_COMPOSITE_ITEM:
        b = fold(b, child(c, e->u.pair.a));
        b = fold(b, child(c, e->u.pair.b));
        break;
    case EK_N_PAREN:
    case EK_N_UNARY:
        b = fold(b, child(c, e->u.wrap.inner));
        break;
    case EK_N_BINARY:
        b = fold(b, child(c, e->u.binary.lhs));
        b = fold(b, child(c, e->u.binary.rhs));
        break;
    case EK_N_IF:
        b = fold(b, child(c, e->u.ifexpr.cond));
        b = fold(b, child(c, e->u.ifexpr.then));
        b = fold(b, child(c, e->u.ifexpr.els));
        break;
    case EK_N_CASE:
        for (i = 0; i < e->u.caseexpr.scrutinees.count; i++) {
            b = fold(b, child(c, e->u.caseexpr.scrutinees.data[i]));
        }
        for (i = 0; i < e->u.caseexpr.arms.count; i++) {
            b = fold(b, child(c, e->u.caseexpr.arms.data[i]));
        }
        break;
    case EK_N_ARM:
        b = fold(b, child(c, e->u.arm.guard));
        b = fold(b, child(c, e->u.arm.body));
        break;
    default:
        break; /* patterns and type positions carry no calls */
    }
    put_bit(c, &c->contrib, e, b);
    return b;
}

/* The pure derivation combinators the spec names (Clauses 7.9.2,
   8.9.2, 9.12.5, 9.14.5): deriving or observing through them sets no
   bit. Matched by member name under the opacity ruling (check.h). */
static int is_combinator(const char *member)
{
    static const char *pure[] = {"null",   "tee",      "empty",
                                 "from_lines", "frozen", "with_seed",
                                 "root",  "join",     "cancel"};
    size_t i;

    for (i = 0; i < sizeof pure / sizeof *pure; i++) {
        if (strcmp(pure[i], member) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The bit of a call the bridge leaves opaque (check.h): world-touching
   when a capability receiver's member is none of the pure combinators
   -- `Conc` sets no bit (Clause 7.9.1) -- or when an argument carries
   a world bit; pure otherwise. */
static ek_bit opaque_call_bit(ck_ctx *c, const ek_type *recv,
                              const char *member, const ek_node *const *args,
                              const ek_type *const *types, size_t n)
{
    const ek_type *r = ck_prune(recv);
    size_t i;

    if (r && member && ck_is_capability(r)
        && strcmp(r->u.nominal.sym->name, "Conc") != 0
        && !is_combinator(member)) {
        return EK_BIT_WORLD;
    }
    for (i = 0; i < n; i++) {
        if (ck_arg_bit(c, args[i], types[i]) == EK_BIT_WORLD) {
            return EK_BIT_WORLD;
        }
    }
    return EK_BIT_PURE;
}

/* The bit of one call (Clause 7.9.2): through a known arrow its own
   bit; through an open arrow the instantiation from the feeder
   arguments (Clause 7.9.6) -- or, inside the body of the declaration
   that owns the open bit, that bit. A call the bridge leaves opaque
   takes the opacity ruling of check.h. off is 1 when a receiver is
   prepended, so parameter i takes argument i - off. */
ek_bit ck_call_bit_of(ck_ctx *c, const ek_type *f, const char *member,
                      const ek_type *recv, const ek_node *const *args,
                      const ek_type *const *types, size_t n, size_t off)
{
    ek_bit b;
    size_t i;

    if (!f || f->kind != EK_T_FN) {
        return opaque_call_bit(c, recv, member, args, types, n);
    }
    if (f->u.fn.arrow == EK_ARROW_WORLD) {
        return EK_BIT_WORLD;
    }
    if (f->u.fn.arrow != EK_ARROW_OPEN) {
        return EK_BIT_PURE;
    }
    b = c->open_scope ? EK_BIT_OPEN : EK_BIT_PURE;
    for (i = off; i < f->u.fn.count && i - off < n; i++) {
        if (ck_type_opens(f->u.fn.params[i])) {
            b = fold(b, ck_arg_bit(c, args[i - off], types[i - off]));
        }
    }
    return b;
}

/* The final row pass: every row carries a defined bit (check.h). */
void ck_fill_bits(ck_ctx *c)
{
    size_t i;

    for (i = 0; i < c->nrows; i++) {
        const ek_node *n = c->rows[i].node;
        const void *decl = ck_map_get(&c->bits, n);

        if (n->kind == EK_N_CALL || n->kind == EK_N_LAMBDA || decl) {
            c->rows[i].bit = map_bit(&c->bits, n);
        } else {
            c->rows[i].bit = ck_body_bit(c, n);
        }
    }
}
