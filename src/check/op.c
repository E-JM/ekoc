#include <string.h>

#include "check/internal.h"

/* Operator typing (Clauses 7.8.3-7.8.6). Every operator is notation
   for a type-namespace member; both operands MUST have the same type,
   the left fixes it, and the right must adapt. Operands whose type the
   bridge ruling leaves open defer every domain check. */

static int eq_domain(const ek_type *t)
{
    return ck_is_numeric(t) || ck_is_machine_word(t) || ck_is_std(t, "Bool")
        || ck_is_std(t, "Char") || ck_is_std(t, "String")
        || ck_is_std(t, "Bytes");
}

static int ord_domain(const ek_type *t)
{
    return ck_is_numeric(t) || ck_is_machine_word(t) || ck_is_std(t, "Char")
        || ck_is_std(t, "String") || ck_is_std(t, "Bytes");
}

static int concat_domain(const ek_type *t)
{
    return t->kind == EK_T_LIST || t->kind == EK_T_DICT
        || ck_is_std(t, "String") || ck_is_std(t, "Bytes");
}

/* True when the operand's type is still open, so no domain check
   applies (the placeholder-std ruling). */
static int open_type(const ek_type *t)
{
    t = ck_prune(t);
    return !t || t->kind == EK_T_IVAR;
}

static const char *op_spelling(ek_binop op)
{
    switch (op) {
    case EK_BIN_ADD: return "+";
    case EK_BIN_SUB: return "-";
    case EK_BIN_MUL: return "*";
    case EK_BIN_DIV: return "/";
    case EK_BIN_REM: return "%";
    case EK_BIN_CONCAT: return "++";
    case EK_BIN_EQ: return "==";
    case EK_BIN_NE: return "!=";
    case EK_BIN_LT: return "<";
    case EK_BIN_LE: return "<=";
    case EK_BIN_GT: return ">";
    case EK_BIN_GE: return ">=";
    case EK_BIN_AND: return "&&";
    case EK_BIN_OR: return "||";
    case EK_BIN_RANGE_EXC: return "..";
    default: return "..=";
    }
}

static int is_arith(ek_binop op)
{
    return op == EK_BIN_ADD || op == EK_BIN_SUB || op == EK_BIN_MUL
        || op == EK_BIN_DIV || op == EK_BIN_REM;
}

static int is_range(ek_binop op)
{
    return op == EK_BIN_RANGE_EXC || op == EK_BIN_RANGE_INC;
}

/* The expected type an operand takes: the numeric (or list, or range
   element) expectation flows into literals (Clause 5.7.4). */
static const ek_type *operand_expected(const ek_type *expected)
{
    const ek_type *e = ck_prune(expected);

    if (!e || e->kind == EK_T_IVAR) {
        return expected;
    }
    if (ck_is_numeric(e) || ck_is_machine_word(e) || e->kind == EK_T_LIST
        || e->kind == EK_T_DICT) {
        return e;
    }
    return NULL;
}

const ek_type *ck_unary(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_node *inner = e->u.wrap.inner;
    const ek_type *t;

    if (inner && inner->kind == EK_N_INT_LIT) {
        return ck_place(c, inner, expected, 1);
    }
    t = ck_expr(c, inner, ck_is_numeric(ck_prune(expected)) ? expected : NULL);
    if (!open_type(t) && !ck_is_numeric(t)) {
        ck_error(c, e, "E-no-operator",
                 "unary `-` is defined on the numeric types, not %s "
                 "(Clause 7.8.4)", ck_type_name(c, t));
    }
    return t;
}

