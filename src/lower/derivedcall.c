#include <string.h>

#include "lower/internal.h"

/* The call sites of the derived services: a member link, free call, or
   type-head call whose reading check recorded as derived lowers to its
   service, with the receiver's applied type naming the specialization;
   the with service rebuilds the receiver's record through its
   construction helper; and a comparison operator over composite rows
   reads the same equality or ordering service. Every argument
   evaluates to a temp in source order and releases once the call
   stands. */


/* The service family a member spelling names: 1 for equality, 2 for
   ordering, 0 for the rest. */
static int cmp_family(const char *name)
{
    if (strcmp(name, "eq") == 0 || strcmp(name, "ne") == 0) {
        return 1;
    }
    if (strcmp(name, "lt") == 0 || strcmp(name, "le") == 0
        || strcmp(name, "gt") == 0 || strcmp(name, "ge") == 0) {
        return 2;
    }
    return 0;
}

/* The service kind behind a member spelling, with 0 when the name is
   no derived service. The open-receiver re-route and the result-type
   recovery share the table. */
int lower_service_kind(const char *name, lower_dsvc_kind *out)
{
    if (strcmp(name, "show") == 0) {
        *out = DSV_SHOW;
        return 1;
    }
    if (strcmp(name, "inspect") == 0) {
        *out = DSV_INSPECT;
        return 1;
    }
    if (strcmp(name, "to_json") == 0) {
        *out = DSV_TOJSON;
        return 1;
    }
    if (strcmp(name, "from_json") == 0) {
        *out = DSV_FROMJSON;
        return 1;
    }
    if (cmp_family(name) == 1) {
        *out = DSV_EQ;
        return 1;
    }
    if (cmp_family(name) == 2) {
        *out = DSV_ORD;
        return 1;
    }
    return 0;
}

/* True when the slot rides the runtime's numeric comparisons. */
static int num_slot(lower_scalar sk, int ordering)
{
    if (sk == EKSC_INT || sk == EKSC_UINT || sk == EKSC_FLOAT
        || sk == EKSC_CHAR) {
        return 1;
    }
    return !ordering && sk == EKSC_BOOL;
}

/* The written argument count of one service: a member call passes the
   receiver through its link, every other form writes it. */
static size_t written_args(int family, lower_cmp_form form)
{
    size_t n = family ? 2u : 1u;

    return form == CF_MEMBER ? n - 1u : n;
}

/* The receiver temp and written argument temps of the call, in source
   order. */
static const char **call_temps(lower_ctx *c, lower_scope *scope,
                               const ek_node *e, const ek_node *callee,
                               lower_cmp_form form, size_t n)
{
    const ek_nlist *args = &e->u.call.args;
    const char **argv = NULL;
    size_t cap = 0, i, k = 0;

    argv = lower_grow(c, NULL, &cap, n ? n : 1, sizeof *argv);
    if (!argv) {
        c->oom = 1;
        return NULL;
    }
    if (form == CF_MEMBER) {
        lower_val r = lower_sub(c, scope, callee->u.field.recv);

        argv[k++] = lower_as_temp(c, scope, &r);
    }
    for (i = 0; i < n - k; i++) {
        lower_val a = lower_sub(c, scope, args->data[i]);

        argv[k++] = lower_as_temp(c, scope, &a);
    }
    return argv;
}

static void release_temps(lower_ctx *c, const char **argv, size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i]);
    }
}

/* The comparison line: the runtime family over the scalar slots -- its
   entries already answer Bool values -- and the derived service
   everywhere else. */
static lower_val emit_cmp(lower_ctx *c, lower_scope *scope,
                          const char *name, const char *svc, lower_scalar sk,
                          const char **argv)
{
    const char *t = lower_temp(c, scope);
    const char *x = argv[0];
    const char *y = argv[1];
    const char *core = NULL;
    int neg = 0;
    int ordering = cmp_family(name) == 2;

    if (num_slot(sk, ordering)) {
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_num_%s(%s, %s);\n", t, name, x, y);
        return (lower_val){ t, 1 };
    }
    if (strcmp(name, "eq") == 0) {
        core = lower_str(c, "%s(%s, %s)", svc, x, y);
    } else if (strcmp(name, "ne") == 0) {
        core = lower_str(c, "%s(%s, %s)", svc, x, y);
        neg = 1;
    } else if (strcmp(name, "lt") == 0) {
        core = lower_str(c, "%s(%s, %s) < 0", svc, x, y);
    } else if (strcmp(name, "le") == 0) {
        core = lower_str(c, "%s(%s, %s) <= 0", svc, x, y);
    } else if (strcmp(name, "gt") == 0) {
        core = lower_str(c, "%s(%s, %s) > 0", svc, x, y);
    } else {
        core = lower_str(c, "%s(%s, %s) >= 0", svc, x, y);
    }
    lower_pad(c);
    if (neg) {
        lower_emitf(c, "ek_val %s = ek_bool(!%s);\n", t, core);
    } else {
        lower_emitf(c, "ek_val %s = ek_bool(%s);\n", t, core);
    }
    return (lower_val){ t, 1 };
}

/* The applied type the service specializes over: the receiver's row
   type at a receiver or free call, the arrow's parameter (or the
   Result's payload of a from_json link) at a type-head call -- each
   under the instantiation the body being emitted specializes. */
