#include <string.h>

#include "lower/internal.h"

/* The JSON side of the derived pair: to_json builds the std Json value
   of one applied type through the std constructors. Records and
   string-keyed dictionaries land as Obj, lists and tuples as Arr in
   order, Strings, Chars, and Bytes as Str (Bytes lowercase
   hexadecimal), scalars as their variants, and an algebraic type by
   external tagging. The from_json direction lowers beside it. */

/* The construction helper of a std Json variant. */
const char *lower_json_ctor(lower_ctx *c, const char *variant)
{
    return lower_std_ctor(c, NULL, "Json", variant);
}

/* The construction helper of a std Result variant. */
const char *lower_result_ctor(lower_ctx *c, const char *name)
{
    return lower_std_ctor(c, NULL, "Result", name);
}

/* return Err (Invalid "detail"); the detail bytes are interned like
   any literal. The Invalid result is a named temp released once Err
   stands, the way every nested constructor result is owned. */
void lower_json_err_lit(lower_demit *d, const char *indent,
                        const char *detail)
{
    const char *inv = lower_std_ctor(d->c, NULL, "ParseError", "Invalid");

    lower_d_printf(d, "%s{\n", indent);
    lower_d_printf(d, "%s    ek_val p = ek_keep(%s);\n", indent,
                   lower_str_cache(d->c, detail, strlen(detail)));
    lower_d_printf(d, "%s    ek_val q = %s(p);\n", indent, inv);
    lower_d_printf(d, "%s    ek_val r = %s(q);\n", indent,
                   lower_result_ctor(d->c, "Err"));
    lower_d_printf(d, "%s    ek_release(p);\n", indent);
    lower_d_printf(d, "%s    ek_release(q);\n", indent);
    lower_d_printf(d, "%s    return r;\n", indent);
    lower_d_printf(d, "%s}\n", indent);
}

/* return Err (Invalid detail); the detail expression is an owned
   ek_val whose one reference moves into the payload, and the Invalid
   result is a named temp released once Err stands. */
void lower_json_err_obj(lower_demit *d, const char *indent, const char *obj)
{
    const char *inv = lower_std_ctor(d->c, NULL, "ParseError", "Invalid");

    lower_d_printf(d, "%s{\n", indent);
    lower_d_printf(d, "%s    ek_val p = %s;\n", indent, obj);
    lower_d_printf(d, "%s    ek_val q = %s(p);\n", indent, inv);
    lower_d_printf(d, "%s    ek_val r = %s(q);\n", indent,
                   lower_result_ctor(d->c, "Err"));
    lower_d_printf(d, "%s    ek_release(p);\n", indent);
    lower_d_printf(d, "%s    ek_release(q);\n", indent);
    lower_d_printf(d, "%s    return r;\n", indent);
    lower_d_printf(d, "%s}\n", indent);
}

/* A borrowed payload slot of the std Json family. */
const char *lower_json_slot(lower_ctx *c, const char *variant, const char *v)
{
    return lower_str(c, "((ekS_std$Json$%s *)%s.as.ref)->p0", variant, v);
}

/* Wraps one owned ek_val expression into the Str variant. */
static void emit_str_wrap(lower_ctx *c, lower_demit *d, const char *expr)
{
    const char *p = lower_d_temp(d);

    lower_d_printf(d, "    ek_val %s = %s;\n", p, expr);
    lower_d_printf(d, "    ek_val r = %s(%s);\n",
                   lower_json_ctor(c, "Str"), p);
    lower_d_printf(d, "    ek_release(%s);\n", p);
    lower_d_printf(d, "    return r;\n");
}

