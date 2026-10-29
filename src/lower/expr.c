#include <string.h>

#include "lower/internal.h"

/* Expressions other than literals: names, operators, and calls. Every
   result is an owned value: operator results are born owned, a name
   read is an owned copy through the helper, and a call result is
   owned. Operands and arguments evaluate to temps in source order and
   are released once their use is emitted. */

/* True and False are scalar-represented; the check row's Bool decides. */
static lower_val ctor_expr(lower_ctx *c, const ek_node *e, const ek_ref *ref)
{
    const ek_ck_row *row = lower_row(c, e);
    const ek_type *t = row ? row->type : NULL;
    const ek_symbol *sym = ref->symbol;
    int is_bool = t && t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && strcmp(t->u.nominal.sym->name, "Bool") == 0;

    if (is_bool && sym && sym->owner && strcmp(sym->owner, "Bool") == 0) {
        if (strcmp(sym->name, "True") == 0) {
            return (lower_val){ "ek_bool(1)", 0 };
        }
        if (strcmp(sym->name, "False") == 0) {
            return (lower_val){ "ek_bool(0)", 0 };
        }
    }
    lower_error(c, e, "the constructor `%s` is not supported by this "
                      "lowerer", sym ? sym->name : "?");
    return lower_val_unit();
}

static lower_val name_expr(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    const ek_ref *ref;
    const lower_local *loc;

    if (e->u.path.count == 1 && strcmp(e->u.path.data[0], "_") == 0) {
        return lower_val_unit(); /* the unit value at value position */
    }
    ref = lower_ref(c, e);
    if (!ref) {
        return lower_val_unit(); /* resolve diagnosed the reference */
    }
    if (ref->kind == EK_SYM_LOCAL || ref->kind == EK_SYM_PARAM) {
        if (!ref->symbol) {
            return lower_val_unit();
        }
        loc = scope_find_local(scope, ref->symbol->name);
        if (!loc) {
            lower_error(c, e, "the binding of `%s` has no lowering",
                        ref->symbol->name);
            return lower_val_unit();
        }
        return (lower_val){ lower_str(c, "ek_keep(%s)", loc->cname), 0 };
    }
    if (ref->kind == EK_SYM_CONSTRUCTOR) {
        return ctor_expr(c, e, ref);
    }
    if (ref->kind == EK_SYM_FUNC && ref->symbol) {
        lower_error(c, e, "the function value `%s` is not supported by "
                          "this lowerer", ref->symbol->name);
        return lower_val_unit();
    }
    lower_error(c, e, "the reference to `%s` is not supported by this "
                      "lowerer",
                e->u.path.count == 1 ? e->u.path.data[0] : "?");
    return lower_val_unit();
}

static const char *num_op(ek_binop op)
{
    switch (op) {
    case EK_BIN_ADD: return "ek_num_add";
    case EK_BIN_SUB: return "ek_num_sub";
    case EK_BIN_MUL: return "ek_num_mul";
    case EK_BIN_DIV: return "ek_num_div";
    case EK_BIN_REM: return "ek_num_mod";
    case EK_BIN_EQ: return "ek_num_eq";
    case EK_BIN_NE: return "ek_num_ne";
    case EK_BIN_LT: return "ek_num_lt";
    case EK_BIN_LE: return "ek_num_le";
    case EK_BIN_GT: return "ek_num_gt";
    case EK_BIN_GE: return "ek_num_ge";
    default: return NULL;
    }
}

static const char *op_spelling(ek_binop op)
{
    switch (op) {
    case EK_BIN_ADD: return "`+`";
    case EK_BIN_SUB: return "`-`";
    case EK_BIN_MUL: return "`*`";
    case EK_BIN_DIV: return "`/`";
    case EK_BIN_REM: return "`%`";
    case EK_BIN_CONCAT: return "`++`";
    case EK_BIN_EQ: return "`==`";
    case EK_BIN_NE: return "`!=`";
    case EK_BIN_LT: return "`<`";
    case EK_BIN_LE: return "`<=`";
    case EK_BIN_GT: return "`>`";
    case EK_BIN_GE: return "`>=`";
    case EK_BIN_AND: return "`&&`";
    case EK_BIN_OR: return "`||`";
    case EK_BIN_RANGE_EXC: return "`..`";
    case EK_BIN_RANGE_INC: return "`..=`";
    }
    return "?";
}

static lower_val binary_expr(lower_ctx *c, lower_scope *scope,
                             const ek_node *e)
{
    const char *fn = num_op(e->u.binary.op);
    lower_val l = lower_expr(c, scope, e->u.binary.lhs);
    lower_val r = lower_expr(c, scope, e->u.binary.rhs);
    const char *lt = lower_as_temp(c, scope, &l);
    const char *rt = lower_as_temp(c, scope, &r);
    const char *t;

    if (!fn) {
        lower_error(c, e, "the %s operator is not supported by this "
                          "lowerer", op_spelling(e->u.binary.op));
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", rt);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", lt);
        return lower_val_unit();
    }
    t = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(%s, %s);\n", t, fn, lt, rt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", rt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", lt);
    return (lower_val){ t, 1 };
}

