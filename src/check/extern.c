#include <string.h>

#include "check/internal.h"
#include "support/buf.h"

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

/* The opaque pointer field spelling (Clause 10.3.3). */
static int is_c_ptr_void(const ek_node *tn)
{
    tn = tn && tn->kind == EK_N_T_PAREN ? tn->u.wrap.inner : tn;
    return tn && tn->kind == EK_N_T_NAME && tn->u.path.count == 3
        && strcmp(tn->u.path.data[0], "C") == 0
        && strcmp(tn->u.path.data[1], "Ptr") == 0
        && strcmp(tn->u.path.data[2], "Void") == 0;
}

/* The written spelling of a field's type node, for the field predicate
   the elaborated form cannot name: the bridge reads as `?`. */
static const char *written_type(const ek_node *tn)
{
    ek_buf b;
    size_t i;

    tn = tn && tn->kind == EK_N_T_PAREN ? tn->u.wrap.inner : tn;
    if (!tn || tn->kind != EK_N_T_NAME || tn->u.path.count == 0) {
        return NULL;
    }
    ek_buf_init(&b);
    for (i = 0; i < tn->u.path.count; i++) {
        if (i && !ek_buf_append_char(&b, '.')) {
            break;
        }
        if (!ek_buf_append_str(&b, tn->u.path.data[i])) {
            break;
        }
    }
    return b.data != NULL ? b.data : NULL;
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
/* The machine-word names, the boundary's element domain. */
static int is_word_name(const char *n)
{
    static const char *words[] = {"U8", "U16", "U32", "U64",
                                  "I8", "I16", "I32", "I64", "F32", "F64"};
    size_t i;

    for (i = 0; i < sizeof words / sizeof *words; i++) {
        if (strcmp(n, words[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The read-borrow spelling C.In T: parameter-only (Clause 10.3.4). */
static int is_cin(const ek_node *tn, int result)
{
    return !result && tn && tn->kind == EK_N_T_APPLY
        && tn->u.t_apply.head
        && tn->u.t_apply.head->kind == EK_N_T_NAME
        && tn->u.t_apply.head->u.path.count == 2
        && strcmp(tn->u.t_apply.head->u.path.data[0], "C") == 0
        && strcmp(tn->u.t_apply.head->u.path.data[1], "In") == 0
        && tn->u.t_apply.args.count == 1;
}

/* The element-array row of the whitelist: C.In []T with T a machine
   word, parameter-only (Clause 10.3.5). */
static int is_cin_list(const ek_node *tn, int result)
{
    const ek_node *arg;

    if (result || !tn || tn->kind != EK_N_T_APPLY
        || !tn->u.t_apply.head
        || tn->u.t_apply.head->kind != EK_N_T_NAME
        || tn->u.t_apply.head->u.path.count != 2
        || strcmp(tn->u.t_apply.head->u.path.data[0], "C") != 0
        || strcmp(tn->u.t_apply.head->u.path.data[1], "In") != 0
        || tn->u.t_apply.args.count != 1) {
        return 0;
    }
    arg = tn->u.t_apply.args.data[0];
    arg = arg && arg->kind == EK_N_T_PAREN ? arg->u.wrap.inner : arg;
    return arg && arg->kind == EK_N_T_LIST
        && arg->u.wrap.inner
        && arg->u.wrap.inner->kind == EK_N_T_NAME
        && arg->u.wrap.inner->u.path.count == 1
        && is_word_name(arg->u.wrap.inner->u.path.data[0]);
}

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
        if (is_cin_list(t, result)) {
            return;
        }
        if (is_cin(t, result)) {
            /* The read-borrow row: the payload must be a ctype record
               (Clause 10.3.4). */
            const ek_node *arg = t->u.t_apply.args.data[0];

            arg = arg && arg->kind == EK_N_T_PAREN ? arg->u.wrap.inner
                                                   : arg;
            et = ck_elaborate(c, arg, NULL);
            if (!is_ctype(et)) {
                ck_error(c, t, "E-extern-type",
                         "`C.In` takes a ctype record (Clauses 10.2.3, "
                         "10.3.4)");
            }
            return;
        }
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
        const char *written = written_type(tn);

        if (!is_word(ft) && !is_ctype(ft) && !is_c_ptr_void(tn)) {
            ck_error(c, tn, "E-ctype-field",
                     "`%s` is neither a machine word, a ctype record, "
                     "nor C.Ptr.Void (Clause 10.3.3)",
                     written ? written : ck_type_name(c, ft));
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
    const ek_node *mod = c->cur->ast;
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