static void emit_to_scalar(lower_ctx *c, lower_demit *d, lower_scalar sk)
{
    switch (sk) {
    case EKSC_UINT:
        lower_d_printf(d, "    return %s(ek_val_int((int64_t)v.as.u));\n",
                       lower_json_ctor(c, "Int"));
        break;
    case EKSC_FLOAT:
        lower_d_printf(d, "    return %s(v);\n", lower_json_ctor(c, "Float"));
        break;
    case EKSC_BOOL:
        lower_d_printf(d, "    return %s(v);\n", lower_json_ctor(c, "Bool"));
        break;
    case EKSC_CHAR:
        emit_str_wrap(c, d, "ek_val_ref(ek_show_char(v))");
        break;
    case EKSC_STRING:
        emit_str_wrap(c, d, "ek_keep(v)");
        break;
    case EKSC_BYTES:
        emit_str_wrap(c, d, "ek_val_ref(ek_bytes_to_hex(v.as.ref))");
        break;
    default:
        lower_d_printf(d, "    return %s(v);\n", lower_json_ctor(c, "Int"));
        break;
    }
}

/* The recursive cell walk behind an Arr, as its own service: each
   element's Json value dies once its cons stands. */
static void emit_to_list_aux(lower_ctx *c, const char *aux, const ek_type *t)
{
    const char *elem = lower_dsvc_request(c, NULL, DSV_TOJSON,
                                          t->u.list.elem);
    lower_demit d;
    ek_buf b;

    lower_d_begin(c, &d, &b);
    lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val cell)", aux));
    lower_d_printf(&d, "static ek_val %s(ek_val cell)\n{\n", aux);
    if (!elem) {
        lower_d_printf(&d, "    return ek_nil_val();\n");
    } else {
        lower_d_printf(&d, "    ek_val h, t, out;\n\n");
        lower_d_printf(&d, "    if (!ek_val_is_cons(cell))\n");
        lower_d_printf(&d, "        return ek_nil_val();\n");
        lower_d_printf(&d, "    h = %s(ek_cons_head(cell.as.ref));\n", elem);
        lower_d_printf(&d, "    t = %s(ek_cons_tail(cell.as.ref));\n", aux);
        lower_d_printf(&d, "    out = ek_cons(h, t);\n");
        lower_d_printf(&d, "    ek_release(h);\n");
        lower_d_printf(&d, "    ek_release(t);\n");
        lower_d_printf(&d, "    return out;\n");
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}

static void emit_to_arr(lower_ctx *c, lower_demit *d, const char *name,
                        const ek_type *t)
{
    const char *aux = lower_str(c, "%s_aux", name);

    emit_to_list_aux(c, aux, t);
    lower_d_printf(d, "    return %s(%s(v));\n", lower_json_ctor(c, "Arr"),
                   aux);
}

/* One Obj entry: the key text and the mapped value, stored over the
   running dictionary named by dict. */
static void emit_obj_entry(lower_ctx *c, lower_demit *d, const char *dict,
                           const char *key_expr, const char *val_svc,
                           const char *val_expr)
{
    const char *k = lower_d_temp(d);
    (void)c;
    const char *p = lower_d_temp(d);
    const char *e = lower_d_temp(d);

    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        ek_val %s = %s;\n", k, key_expr);
    lower_d_printf(d, "        ek_val %s = %s(%s);\n", p, val_svc, val_expr);
    lower_d_printf(d, "        ek_val %s = ek_dict_set(%s, %s, %s);\n", e,
                   dict, k, p);
    lower_d_printf(d, "        ek_release(%s);\n", p);
    lower_d_printf(d, "        ek_release(%s);\n", k);
    lower_d_printf(d, "        ek_release(%s);\n", dict);
    lower_d_printf(d, "        %s = %s;\n", dict, e);
    lower_d_printf(d, "    }\n");
}

