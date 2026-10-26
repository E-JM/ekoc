#include <string.h>

#include "check/internal.h"

/* Conformance (Clauses 7.5.7, 7.9.5). Rigid type variables never
   unify with anything but themselves (Clause 7.6.3); inference
   variables bind. `allow_shape` admits the nominal-to-anonymous
   adaptation of Clause 7.5.5; `never_ok` admits bottom adaptation
   (Clause 7.5.4). Neither holds inside a function type. */

int ck_occurs(const ek_type *v, const ek_type *t)
{
    size_t i;

    t = ck_prune(t);
    if (t == v) {
        return 1;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            if (ck_occurs(v, t->u.nominal.args[i])) {
                return 1;
            }
        }
        return 0;
    case EK_T_LIST:
        return ck_occurs(v, t->u.list.elem);
    case EK_T_DICT:
        return ck_occurs(v, t->u.dict.key) || ck_occurs(v, t->u.dict.val);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (ck_occurs(v, t->u.tuple.items[i])) {
                return 1;
            }
        }
        return 0;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (ck_occurs(v, t->u.record.fields[i].type)) {
                return 1;
            }
        }
        return 0;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            if (ck_occurs(v, t->u.fn.params[i])) {
                return 1;
            }
        }
        return ck_occurs(v, t->u.fn.result);
    default:
        return 0;
    }
}

int ck_bind(ck_ctx *c, const ek_type *v, const ek_type *t)
{
    (void)c;
    if (!v || !t) {
        return 1;
    }
    if (v == t || ck_occurs(v, t)) {
        return 0;
    }
    ((ek_type *)v)->u.ivar.bound = t;
    return 1;
}

static ck_conform conform(ck_ctx *c, const ek_type *a, const ek_type *e,
                          int allow_shape, int never_ok);

/* A component of a composite: invariant, so judged by equality with
   inference variables binding (Clause 7.5.3); shape adaptation does
   not enter, and Never only where the position admits it. */
static ck_conform component(ck_ctx *c, const ek_type *a, const ek_type *e,
                            int never_ok)
{
    return conform(c, a, e, 0, never_ok) == CK_OK ? CK_OK : CK_MISMATCH;
}

ck_conform ck_conforms(ck_ctx *c, const ek_type *a, const ek_type *e,
                       int allow_shape)
{
    return conform(c, a, e, allow_shape, 1);
}

/* `Never` is a std ADT, so bottom arrives as a nominal too. */
static int is_bottom(const ek_type *t)
{
    return t && (t->kind == EK_T_NEVER
                 || (t->kind == EK_T_NOMINAL && t->u.nominal.sym
                     && t->u.nominal.sym->decl == NULL
                     && t->u.nominal.sym->module
                     && strcmp(t->u.nominal.sym->module, "std") == 0
                     && strcmp(t->u.nominal.sym->name, "Never") == 0));
}

/* Function conformance: one-step arrow-bit adaptation, same arity,
   invariant parameters and result (Clause 7.9.5). */
static ck_conform conform_fn(ck_ctx *c, const ek_type *a, const ek_type *e)
{
    size_t i;

    if (a->u.fn.arrow != e->u.fn.arrow
        && !(a->u.fn.arrow == EK_ARROW_PURE && e->u.fn.arrow != EK_ARROW_PURE)) {
        return CK_MISMATCH; /* bit adaptation is one step (7.9.5) */
    }
    if (a->u.fn.count != e->u.fn.count) {
        return CK_MISMATCH;
    }
    for (i = 0; i < a->u.fn.count; i++) {
        if (component(c, a->u.fn.params[i], e->u.fn.params[i], 0)
            != CK_OK) {
            return CK_MISMATCH;
        }
    }
    return component(c, a->u.fn.result, e->u.fn.result, 0);
}

/* Conformance of the composite kinds, reached with equal kinds: same
   shape, invariant components, and one-step arrow-bit adaptation
   (Clause 7.5.7, 7.9.5). */
