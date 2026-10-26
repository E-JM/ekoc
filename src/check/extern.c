#include <string.h>

#include "check/internal.h"

/* The compile-time checks over the root's declarations: the extern
   form and signature rules, the marshalling whitelist, the ctype field
   predicate, the reserved symbols, and the open-bit position rule over
   type declarations. */

/* The machine words: the integer words plus the two floats. */
static int is_word(const ek_type *t)
{
    return ck_is_machine_word(t) || ck_is_std(t, "F32")
        || ck_is_std(t, "F64");
}

static int is_ctype(const ek_type *t)
{
    t = ck_prune(t);
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || !t->u.nominal.sym->decl) {
        return 0;
    }
    return t->u.nominal.sym->decl->kind == EK_N_CTYPE_DECL;
}

/* One type of the marshalling whitelist. result is 1 in a result
   position: `Bytes` and a closure are parameter-only, `_` is
   result-only. Inference variables stay opaque (the bridge). */
static int marshals(ck_ctx *c, const ek_type *t, int result)
{
    size_t i;

    t = ck_prune(t);
    if (!t) {
        return 1;
    }
    switch (t->kind) {
    case EK_T_UNIT:
        return result;
    case EK_T_IVAR:
        return 1;
    case EK_T_FN:
        if (result) {
            return 0;
        }
        for (i = 0; i < t->u.fn.count; i++) {
            if (!marshals(c, t->u.fn.params[i], 0)) {
                return 0;
            }
        }
        return marshals(c, t->u.fn.result, 1);
    case EK_T_NOMINAL:
        if (is_word(t) || ck_is_std(t, "Bool") || ck_is_std(t, "Int")
            || ck_is_std(t, "UInt") || ck_is_std(t, "Float")
            || ck_is_std(t, "String")) {
            return 1;
        }
        if (ck_is_std(t, "Bytes")) {
            return !result;
        }
        return is_ctype(t);
    default:
        return 0; /* list, dict, tuple, record, variable, Never */
    }
}

/* True when an open arrow appears anywhere in the annotation. */
static int has_open_arrow(const ek_node *t)
{
    size_t i;

    if (!t) {
        return 0;
    }
    switch (t->kind) {
    case EK_N_T_FN:
        if (t->u.t_fn.arrow == EK_ARROW_OPEN) {
            return 1;
        }
        for (i = 0; i < t->u.t_fn.params.count; i++) {
            if (has_open_arrow(t->u.t_fn.params.data[i]->u.t_field.type)) {
                return 1;
            }
        }
        return has_open_arrow(t->u.t_fn.result);
    case EK_N_T_LIST:
    case EK_N_T_PAREN:
        return has_open_arrow(t->u.wrap.inner);
    case EK_N_T_DICT:
        return has_open_arrow(t->u.pair.a) || has_open_arrow(t->u.pair.b);
    case EK_N_T_APPLY:
        for (i = 0; i < t->u.t_apply.args.count; i++) {
            if (has_open_arrow(t->u.t_apply.args.data[i])) {
                return 1;
            }
        }
        return 0;
    case EK_N_T_RECORD:
        for (i = 0; i < t->u.kids.count; i++) {
            if (has_open_arrow(t->u.kids.data[i]->u.t_field.type)) {
                return 1;
            }
        }
        return 0;
    default:
        return 0;
    }
}

/* The own arrow is `!->` or `!`, or `->` with `Pure`; an open arrow is
   not admitted. The open-arrow violation carries no published code and
   reuses E-extern-arrow. */
static void check_arrow(ck_ctx *c, const ek_node *dn, const ek_node *ann,
                        int pure)
{
    if (!ann || ann->kind != EK_N_T_FN) {
        ck_error(c, dn, "E-extern-arrow",
                 "an extern signature is a function type (Clause 10.2.1)");
        return;
    }
    if (has_open_arrow(ann)) {
        ck_error(c, dn, "E-extern-arrow",
                 "an open arrow is not admitted on an extern declaration "
                 "(Clauses 10.2.3, 7.9.3)");
        return;
    }
    if (ann->u.t_fn.arrow == EK_ARROW_PURE && !pure) {
        ck_error(c, dn, "E-extern-arrow",
                 "an extern without `Pure` carries a world-touching arrow "
                 "(Clause 10.2.1)");
    } else if (ann->u.t_fn.arrow == EK_ARROW_WORLD && pure) {
        ck_error(c, dn, "E-extern-arrow",
                 "a `Pure` extern carries a pure arrow (Clause 10.2.1)");
    }
}