static void emit_to_record(lower_ctx *c, lower_demit *d, const char *stem,
                           const char **fnames, const ek_type **ftypes,
                           size_t nf)
{
    size_t i;

    lower_d_printf(d, "    ek_val d = ek_dict_new();\n");
    lower_d_printf(d, "    ek_val out;\n\n");
    for (i = 0; i < nf; i++) {
        const char *slot = fnames ? lower_str(c, "f_%s", fnames[i])
                                  : lower_str(c, "f_%zu", i);
        const char *svc = lower_dsvc_request(c, NULL, DSV_TOJSON, ftypes[i]);
        const char *lit = lower_str_cache(c, fnames ? fnames[i] : "p",
                                          fnames ? strlen(fnames[i]) : 1);

        if (!svc) {
            lower_d_printf(d, "    (void)d;\n");
            lower_json_err_lit(d, "    ", ""); /* check gated the shape */
            return;
        }
        emit_obj_entry(c, d, "d", lower_str(c, "ek_keep(%s)", lit), svc,
                       lower_str(c, "((ekS_%s *)v.as.ref)->%s", stem, slot));
    }
    lower_d_printf(d, "    out = %s(d);\n", lower_json_ctor(c, "Obj"));
    lower_d_printf(d, "    ek_release(d);\n");
    lower_d_printf(d, "    return out;\n");
}

static void emit_to_dict(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *svc = lower_dsvc_request(c, NULL, DSV_TOJSON,
                                         t->u.dict.val);

    if (!svc) {
        lower_json_err_lit(d, "    ", "");
        return;
    }
    lower_d_printf(d, "    ek_val d = ek_dict_new();\n");
    lower_d_printf(d, "    ek_val k, x;\n");
    lower_d_printf(d, "    size_t cur = 0;\n");
    lower_d_printf(d, "    ek_val out;\n\n");
    lower_d_printf(d, "    while (ek_dict_next(v, &cur, &k, &x)) {\n");
    lower_d_printf(d, "        ek_val e = ek_dict_set(d, k, %s(x));\n", svc);
    lower_d_printf(d, "        ek_release(d);\n");
    lower_d_printf(d, "        d = e;\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    out = %s(d);\n", lower_json_ctor(c, "Obj"));
    lower_d_printf(d, "    ek_release(d);\n");
    lower_d_printf(d, "    return out;\n");
}

/* A tuple maps component-wise into one Arr. */
static void emit_to_tuple(lower_ctx *c, lower_demit *d, const char *stem,
                          const ek_type **ftypes, size_t nf)
{
    const char **parts;
    size_t cap = 0, i;

    parts = lower_grow(c, NULL, &cap, nf ? nf : 1, sizeof *parts);
    if (!parts) {
        return;
    }
    lower_use_list(c);
    for (i = 0; i < nf; i++) {
        const char *svc = lower_dsvc_request(c, NULL, DSV_TOJSON, ftypes[i]);
        const char *a = lower_d_temp(d);

        parts[i] = a;
        lower_d_printf(d, "    ek_val %s = %s(((ekS_%s *)v.as.ref)->f_%zu);"
                          "\n", a, svc ? svc : "0", stem, i);
        if (!svc) {
            size_t k;

            for (k = 0; k <= i; k++) {
                lower_d_printf(d, "    (void)%s;\n", parts[k]);
            }
            lower_json_err_lit(d, "    ", ""); /* check gated the shape */
            return;
        }
    }
    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        ek_val arr[] = {");
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "%s%s", i ? ", " : "", parts[i]);
    }
    lower_d_printf(d, "};\n");
    lower_d_printf(d, "        ek_val l = ekn_list(arr, %zu);\n", nf);
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "        ek_release(%s);\n", parts[i]);
    }
    lower_d_printf(d, "        ek_val r = %s(l);\n",
                   lower_json_ctor(c, "Arr"));
    lower_d_printf(d, "        ek_release(l);\n");
    lower_d_printf(d, "        return r;\n");
    lower_d_printf(d, "    }\n");
}

/* External tagging: a zero-payload variant is the bare string, a
   payload variant the single-key object. */