static lower_val unary_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    lower_val v = lower_expr(c, scope, e->u.wrap.inner);
    const char *t = lower_as_temp(c, scope, &v);
    const char *r = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_num_neg(%s);\n", r, t);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return (lower_val){ r, 1 };
}

/* The panic service: the free-call form of the String member, its
   reading selected by check. */
static int is_panic_call(lower_ctx *c, const ek_node *e)
{
    const ek_node *callee = e->u.call.callee;
    const ek_ck_row *row;

    if (!callee || callee->kind != EK_N_NAME || callee->u.path.count != 1
        || strcmp(callee->u.path.data[0], "panic") != 0) {
        return 0;
    }
    row = lower_row(c, callee);
    return row && row->reading == EK_RD_DERIVED;
}

static lower_val panic_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    const ek_nlist *args = &e->u.call.args;
    lower_val a;
    const char *at;

    if (args->count != 1) {
        lower_error(c, e, "the call of `panic` is malformed");
        return lower_val_unit();
    }
    a = lower_expr(c, scope, args->data[0]);
    at = lower_as_temp(c, scope, &a);
    lower_pad(c);
    lower_emitf(c, "ek_panic_value(%s);\n", at);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", at);
    return lower_val_unit();
}

static lower_val direct_call(lower_ctx *c, lower_scope *scope,
                             const ek_node *e, const ek_ref *ref)
{
    const ek_symbol *sym = ref->symbol;
    const ek_nlist *args = &e->u.call.args;
    const ek_node *ann = sym->type;
    const char **argv;
    const char *t;
    size_t n, cap, i;

    if (!ann || ann->kind != EK_N_T_FN) {
        lower_error(c, e, "the signature of `%s` has no lowering",
                    sym->name);
        return lower_val_unit();
    }
    n = ann->u.t_fn.params.count;
    cap = 0;
    argv = lower_grow(c, NULL, &cap, n, sizeof *argv);
    if (!argv && n) {
        c->oom = 1;
        return lower_val_unit();
    }
    for (i = 0; i < n && i < args->count; i++) {
        lower_val a = lower_expr(c, scope, args->data[i]);

        argv[i] = lower_as_temp(c, scope, &a);
    }
    for (; i < n; i++) {
        lower_val a = lower_val_unit();

        argv[i] = lower_as_temp(c, scope, &a); /* a malformed call */
    }
    lower_enqueue(c, sym);
    t = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", t, lower_mangled(c, sym));
    for (i = 0; i < n; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
    }
    lower_emitf(c, ");\n");
    for (i = n; i-- > 0;) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i]);
    }
    return (lower_val){ t, 1 };
}

static lower_val call_expr(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    const ek_ref *ref;

    if (is_panic_call(c, e)) {
        return panic_call(c, scope, e);
    }
    ref = lower_ref(c, e);
    if (!ref && e->u.call.callee) {
        ref = lower_ref(c, e->u.call.callee);
    }
    if (ref && ref->kind == EK_SYM_FUNC && ref->symbol) {
        return direct_call(c, scope, e, ref);
    }
    if (ref) {
        lower_error(c, e, "the call is not supported by this lowerer");
        return lower_val_unit();
    }
    return lower_val_unit(); /* resolve diagnosed the callee */
}

/* The written name of a construct, for the unsupported-construct
   diagnostics. */
const char *lower_construct(const ek_node *n)
{
    switch (n->kind) {
    case EK_N_CASE: return "`case` expression";
    case EK_N_LAMBDA: return "lambda";
    case EK_N_COMPOSITE: return "composite literal";
    case EK_N_APPLY: return "constructor application";
    case EK_N_FIELD: return "field access";
    case EK_N_INVERT: return "`<-` binding";
    default: return "construct";
    }
}

lower_val lower_expr(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    if (!e) {
        return lower_val_unit();
    }
    switch (e->kind) {
    case EK_N_INT_LIT:
        return lower_int_lit(c, e);
    case EK_N_FLOAT_LIT:
        return lower_float_lit(c, e);
    case EK_N_CHAR_LIT:
        return lower_char_lit(c, e);
    case EK_N_STRING_LIT:
        return lower_string_expr(c, e);
    case EK_N_NAME:
        return name_expr(c, scope, e);
    case EK_N_PAREN:
        return lower_expr(c, scope, e->u.wrap.inner);
    case EK_N_UNARY:
        return unary_expr(c, scope, e);
    case EK_N_BINARY:
        return binary_expr(c, scope, e);
    case EK_N_CALL:
        return call_expr(c, scope, e);
    case EK_N_IF: {
        lower_val v;

        lower_if(c, scope, e, &v);
        return v;
    }
    case EK_N_BLOCK: {
        lower_scope s;
        lower_val v;
        unsigned d = c->depth;

        memset(&s, 0, sizeof s);
        s.parent = scope;
        c->depth = d + 1;
        v = lower_block_value(c, &s, e);
        c->depth = d;
        lower_release_scope(c, &s);
        return v;
    }
    default:
        lower_error(c, e, "the %s is not supported by this lowerer",
                    lower_construct(e));
        return lower_val_unit();
    }
}
