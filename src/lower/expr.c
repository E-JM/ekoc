#include <string.h>

#include "lower/internal.h"

/* Expressions other than literals and calls: names, operators, and
   fields. Every result is an owned value: operator results are born
   owned, a name read is an owned copy through the helper, and a name
   holding a function is a fresh closure over its static trampoline.
   Operands evaluate to temps in source order and are released once
   their use is emitted. */

/* A descent that is not a tail position of the function being
   emitted. */
lower_val lower_sub(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    int saved = c->in_tail;
    lower_val v;

    c->in_tail = 0;
    v = lower_expr(c, scope, e);
    c->in_tail = saved;
    return v;
}

/* True and False are scalar-represented; the check row's Bool decides.
   Every other constructor lands through the generated struct layer. */
static lower_val ctor_expr(lower_ctx *c, const ek_node *e, const ek_ref *ref)
{
    const ek_type *t = lower_row_type(c, e);
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
    return lower_ctor_value(c, e, ref);
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
        const char *ekt = lower_trampoline(c, ref->symbol);

        return (lower_val){ lower_str(c, "ek_closure_new(%s, "
                                          "ek_val_unit())", ekt), 0 };
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

/* The concatenation entry: `++` joins the runtime families -- strings
   and byte blocks over their objects, lists through the spine append,
   dictionaries through the merge that lets the right operand win.
   *by_obj marks the entries that read the two operands as objects.
   The operator's other domains are outside this lowerer. */
static const char *concat_fn(lower_ctx *c, const ek_node *e, int *by_obj)
{
    const ek_type *t = lower_row_type(c, e);
    const char *n;

    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym) {
        if (t && t->kind == EK_T_LIST) {
            *by_obj = 0;
            return "ek_list_append";
        }
        if (t && t->kind == EK_T_DICT) {
            *by_obj = 0;
            return "ek_dict_merge";
        }
        return NULL;
    }
    n = t->u.nominal.sym->name;
    if (strcmp(n, "String") == 0) {
        *by_obj = 1;
        return "ek_str_concat";
    }
    if (strcmp(n, "Bytes") == 0) {
        *by_obj = 1;
        return "ek_bytes_concat";
    }
    return NULL;
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

/* One right operand: a tail position whose value moves into the
   result temp. The tail marker here is the one the enclosing context
   set, so a self call in the operand reenters the function. */
static void emit_bool_branch(lower_ctx *c, lower_scope *scope,
                             const ek_node *b, const char *r)
{
    lower_val v = lower_expr(c, scope, b);

    if (!v.text) {
        v = lower_val_unit(); /* the tail jumped to retry */
    }
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", r, v.text);
}

/* The logical operators lower by their defining expansion: `a && b`
   is `if a: b else: False` and `a || b` is `if a: True else: b`. The
   right operand evaluates only when the left operand leaves the
   result undecided, and it inherits the tail position. */
static lower_val bool_op(lower_ctx *c, lower_scope *scope,
                         const ek_node *e, int is_and)
{
    unsigned d = c->depth;
    lower_val l = lower_sub(c, scope, e->u.binary.lhs);
    const char *lt = lower_as_temp(c, scope, &l);
    const char *r = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_unit();\n", r);
    lower_pad(c);
    lower_emitf(c, "if (%s.as.i) {\n", lt);
    c->depth = d + 1;
    if (is_and) {
        emit_bool_branch(c, scope, e->u.binary.rhs, r);
    } else {
        lower_pad(c);
        lower_emitf(c, "%s = ek_bool(1);\n", r);
    }
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    if (is_and) {
        lower_pad(c);
        lower_emitf(c, "%s = ek_bool(0);\n", r);
    } else {
        emit_bool_branch(c, scope, e->u.binary.rhs, r);
    }
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", lt);
    return (lower_val){ r, 1 };
}

/* The range construction: the two endpoints and the closed flag
   build the generated struct family of the element slot; the helper
   retains every field, so the value is born owned. */
static lower_val range_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, int closed)
{
    const ek_type *t = lower_row_type(c, e);
    const ek_type *elem = t && t->kind == EK_T_NOMINAL
        && t->u.nominal.argc == 1 ? t->u.nominal.args[0] : NULL;
    lower_scalar slot = lower_scalar_kind(elem);
    lower_gtype *g;
    lower_val l, r;
    const char *lt, *rt, *tname;

    if (slot != EKSC_INT && slot != EKSC_UINT && slot != EKSC_CHAR) {
        lower_error(c, e, "the %s operator is not supported by this "
                          "lowerer",
                    e->u.binary.op == EK_BIN_RANGE_INC ? "`..=`" : "`..`");
        l = lower_sub(c, scope, e->u.binary.lhs);
        r = lower_sub(c, scope, e->u.binary.rhs);
        lt = lower_as_temp(c, scope, &l);
        rt = lower_as_temp(c, scope, &r);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", rt);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", lt);
        return lower_val_unit();
    }
    g = lower_range_family(c, slot);
    if (!g) {
        return lower_val_unit();
    }
    l = lower_sub(c, scope, e->u.binary.lhs);
    r = lower_sub(c, scope, e->u.binary.rhs);
    lt = lower_as_temp(c, scope, &l);
    rt = lower_as_temp(c, scope, &r);
    tname = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(%s, %s, ek_bool(%d));\n", tname,
                g->helper, lt, rt, closed);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", rt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", lt);
    return (lower_val){ tname, 1 };
}

/* The join line: the object families read the operands' references,
   the value families the slots; the result is born owned either way. */
static void emit_concat_call(lower_ctx *c, const char *t, const char *fn,
                             int by_obj, const char *lt, const char *rt)
{
    if (by_obj) {
        lower_emitf(c, "ek_val %s = ek_val_ref(%s(%s.as.ref, "
                       "%s.as.ref));\n", t, fn, lt, rt);
        return;
    }
    lower_emitf(c, "ek_val %s = %s(%s, %s);\n", t, fn, lt, rt);
}