/* The whitelist over the annotation, so the diagnostic lands on the
   offending type. */
static void check_type(ck_ctx *c, const ek_node *t, int result)
{
    const ek_type *et;
    size_t i, n;

    if (!t || c->oom) {
        return;
    }
    switch (t->kind) {
    case EK_N_T_FN:
        if (result) {
            ck_error(c, t, "E-extern-type",
                     "a closure is parameter-only in an extern signature "
                     "(Clauses 10.2.3, 10.3.5)");
            return;
        }
        for (i = 0, n = t->u.t_fn.params.count; i < n; i++) {
            check_type(c, t->u.t_fn.params.data[i]->u.t_field.type, 0);
        }
        check_type(c, t->u.t_fn.result, 1);
        return;
    case EK_N_T_PAREN:
        check_type(c, t->u.wrap.inner, result);
        return;
    case EK_N_T_NAME:
    case EK_N_T_APPLY:
        et = ck_elaborate(c, t, NULL);
        if (!marshals(c, et, result)) {
            ck_error(c, t, "E-extern-type",
                     "this type is outside the marshalling whitelist "
                     "(Clauses 10.2.3, 10.3.5)");
        }
        return;
    default:
        ck_error(c, t, "E-extern-type",
                 "this type is outside the marshalling whitelist "
                 "(Clauses 10.2.3, 10.3.5)");
        return;
    }
}

/* Symbols beginning `eko_` belong to the host. */
static void check_symbol(ck_ctx *c, const ek_node *dn, const char *sym)
{
    if (sym && strncmp(sym, "eko_", 4) == 0) {
        ck_error(c, dn, "E-reserved-symbol",
                 "the symbol `%s` is reserved to the host (Clause 10.6)",
                 sym);
    }
}

/* The ctype field predicate: every field is a machine word or another
   ctype record. */
static void check_ctype(ck_ctx *c, const ek_node *d)
{
    const ek_node *rec = d->u.decl.body;
    size_t i, n;

    if (!rec || rec->kind != EK_N_T_RECORD) {
        return; /* a malformed rhs is the parser's finding */
    }
    ck_open_positions(c, rec, 0);
    for (i = 0, n = rec->u.kids.count; i < n; i++) {
        const ek_node *tn = rec->u.kids.data[i]->u.t_field.type;
        const ek_type *ft = ck_elaborate(c, tn, NULL);

        if (!is_word(ft) && !is_ctype(ft)) {
            ck_error(c, tn, "E-ctype-field",
                     "`%s` is neither a machine word nor a ctype record "
                     "(Clause 10.3.3)", ck_type_name(c, ft));
        }
    }
}

/* The open bit generalizes at no type declaration: every position is
   E-open-bit-position. */
static void check_type_decl(ck_ctx *c, const ek_node *d)
{
    size_t i;

    if (d->u.type_decl.rhs == EK_TYPE_RHS_RECORD
        || d->u.type_decl.rhs == EK_TYPE_RHS_TUPLE) {
        ck_open_positions(c, d->u.type_decl.type, 0);
        return;
    }
    for (i = 0; i < d->u.type_decl.variants.count; i++) {
        ck_open_positions(c, d->u.type_decl.variants.data[i]->u.variant
                                  .payload, 0);
    }
}

void ck_externs(ck_ctx *c)
{
    const ek_node *mod = c->root->ast;
    size_t i, n;

    if (!mod || mod->kind != EK_N_MODULE) {
        return;
    }
    for (i = 0, n = mod->u.kids.count; i < n && !c->oom; i++) {
        const ek_node *d = mod->u.kids.data[i];

        switch (d->kind) {
        case EK_N_EXTERN_DECL:
            check_symbol(c, d, d->u.extern_decl.symbol);
            check_arrow(c, d, d->u.extern_decl.type, d->u.extern_decl.pure);
            check_type(c, d->u.extern_decl.type, 0);
            break;
        case EK_N_CTYPE_DECL:
            check_ctype(c, d);
            break;
        case EK_N_TYPE_DECL:
            check_type_decl(c, d);
            break;
        default:
            break;
        }
    }
}
