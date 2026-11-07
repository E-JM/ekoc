#include "lower/internal.h"

/* The comparison service behind a binary operator over the value
   layer's two object slots: String and Bytes, whose payloads the
   runtime's numeric comparisons cannot read. Every other operator
   domain stays on the runtime entries; equality over the remaining
   types arrives as the derived member call. NULL hands the operator
   back. */
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