static lower_val binary_expr(lower_ctx *c, lower_scope *scope,
                             const ek_node *e)
{
    if (e->u.binary.op == EK_BIN_AND) {
        return bool_op(c, scope, e, 1);
    }
    if (e->u.binary.op == EK_BIN_OR) {
        return bool_op(c, scope, e, 0);
    }
    if (e->u.binary.op == EK_BIN_RANGE_EXC) {
        return range_expr(c, scope, e, 0);
    }
    if (e->u.binary.op == EK_BIN_RANGE_INC) {
        return range_expr(c, scope, e, 1);
    }
    {
        ek_binop op = e->u.binary.op;
        const char *fn = NULL;
        int derived = 0;
        int by_obj = 0;

        /* the comparison family reads the derived service over the
           value layer's object slots first, every other row keeping
           the runtime comparisons */
        if (op == EK_BIN_EQ || op == EK_BIN_NE || op == EK_BIN_LT
            || op == EK_BIN_LE || op == EK_BIN_GT || op == EK_BIN_GE) {
            fn = lower_derived_cmp(c, e, op);
            derived = fn != NULL;
            if (!fn) {
                fn = num_op(op);
            }
        } else if (op == EK_BIN_CONCAT) {
            fn = concat_fn(c, e, &by_obj);
        } else {
            fn = num_op(op);
        }
        lower_val l = lower_sub(c, scope, e->u.binary.lhs);
        lower_val r = lower_sub(c, scope, e->u.binary.rhs);
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
        if (derived) {
            /* the structural comparison over the object slots answers
               an ordering word or a Bool word */
            if (e->u.binary.op == EK_BIN_EQ) {
                lower_emitf(c, "ek_val %s = ek_bool(%s(%s, %s));\n", t, fn,
                            lt, rt);
            } else if (e->u.binary.op == EK_BIN_NE) {
                lower_emitf(c, "ek_val %s = ek_bool(!%s(%s, %s));\n", t, fn,
                            lt, rt);
            } else if (e->u.binary.op == EK_BIN_LT) {
                lower_emitf(c, "ek_val %s = ek_bool(%s(%s, %s) < 0);\n", t,
                            fn, lt, rt);
            } else if (e->u.binary.op == EK_BIN_LE) {
                lower_emitf(c, "ek_val %s = ek_bool(%s(%s, %s) <= 0);\n", t,
                            fn, lt, rt);
            } else if (e->u.binary.op == EK_BIN_GT) {
                lower_emitf(c, "ek_val %s = ek_bool(%s(%s, %s) > 0);\n", t,
                            fn, lt, rt);
            } else {
                lower_emitf(c, "ek_val %s = ek_bool(%s(%s, %s) >= 0);\n", t,
                            fn, lt, rt);
            }
        } else if (e->u.binary.op == EK_BIN_CONCAT) {
            emit_concat_call(c, t, fn, by_obj, lt, rt);
        } else {
            lower_emitf(c, "ek_val %s = %s(%s, %s);\n", t, fn, lt, rt);
        }
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", rt);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", lt);
        return (lower_val){ t, 1 };
    }
}

static lower_val unary_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    lower_val v = lower_sub(c, scope, e->u.wrap.inner);
    const char *t = lower_as_temp(c, scope, &v);
    const char *r = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_num_neg(%s);\n", r, t);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return (lower_val){ r, 1 };
}

/* A field access: the receiver lowers to a temp, the read is an owned
   copy out of the generated struct, and the receiver temp dies. */
static lower_val field_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    const ek_ck_row *row = lower_row(c, e);
    const ek_type *rt = lower_row_type(c, e->u.field.recv);
    lower_val v;
    const char *t, *r, *slot;

    if (!row || row->reading != EK_RD_FIELD || !rt) {
        lower_error(c, e, "the %s is not supported by this lowerer",
                    lower_construct(e));
        return lower_val_unit();
    }
    v = lower_sub(c, scope, e->u.field.recv);
    t = lower_as_temp(c, scope, &v);
    slot = lower_field_slot(c, e, rt, t, e->u.field.name);
    if (!slot) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
        return lower_val_unit();
    }
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_keep(%s);\n", r, slot);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return (lower_val){ r, 1 };
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

static lower_val lower_block_expr(lower_ctx *c, lower_scope *scope,
                                  const ek_node *e)
{
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
        return lower_string_expr(c, scope, e);
    case EK_N_NAME:
        return name_expr(c, scope, e);
    case EK_N_PAREN:
        return lower_expr(c, scope, e->u.wrap.inner);
    case EK_N_UNARY:
        return unary_expr(c, scope, e);
    case EK_N_BINARY:
        return binary_expr(c, scope, e);
    case EK_N_CALL:
        return lower_call_expr(c, scope, e);
    case EK_N_APPLY:
        return lower_ctor_apply(c, scope, e);
    case EK_N_COMPOSITE:
        return lower_composite(c, scope, e);
    case EK_N_FIELD:
        return field_expr(c, scope, e);
    case EK_N_IF: {
        lower_val v;

        lower_if(c, scope, e, &v);
        return v;
    }
    case EK_N_CASE:
        return lower_case(c, scope, e);
    case EK_N_LAMBDA:
        return lower_lambda(c, scope, e);
    case EK_N_INVERT:
        return lower_invert(c, scope, e, NULL, 0);
    case EK_N_BLOCK:
        return lower_block_expr(c, scope, e);
    default:
        lower_error(c, e, "the %s is not supported by this lowerer",
                    lower_construct(e));
        return lower_val_unit();
    }
}
