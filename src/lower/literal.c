#include <math.h>
#include <string.h>

#include "lower/internal.h"

/* Literal placement: the check row's nominal name decides the runtime
   slot a literal widens into, and a string literal becomes an escaped
   C string on the heap. */

/* The runtime slot a literal widens into, decided by the check row's
   nominal name. */
enum lit_slot { LIT_INT, LIT_UINT, LIT_FLOAT, LIT_CHAR };

static enum lit_slot lit_slot(lower_ctx *c, const ek_node *e)
{
    const ek_ck_row *row = lower_row(c, e);
    const ek_type *t = row ? row->type : NULL;

    if (t && t->kind == EK_T_NOMINAL && t->u.nominal.sym) {
        const char *n = t->u.nominal.sym->name;

        if (strcmp(n, "UInt") == 0 || strcmp(n, "U8") == 0
            || strcmp(n, "U16") == 0 || strcmp(n, "U32") == 0
            || strcmp(n, "U64") == 0) {
            return LIT_UINT;
        }
        if (strcmp(n, "Float") == 0 || strcmp(n, "F32") == 0
            || strcmp(n, "F64") == 0) {
            return LIT_FLOAT;
        }
        if (strcmp(n, "Char") == 0) {
            return LIT_CHAR;
        }
    }
    return LIT_INT;
}

lower_val lower_int_lit(lower_ctx *c, const ek_node *e)
{
    unsigned long long mag = e->u.int_lit.magnitude;

    if (lit_slot(c, e) == LIT_UINT) {
        return (lower_val){ lower_str(c, "ek_val_uint(%lluULL)", mag), 0 };
    }
    return (lower_val){ lower_str(c, "ek_val_int((int64_t)%lluULL)", mag), 0 };
}

lower_val lower_float_lit(lower_ctx *c, const ek_node *e)
{
    double v = e->u.float_lit.value;
    const char *txt;

    if (isfinite(v)) {
        txt = lower_str(c, "%.17g", v);
    } else {
        txt = e->u.float_lit.text; /* already rejected by check */
    }
    return (lower_val){ lower_str(c, "ek_val_float(%s)", txt), 0 };
}

lower_val lower_char_lit(lower_ctx *c, const ek_node *e)
{
    return (lower_val){ lower_str(c, "ek_val_char(%uu)",
                                  (unsigned)e->u.char_lit.scalar), 0 };
}

lower_val lower_string_expr(lower_ctx *c, const ek_node *e)
{
    ek_buf b;
    lower_val out;

    if (e->u.str.count > 1
        || (e->u.str.count == 1 && (!e->u.str.segs[0].text
                                    || e->u.str.segs[0].expr))) {
        lower_error(c, e, "the interpolated string literal is not "
                          "supported by this lowerer");
        return lower_val_unit();
    }
    ek_buf_init(&b);
    if (!ek_buf_append_str(&b, "ek_val_ref(ek_str_from_cstr(")) {
        c->oom = 1;
    }
    if (e->u.str.count == 1) {
        lower_string_literal(c, &b, e->u.str.segs[0].text,
                             e->u.str.segs[0].text_len);
    } else {
        lower_string_literal(c, &b, "", 0);
    }
    if (!ek_buf_append_str(&b, "))")) {
        c->oom = 1;
    }
    out.text = lower_take(c, &b);
    out.is_temp = 0;
    return out;
}