static const ek_type *spec_type(lower_ctx *c, const ek_node *e,
                                const ek_node *callee, const ek_ck_row *frow,
                                lower_cmp_form form, const char *name)
{
    const ek_ck_row *row;
    const ek_type *f;

    if (form == CF_MEMBER) {
        row = callee->u.field.recv ? lower_row(c, callee->u.field.recv)
                                   : NULL;
        return row ? lower_type_in(c, row->type) : NULL;
    }
    if (form == CF_FREE) {
        row = e->u.call.args.count ? lower_row(c, e->u.call.args.data[0])
                                   : NULL;
        return row ? lower_type_in(c, row->type) : NULL;
    }
    f = frow ? lower_type_in(c, frow->type) : NULL;
    if (!f || f->kind != EK_T_FN || f->u.fn.count < 1) {
        return NULL;
    }
    if (strcmp(name, "from_json") == 0) {
        const ek_type *res = f->u.fn.result;
        if (res && res->kind == EK_T_NOMINAL && res->u.nominal.argc == 2) {
            return res->u.nominal.args[0];
        }
        return NULL;
    }
    return f->u.fn.params[0];
}

/* The unsupported arity: everything still evaluates and dies. */
static lower_val discard_all(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             lower_cmp_form form)
{
    size_t n = (size_t)(form == CF_MEMBER) + e->u.call.args.count;
    const char **argv = call_temps(c, scope, e, callee, form, n);

    if (argv) {
        release_temps(c, argv, n);
    }
    return lower_val_unit();
}

lower_val lower_derived_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_node *callee,
                             const ek_ck_row *frow, lower_cmp_form form)
{
    const char *name = callee->kind == EK_N_FIELD
                           ? callee->u.field.name
                           : (callee->u.path.count
                                  ? callee->u.path.data[0] : "");
    const ek_type *spec;
    lower_dsvc_kind kind;
    const char *svc;
    const char **argv;
    size_t n, want;
    int family = cmp_family(name);
    const char *t;
    lower_scalar sk;

    if (strcmp(name, "show") == 0) {
        kind = DSV_SHOW;
    } else if (strcmp(name, "inspect") == 0) {
        kind = DSV_INSPECT;
    } else if (strcmp(name, "to_json") == 0) {
        kind = DSV_TOJSON;
    } else if (strcmp(name, "from_json") == 0) {
        kind = DSV_FROMJSON;
    } else if (family) {
        kind = family == 1 ? DSV_EQ : DSV_ORD;
    } else {
        return (lower_val){ NULL, 0 };
    }
    spec = spec_type(c, e, callee, frow, form, name);
    if (!spec) {
        return (lower_val){ NULL, 0 };
    }
    want = written_args(family, form);
    if (e->u.call.args.count != want) {
        lower_error(c, e, "the call of `%s` is malformed", name);
        return discard_all(c, scope, e, callee, form);
    }
    sk = lower_scalar_kind(spec);
    if (family && num_slot(sk, family == 2)) {
        svc = NULL; /* the runtime comparison entries answer directly */
    } else {
        svc = lower_dsvc_request(c, e, kind, spec);
        if (!svc) {
            lower_error(c, e, "the derived `%s` has no lowering here",
                        name);
            return discard_all(c, scope, e, callee, form);
        }
    }
    n = (size_t)(form == CF_MEMBER) + want;
    argv = call_temps(c, scope, e, callee, form, n);
    if (!argv) {
        return lower_val_unit();
    }
    if (family) {
        lower_val out = emit_cmp(c, scope, name, svc, sk, argv);

        release_temps(c, argv, n);
        return out;
    }
    t = lower_temp(c, scope);
    lower_pad(c);
    if (kind == DSV_SHOW || kind == DSV_INSPECT) {
        lower_emitf(c, "ek_val %s = ek_val_ref(%s(%s));\n", t, svc, argv[0]);
    } else {
        lower_emitf(c, "ek_val %s = %s(%s);\n", t, svc, argv[0]);
    }
    release_temps(c, argv, n);
    return (lower_val){ t, 1 };
}

/* The comparison service of a binary operator over the value layer's
   two object slots: String and Bytes, whose payloads the runtime's
   numeric comparisons cannot read. Every other operator domain stays
   on the runtime entries; equality over the remaining types arrives as
   the derived member call. NULL hands the operator back. */
const char *lower_derived_cmp(lower_ctx *c, const ek_node *e, ek_binop op)
{
    const ek_type *t = e->u.binary.lhs ? lower_row_type(c, e->u.binary.lhs)
                                       : NULL;
    lower_scalar sk = t ? lower_scalar_kind(t) : EKSC_OTHER;

    if (sk != EKSC_STRING && sk != EKSC_BYTES) {
        return NULL;
    }
    if (op == EK_BIN_EQ || op == EK_BIN_NE) {
        return lower_dsvc_request(c, e, DSV_EQ, t);
    }
    if (op == EK_BIN_LT || op == EK_BIN_LE || op == EK_BIN_GT
        || op == EK_BIN_GE) {
        return lower_dsvc_request(c, e, DSV_ORD, t);
    }
    return NULL;
}