const ek_type *ck_binary(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    ek_binop op = e->u.binary.op;
    const ek_node *a = e->u.binary.lhs;
    const ek_node *b = e->u.binary.rhs;
    const ek_type *lt, *rt, *same;

    if (op == EK_BIN_AND || op == EK_BIN_OR) {
        lt = ck_expr(c, a, ck_std_type(c, "Bool"));
        rt = ck_expr(c, b, ck_std_type(c, "Bool"));
        if (!open_type(lt) && !ck_is_std(ck_prune(lt), "Bool")) {
            ck_error(c, e, "E-type-mismatch",
                     "`%s` takes `Bool` operands, not %s (Clause 7.8.5)",
                     op_spelling(op), ck_type_name(c, lt));
        }
        return ck_std_type(c, "Bool");
    }
    if (is_range(op)) {
        const ek_type *re = ck_prune(expected);
        const ek_type *end = NULL;
        const ek_type *elem;
        const ek_type *arg1;
        const ek_symbol *rsym;

        if (re && re->kind == EK_T_NOMINAL && ck_is_std(re, "Range")
            && re->u.nominal.argc == 1) {
            end = re->u.nominal.args[0];
        }
        lt = ck_expr(c, a, end);
        rt = ck_expr(c, b, lt ? lt : end);
        same = ck_unify(c, lt, rt, 1);
        if (!same && lt && rt) {
            ck_error(c, e, "E-type-mismatch",
                     "the range endpoints are %s and %s (Clause 6.5.6)",
                     ck_type_name(c, lt), ck_type_name(c, rt));
        }
        elem = same ? same : ck_prune(lt);
        if (!open_type(elem) && !ck_is_std(elem, "Int")
            && !ck_is_std(elem, "UInt") && !ck_is_std(elem, "Char")) {
            ck_error(c, e, "E-no-operator",
                     "`%s` takes `Int`, `UInt`, or `Char` endpoints, not "
                     "%s (Clauses 6.5.6, 7.8.3)", op_spelling(op),
                     ck_type_name(c, elem));
        }
        rsym = ck_std_symbol(c, "Range");
        arg1 = elem;
        return rsym ? ck_nominal(c, rsym, &arg1, 1) : NULL;
    }
    if (is_arith(op) || op == EK_BIN_CONCAT) {
        lt = ck_expr(c, a, operand_expected(expected));
        rt = ck_expr(c, b, lt);
        same = ck_unify(c, lt, rt, 1);
        if (!same) {
            same = ck_prune(lt); /* judge the domain on the left type */
        }
        if (same && !open_type(same)) {
            if (is_arith(op) && !ck_is_numeric(same)) {
                ck_error(c, e, "E-no-operator",
                         "`%s` is defined on the numeric types with no "
                         "implicit conversion (Clause 7.8.4)",
                         op_spelling(op));
            } else if (op == EK_BIN_CONCAT && !concat_domain(same)) {
                ck_error(c, e, "E-no-operator",
                         "`++` is defined on lists, dictionaries, `String`, "
                         "and `Bytes`, not %s (Clause 7.8.6)",
                         ck_type_name(c, same));
            }
        }
        return same ? same : ck_prune(lt);
    }
    /* Equality and ordering. */
    lt = ck_expr(c, a, NULL);
    rt = ck_expr(c, b, lt);
    same = ck_unify(c, lt, rt, 1);
    if (!same) {
        same = ck_prune(lt); /* judge the domain on the left type */
    }
    if (same && !open_type(same)) {
        if (op == EK_BIN_EQ || op == EK_BIN_NE) {
            /* A type variable may compare: `==` is the derived eq of
               Clause 7.6.6. */
            if (!eq_domain(same) && same->kind != EK_T_TVAR) {
                ck_error(c, e, "E-no-operator",
                         "`==` and `!=` are defined on the value layer "
                         "only; equality on %s is the member `%s.eq` "
                         "(Clause 7.8.3)", ck_type_name(c, same),
                         ck_type_name(c, same));
            }
        } else if (!ord_domain(same)) {
            ck_error(c, e, "E-no-operator",
                     "`%s` is defined on the value layer with an order, "
                     "not on %s (Clause 7.8.3)", op_spelling(op),
                     ck_type_name(c, same));
        }
    }
    return ck_std_type(c, "Bool");
}