static void emit_to_adt(lower_ctx *c, lower_demit *d, lower_variant *vs,
                        size_t nv)
{
    size_t i;

    for (i = 0; i < nv; i++) {
        const char *lit = lower_str_cache(c, vs[i].name, strlen(vs[i].name));

        lower_d_printf(d, "    if (v.as.ref->info == &ekT_%s) {\n",
                       vs[i].stem);
        if (!vs[i].payload) {
            lower_d_printf(d, "        return %s(ek_keep(%s));\n",
                           lower_json_ctor(c, "Str"), lit);
        } else {
            const char *svc = lower_dsvc_request(c, NULL, DSV_TOJSON,
                                                 vs[i].payload);

            if (!svc) {
                lower_d_printf(d, "        (void)v;\n");
                lower_json_err_lit(d, "    ", ""); /* check gated the shape */
            } else {
                const char *dd = lower_d_temp(d);
                const char *k = lower_d_temp(d);
                const char *p = lower_d_temp(d);
                const char *e = lower_d_temp(d);
                const char *r = lower_d_temp(d);

                lower_d_printf(d, "        ek_val %s = ek_dict_new();\n", dd);
                lower_d_printf(d, "        ek_val %s = ek_keep(%s);\n", k,
                               lit);
                lower_d_printf(d, "        ek_val %s = %s(((ekS_%s *)v.as."
                                  "ref)->p0);\n", p, svc, vs[i].stem);
                lower_d_printf(d, "        ek_val %s = ek_dict_set(%s, %s, "
                                  "%s);\n", e, dd, k, p);
                lower_d_printf(d, "        ek_release(%s);\n", p);
                lower_d_printf(d, "        ek_release(%s);\n", k);
                lower_d_printf(d, "        ek_release(%s);\n", dd);
                lower_d_printf(d, "        %s = %s;\n", dd, e);
                lower_d_printf(d, "        ek_val %s = %s(%s);\n", r,
                               lower_json_ctor(c, "Obj"), dd);
                lower_d_printf(d, "        ek_release(%s);\n", dd);
                lower_d_printf(d, "        return %s;\n", r);
            }
        }
        lower_d_printf(d, "    }\n");
    }
    lower_d_printf(d, "    return %s();\n", lower_json_ctor(c, "Null"));
}

void lower_dsvc_json_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int from)
{
    lower_demit d;
    ek_buf b;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    lower_variant *vs;
    size_t nf, nv;

    if (from) {
        lower_dsvc_from_emit(c, name, t);
        return;
    }
    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static ek_val %s(ek_val v)\n{\n", name);
    if (lower_scalar_kind(t) != EKSC_OTHER) {
        emit_to_scalar(c, &d, lower_scalar_kind(t));
    } else if (t->kind == EK_T_UNIT) {
        lower_d_printf(&d, "    return %s();\n", lower_json_ctor(c, "Null"));
    } else if (t->kind == EK_T_LIST) {
        emit_to_arr(c, &d, name, t);
    } else if (t->kind == EK_T_DICT) {
        emit_to_dict(c, &d, t);
    } else if (t->kind == EK_T_TUPLE) {
        if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
            emit_to_tuple(c, &d, g->stem, ftypes, nf);
        } else {
            lower_json_err_lit(&d, "    ", "");
        }
    } else if (t->kind == EK_T_RECORD) {
        if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
            emit_to_record(c, &d, g->stem, fnames, ftypes, nf);
        } else {
            lower_json_err_lit(&d, "    ", "");
        }
    } else if (t->kind == EK_T_NOMINAL
               && lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g)
               && g) {
        if (g->shape == GT_TUP) {
            emit_to_tuple(c, &d, g->stem, ftypes, nf);
        } else {
            emit_to_record(c, &d, g->stem, fnames, ftypes, nf);
        }
    } else if (t->kind == EK_T_NOMINAL
               && lower_variants_of(c, NULL, t, &vs, &nv, 0)) {
        emit_to_adt(c, &d, vs, nv);
    } else {
        lower_json_err_lit(&d, "    ", "");
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}
