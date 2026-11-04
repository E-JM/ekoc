#include <string.h>

#include "lower/internal.h"

/* The operator expressions: the numeric rows, the derived
   comparisons, the concatenation family, the logical expansions, the
   range constructions, and negation. Operands lower to temps through
   the sub entry, and every release pairs with its retain. */

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

lower_val lower_binary_expr(lower_ctx *c, lower_scope *scope,
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

lower_val lower_unary_expr(lower_ctx *c, lower_scope *scope,
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
