#include <math.h>
#include <string.h>

#include "lower/internal.h"

/* Literal placement: the check row's nominal name decides the runtime
   slot a literal widens into, and a string literal becomes either one
   escaped C string on the heap or, with interpolation slots, a
   left-to-right concatenation of the escaped segments and the slots'
   derived show renderings. */

/* The runtime slot a literal widens into, decided by the check row's
   nominal name. */
enum lit_slot { LIT_INT, LIT_UINT, LIT_FLOAT, LIT_CHAR };

static enum lit_slot lit_slot(lower_ctx *c, const ek_node *e)
{
    const ek_type *t = lower_row_type(c, e);

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

/* The escaped C string of one literal segment. */
static const char *seg_cstr(lower_ctx *c, const ek_str_seg *seg)
{
    ek_buf b;

    ek_buf_init(&b);
    lower_string_literal(c, &b, seg->text, seg->text_len);
    return lower_take(c, &b);
}

/* The derived show specialization of one slot's row type; NULL when
   the slot's value has no rendering here. A slot whose row the check
   left opaque recovers the call's own result type when the open-call
   re-route handles it, since the service fixes its result. */
static const char *slot_show(lower_ctx *c, const ek_node *expr)
{
    const ek_type *t = lower_row_type(c, expr);
    const ek_type *rt = t;

    if (t && lower_type_has_var(t)) {
        const ek_type *call = lower_open_call_result(c, expr);

        if (call) {
            rt = call;
        }
    }
    if (!rt) {
        return NULL;
    }
    return lower_dsvc_request(c, expr, DSV_SHOW, rt);
}

/* acc = acc ++ part; the part and the old accumulator die in the
   join, and the accumulator name carries the result on. */
static void emit_concat(lower_ctx *c, lower_scope *scope, const char *acc,
                        const char *part)
{
    const char *n = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "{\n");
    c->depth++;
    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = ek_str_concat(%s, %s);\n", n, acc, part);
    lower_pad(c);
    lower_emitf(c, "ek_release_obj(%s);\n", part);
    lower_pad(c);
    lower_emitf(c, "ek_release_obj(%s);\n", acc);
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", acc, n);
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
}

/* The first part opens the accumulator. */
static void emit_first_literal(lower_ctx *c, const ek_str_seg *seg,
                               const char *acc)
{
    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = ek_str_from_cstr(%s);\n", acc,
                seg_cstr(c, seg));
}

static void emit_part_literal(lower_ctx *c, lower_scope *scope,
                              const ek_str_seg *seg, const char *acc)
{
    const char *s = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = ek_str_from_cstr(%s);\n", s,
                seg_cstr(c, seg));
    emit_concat(c, scope, acc, s);
}

/* One slot: the value lowers to a temp, its rendering joins the
   chain, and the value temp dies once the rendering exists. */
static void emit_part_slot(lower_ctx *c, lower_scope *scope,
                           const ek_node *expr, const char *svc,
                           const char *acc)
{
    lower_val v = lower_sub(c, scope, expr);
    const char *vt = lower_as_temp(c, scope, &v);
    const char *part = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = %s(%s);\n", part, svc, vt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", vt);
    emit_concat(c, scope, acc, part);
}

/* The unsupported-slot discard: every slot still evaluates and dies,
   and the literal produces no value. */
static lower_val discard_parts(lower_ctx *c, lower_scope *scope,
                               const ek_node *e)
{
    size_t i;

    for (i = 0; i < e->u.str.count; i++) {
        const ek_str_seg *seg = &e->u.str.segs[i];

        if (seg->expr) {
            lower_val v = lower_sub(c, scope, seg->expr);
            const char *t = lower_as_temp(c, scope, &v);

            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", t);
        }
    }
    return lower_val_unit();
}

/* The single-literal path: zero segments or one pure literal segment
   becomes one escaped C string on the heap. */
static lower_val plain_string(lower_ctx *c, const ek_node *e)
{
    ek_buf b;
    lower_val out;

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

/* One slot opening the accumulator: the value temp dies once its
   rendering exists. */
static void emit_first_slot(lower_ctx *c, lower_scope *scope,
                            const ek_node *expr, const char *svc,
                            const char *acc)
{
    lower_val v = lower_sub(c, scope, expr);
    const char *vt = lower_as_temp(c, scope, &v);

    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = %s(%s);\n", acc, svc, vt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", vt);
}

/* The interpolated path: the parts join left to right, each literal
   segment escaped as the single-segment path escapes it, each slot
   rendered through its row type's show. */
lower_val lower_string_expr(lower_ctx *c, lower_scope *scope,
                            const ek_node *e)
{
    const char *acc;
    size_t i;

    if (e->u.str.count == 0
        || (e->u.str.count == 1 && !e->u.str.segs[0].expr)) {
        return plain_string(c, e);
    }
    for (i = 0; i < e->u.str.count; i++) {
        if (e->u.str.segs[i].expr && !slot_show(c, e->u.str.segs[i].expr)) {
            lower_error(c, e, "the interpolation slot has no lowering");
            return discard_parts(c, scope, e);
        }
    }
    acc = lower_temp(c, scope);
    for (i = 0; i < e->u.str.count; i++) {
        const ek_str_seg *seg = &e->u.str.segs[i];

        if (!seg->expr) {
            if (i == 0) {
                emit_first_literal(c, seg, acc);
            } else {
                emit_part_literal(c, scope, seg, acc);
            }
        } else if (i == 0) {
            emit_first_slot(c, scope, seg->expr, slot_show(c, seg->expr),
                            acc);
        } else {
            emit_part_slot(c, scope, seg->expr, slot_show(c, seg->expr),
                           acc);
        }
    }
    {
        const char *t = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", t, acc);
        return (lower_val){ t, 1 };
    }
}
