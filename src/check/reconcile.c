#include <string.h>

#include "check/internal.h"

/* The no-eq rule and the declared-bit binding rules: where `?` may
   sit in an annotation, when a signature's open bit is fed, and the
   reconciliation of the declared bit against the body bit. */

/* The capability types: built-in nominal opaque types, known under
   the bridge by their placeholder names. */
int ck_is_capability(const ek_type *t)
{
    static const char *caps[] = {"Out",   "In",   "Fs",    "Env",  "Args",
                                 "Clock", "Rand", "Proc",  "Conc"};
    size_t i;

    t = ck_prune(t);
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || t->u.nominal.argc != 0 || t->u.nominal.sym->decl) {
        return 0;
    }
    for (i = 0; i < sizeof caps / sizeof *caps; i++) {
        if (strcmp(t->u.nominal.sym->name, caps[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The handle types: reached only through a capability member, opaque
   handles whose equality and ordering halt the same way. `Task t`
   carries its binder. They are declarations of the capability module
   (Clause 9.1.1), so the module names them. */
int ck_is_handle(const ek_type *t)
{
    static const char *handles[] = {"File", "Scope", "Task"};
    size_t i;

    t = ck_prune(t);
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || t->u.nominal.argc > 1 || !t->u.nominal.sym->module
        || strcmp(t->u.nominal.sym->module, "cap") != 0) {
        return 0;
    }
    for (i = 0; i < sizeof handles / sizeof *handles; i++) {
        if (strcmp(t->u.nominal.sym->name, handles[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The eq derivation halts at capabilities and functions, recursively:
   a type built over either has no eq. */
static int no_eq_walk(ck_ctx *c, const ek_type *t, int depth);

/* The eq derivation halts at capabilities and functions, recursively
   through a nominal representation or payload. */
static int no_eq_nominal(ck_ctx *c, const ek_type *t, int depth)
{
    const ek_type *repr;
    const ck_decl *d;
    size_t i;

    if (ck_is_capability(t) || ck_is_handle(t)) {
        return 1;
    }
    if (!t->u.nominal.sym) {
        return 0;
    }
    repr = ck_decl_repr(c, t);
    if (repr) {
        return no_eq_walk(c, repr, depth - 1);
    }
    d = ck_decl_of(c, t->u.nominal.sym);
    if (!d || d->nbinders) {
        return 0;
    }
    for (i = 0; i < d->nctors; i++) {
        if (d->ctors[i] && d->ctors[i]->payload
            && no_eq_walk(c, d->ctors[i]->payload, depth - 1)) {
            return 1;
        }
    }
    return 0;
}

/* The eq derivation halts at capabilities and functions, recursively:
   a type built over either has no eq. The depth bound terminates
   recursive representations. */
static int no_eq_walk(ck_ctx *c, const ek_type *t, int depth)
{
    size_t i;

    if (depth <= 0) {
        return 0;
    }
    t = ck_prune(t);
    if (!t) {
        return 0;
    }
    switch (t->kind) {
    case EK_T_FN:
        return 1;
    case EK_T_LIST:
        return no_eq_walk(c, t->u.list.elem, depth - 1);
    case EK_T_DICT:
        return no_eq_walk(c, t->u.dict.key, depth - 1)
            || no_eq_walk(c, t->u.dict.val, depth - 1);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (no_eq_walk(c, t->u.tuple.items[i], depth - 1)) {
                return 1;
            }
        }
        return 0;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (no_eq_walk(c, t->u.record.fields[i].type, depth - 1)) {
                return 1;
            }
        }
        return 0;
    case EK_T_NOMINAL:
        return no_eq_nominal(c, t, depth);
    default:
        return 0;
    }
}

int ck_no_eq(ck_ctx *c, const ek_type *t)
{
    return no_eq_walk(c, t, 24);
}

void ck_no_eq_error(ck_ctx *c, const ek_node *at, const ek_type *t)
{
    if (c->oom || !ck_no_eq(c, t)) {
        return;
    }
    ck_error(c, at, "E-no-eq",
             "equality over %s has no derivation; capabilities, "
             "handles, and functions have no `eq` (Clause 9.8.4)",
             ck_type_name(c, t));
}

/* The JSON mapping halts at a capability, a function, and a
   non-string-keyed dictionary, recursively. */
static int no_json_walk(ck_ctx *c, const ek_type *t, int depth);

static int no_json_nominal(ck_ctx *c, const ek_type *t, int depth)
{
    const ek_type *repr;
    const ck_decl *d;
    size_t i;

    if (ck_is_capability(t)) {
        return 1;
    }
    if (!t->u.nominal.sym) {
        return 0;
    }
    repr = ck_decl_repr(c, t);
    if (repr) {
        return no_json_walk(c, repr, depth - 1);
    }
    d = ck_decl_of(c, t->u.nominal.sym);
    if (!d || d->nbinders) {
        return 0;
    }
    for (i = 0; i < d->nctors; i++) {
        if (d->ctors[i] && d->ctors[i]->payload
            && no_json_walk(c, d->ctors[i]->payload, depth - 1)) {
            return 1;
        }
    }
    return 0;
}

/* The JSON mapping halts at a capability, a function, and a
   non-string-keyed dictionary, recursively. */
static int no_json_walk(ck_ctx *c, const ek_type *t, int depth)
{
    const ek_type *key;
    size_t i;

    if (depth <= 0) {
        return 0;
    }
    t = ck_prune(t);
    if (!t) {
        return 0;
    }
    switch (t->kind) {
    case EK_T_FN:
        return 1;
    case EK_T_LIST:
        return no_json_walk(c, t->u.list.elem, depth - 1);
    case EK_T_DICT:
        key = ck_prune(t->u.dict.key);
        if (key && key->kind != EK_T_IVAR && key->kind != EK_T_TVAR
            && !ck_is_std(key, "String")) {
            return 1; /* non-string-keyed dictionaries have no form */
        }
        return no_json_walk(c, t->u.dict.val, depth - 1);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (no_json_walk(c, t->u.tuple.items[i], depth - 1)) {
                return 1;
            }
        }
        return 0;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (no_json_walk(c, t->u.record.fields[i].type, depth - 1)) {
                return 1;
            }
        }
        return 0;
    case EK_T_NOMINAL:
        return no_json_nominal(c, t, depth);
    default:
        return 0;
    }
}

int ck_no_json(ck_ctx *c, const ek_type *t)
{
    return no_json_walk(c, t, 24);
}

void ck_json_shape_error(ck_ctx *c, const ek_node *at, const ek_type *t)
{
    if (c->oom || !ck_no_json(c, t)) {
        return;
    }
    ck_error(c, at, "E-json-shape",
             "the type %s has no default JSON form; capabilities, "
             "functions, and non-string-keyed dictionaries have no "
             "`to_json` (Clause 9.9.3)", ck_type_name(c, t));
}

/* Open arrows are allowed on the annotation's own arrow and inside its
   parameter types; a result type, container element, or record field
   forbids them. */
void ck_open_positions(ck_ctx *c, const ek_node *t, int allow)
{
    size_t i, n;

    if (!t || c->oom) {
        return;
    }
    switch (t->kind) {
    case EK_N_T_FN:
        if (t->u.t_fn.arrow == EK_ARROW_OPEN && !allow) {
            ck_error(c, t, "E-open-bit-position",
                     "`?` occurs where no binding generalizes it; the open "
                     "bit is allowed on the binding's own arrow and inside "
                     "its parameter types (Clause 7.9.6)");
        }
        for (i = 0, n = t->u.t_fn.params.count; i < n; i++) {
            ck_open_positions(c, t->u.t_fn.params.data[i]->u.t_field.type,
                              1);
        }
        ck_open_positions(c, t->u.t_fn.result, 0);
        return;
    case EK_N_T_LIST:
    case EK_N_T_PAREN:
        ck_open_positions(c, t->u.wrap.inner, 0);
        return;
    case EK_N_T_DICT:
        ck_open_positions(c, t->u.pair.a, 0);
        ck_open_positions(c, t->u.pair.b, 0);
        return;
    case EK_N_T_APPLY:
        for (i = 0, n = t->u.t_apply.args.count; i < n; i++) {
            ck_open_positions(c, t->u.t_apply.args.data[i], 0);
        }
        return;
    case EK_N_T_RECORD:
        for (i = 0, n = t->u.kids.count; i < n; i++) {
            ck_open_positions(c, t->u.kids.data[i]->u.t_field.type, 0);
        }
        return;
    case EK_N_T_TUPLE:
        for (i = 0, n = t->u.kids.count; i < n; i++) {
            const ek_node *k = t->u.kids.data[i];

            ck_open_positions(c, k->kind == EK_N_T_FIELD ? k->u.t_field.type
                                                         : k, 0);
        }
        return;
    default:
        return;
    }
}

/* The annotation-side checks of one binding: the position rule, and
   for an open own arrow the feeder rule, judged on the elaborated
   parameter types. Returns 1 when the open bit is unfed, so the body
   must not reconcile. */
int ck_annotation_bits(ck_ctx *c, const ek_node *ann, const ek_type *bound)
{
    const ek_type *f = ck_prune(bound);
    size_t i;

    ck_open_positions(c, ann, 1);
    if (!f || f->kind != EK_T_FN || f->u.fn.arrow != EK_ARROW_OPEN) {
        return 0;
    }
    for (i = 0; i < f->u.fn.count; i++) {
        if (ck_type_opens(f->u.fn.params[i])) {
            return 0;
        }
    }
    ck_error(c, ann, "E-open-bit-unfed",
             "the open bit has no parameter to feed it; a `?` arrow needs "
             "a parameter whose type contains `?` (Clause 7.9.6)");
    return 1;
}

ek_bit ck_own_bit(const ek_type *bound)
{
    const ek_type *f = ck_prune(bound);

    if (!f || f->kind != EK_T_FN) {
        return EK_BIT_PURE;
    }
    if (f->u.fn.arrow == EK_ARROW_WORLD) {
        return EK_BIT_WORLD;
    }
    return f->u.fn.arrow == EK_ARROW_OPEN ? EK_BIT_OPEN : EK_BIT_PURE;
}

static const char *bit_name(ek_bit b)
{
    if (b == EK_BIT_WORLD) {
        return "world-touching";
    }
    return b == EK_BIT_OPEN ? "open" : "pure";
}

/* At one annotated binding the declared bit MUST equal the OR of the
   call bits of the body. A lambda value is the declared body itself,
   so its own bit is the body bit; any other value excludes lambda
   interiors, because a closure's calls belong to the closure's own
   declared arrow. */
void ck_reconcile(ck_ctx *c, const ek_node *at, const ek_type *bound,
                  const ek_node *val)
{
    ek_bit own = ck_own_bit(bound);
    ek_bit body;

    if (!val || c->oom) {
        return;
    }
    if (val->kind == EK_N_LAMBDA) {
        body = ck_bit_of(c, val);
        if (own == EK_BIT_PURE && body == EK_BIT_WORLD) {
            return; /* the lambda check reported the direction */
        }
    } else {
        body = ck_body_bit(c, val);
    }
    if (body != own) {
        ck_error(c, at, "E-world-bit",
                 "the declared bit is %s, the body bit is %s (Clause "
                 "7.9.2)", bit_name(own), bit_name(body));
    }
}