static ck_conform conform_composite(ck_ctx *c, const ek_type *a,
                                    const ek_type *e)
{
    size_t i;

    switch (a->kind) {
    case EK_T_LIST:
        return component(c, a->u.list.elem, e->u.list.elem, 1);
    case EK_T_DICT:
        if (component(c, a->u.dict.key, e->u.dict.key, 1) != CK_OK) {
            return CK_MISMATCH;
        }
        return component(c, a->u.dict.val, e->u.dict.val, 1);
    case EK_T_TUPLE:
        if (a->u.tuple.count != e->u.tuple.count) {
            return CK_MISMATCH;
        }
        for (i = 0; i < a->u.tuple.count; i++) {
            if (component(c, a->u.tuple.items[i], e->u.tuple.items[i], 1)
                != CK_OK) {
                return CK_MISMATCH;
            }
        }
        return CK_OK;
    case EK_T_RECORD:
        if (a->u.record.count != e->u.record.count) {
            return CK_MISMATCH;
        }
        for (i = 0; i < a->u.record.count; i++) {
            if (strcmp(a->u.record.fields[i].name, e->u.record.fields[i].name)
                    != 0
                || component(c, a->u.record.fields[i].type,
                             e->u.record.fields[i].type, 1) != CK_OK) {
                return CK_MISMATCH;
            }
        }
        return CK_OK;
    case EK_T_FN:
        return conform_fn(c, a, e);
    default:
        return CK_OK;
    }
}

/* The nominal-to-nominal judgment: same declaration, conforming
   arguments (Clause 7.5.2). */
static ck_conform conform_nominal(ck_ctx *c, const ek_type *a,
                                  const ek_type *e)
{
    size_t i;

    if (a->u.nominal.sym != e->u.nominal.sym
        || a->u.nominal.argc != e->u.nominal.argc) {
        return CK_NOMINAL;
    }
    for (i = 0; i < a->u.nominal.argc; i++) {
        if (component(c, a->u.nominal.args[i], e->u.nominal.args[i], 0)
            != CK_OK) {
            return CK_MISMATCH;
        }
    }
    return CK_OK;
}

/* Nominal to anonymous, one direction, exact shape (7.5.5). */
static ck_conform conform_shape(ck_ctx *c, const ek_type *a,
                                const ek_type *e)
{
    const ek_type *repr = ck_decl_repr(c, a);

    if (repr && conform(c, repr, e, 1, 0) == CK_OK) {
        return CK_OK;
    }
    return e->kind == EK_T_RECORD || e->kind == EK_T_TUPLE ? CK_SHAPE
                                                           : CK_MISMATCH;
}

static ck_conform conform(ck_ctx *c, const ek_type *a, const ek_type *e,
                          int allow_shape, int never_ok)
{
    a = ck_prune(a);
    e = ck_prune(e);
    if (!a || !e || a == e) {
        return CK_OK;
    }
    if (a->kind == EK_T_IVAR) {
        return ck_bind(c, a, e) ? CK_OK : CK_MISMATCH;
    }
    if (e->kind == EK_T_IVAR) {
        return ck_bind(c, e, a) ? CK_OK : CK_MISMATCH;
    }
    if (is_bottom(a) && e->kind != EK_T_FN && never_ok) {
        return CK_OK; /* bottom adapts at value positions (7.5.4) */
    }
    if (a->kind == EK_T_TVAR || e->kind == EK_T_TVAR) {
        return a == e ? CK_OK : CK_MISMATCH; /* rigid (7.6.3) */
    }
    if (a->kind == EK_T_NOMINAL && e->kind == EK_T_NOMINAL) {
        return conform_nominal(c, a, e);
    }
    if (a->kind == EK_T_NOMINAL && allow_shape) {
        return conform_shape(c, a, e);
    }
    if (a->kind != e->kind) {
        return CK_MISMATCH;
    }
    return conform_composite(c, a, e);
}

const char *ck_conform_code(ck_conform k)
{
    switch (k) {
    case CK_NOMINAL:
        return "E-nominal-mismatch";
    case CK_SHAPE:
        return "E-shape-mismatch";
    default:
        return "E-type-mismatch";
    }
}

/* The symmetric judgment of operator and arm typing: either direction
   conforming suffices; the result prunes through what bound. */
const ek_type *ck_unify(ck_ctx *c, const ek_type *a, const ek_type *b,
                        int allow_shape)
{
    if (ck_conforms(c, a, b, allow_shape) == CK_OK) {
        return ck_prune(b);
    }
    if (ck_conforms(c, b, a, allow_shape) == CK_OK) {
        return ck_prune(a);
    }
    return NULL;
}
