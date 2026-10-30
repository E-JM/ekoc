#include <string.h>

#include "lower/internal.h"

/* The display and structural channels over an applied type. The two
   channels agree everywhere except a top-level String: show renders it
   bare, inspect quotes and escapes it, so a String nested inside a
   composite keeps its quotes on both channels. Composite bodies are
   the inspect form; the show entry forwards to it. Scalars render
   through the runtime's display family. */

static const char *insp_of(lower_ctx *c, const ek_type *t)
{
    return lower_dsvc_request(c, NULL, DSV_INSPECT, t);
}

static const char *show_of(lower_ctx *c, const ek_type *t)
{
    return lower_dsvc_request(c, NULL, DSV_SHOW, t);
}

static void emit_bytes_show(lower_demit *d)
{
    lower_d_printf(d, "    ek_obj *acc = ek_str_from_cstr(%s);\n",
                   lower_d_cstr(d->c, "Bytes.from_hex(\""));
    lower_d_printf(d, "    ek_obj *part = ek_bytes_to_hex(v.as.ref);\n\n");
    lower_d_concat_obj(d, "    ", "acc", "part");
    lower_d_concat_lit(d, "    ", "acc", "\")");
    lower_d_printf(d, "    return acc;\n");
}

static void emit_scalar_show(lower_demit *d, lower_scalar sk, int inspect)
{
    if (sk == EKSC_STRING) {
        if (inspect) {
            lower_d_printf(d, "    return ek_str_quote(v.as.ref, '\"');\n");
        } else {
            lower_d_printf(d, "    ek_retain(v);\n");
            lower_d_printf(d, "    return v.as.ref;\n");
        }
        return;
    }
    if (sk == EKSC_BYTES) {
        emit_bytes_show(d);
        return;
    }
    if (sk == EKSC_CHAR) {
        lower_d_printf(d, "    return ek_str_quote(ek_show_char(v), "
                          "'\\'');\n");
        return;
    }
    if (sk == EKSC_INT) {
        lower_d_printf(d, "    return ek_show_int(v);\n");
    } else if (sk == EKSC_UINT) {
        lower_d_printf(d, "    return ek_show_uint(v);\n");
    } else if (sk == EKSC_FLOAT) {
        lower_d_printf(d, "    return ek_show_float(v);\n");
    } else {
        lower_d_printf(d, "    return ek_show_bool(v);\n");
    }
}

/* acc = acc ++ quote(obj); the object dies with the join. */
static void emit_quoted(lower_demit *d, const char *indent, const char *acc,
                        const char *obj)
{
    const char *q = lower_d_temp(d);

    lower_d_printf(d, "%sek_obj *%s = ek_str_quote(%s, '\"');\n", indent, q,
                   obj);
    lower_d_printf(d, "%sek_release_obj(%s);\n", indent, obj);
    lower_d_concat_obj(d, indent, acc, q);
}

static void emit_list_show(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *elem = insp_of(c, t->u.list.elem);
    const char *ind = "        ";

    if (!elem) {
        lower_d_printf(d, "    return ek_str_from_cstr(\"<opaque>\");\n");
        return;
    }
    lower_d_printf(d, "    ek_obj *acc = ek_str_from_cstr(%s);\n",
                   lower_d_cstr(d->c, "{"));
    lower_d_printf(d, "    ek_val cell = v;\n");
    lower_d_printf(d, "    int first = 1;\n\n");
    lower_d_printf(d, "    while (ek_val_is_cons(cell)) {\n");
    lower_d_printf(d, "        ek_obj *part;\n\n");
    lower_d_printf(d, "%sif (!first)\n", ind);
    lower_d_concat_lit(d, ind, "acc", ", ");
    lower_d_printf(d, "%spart = %s(ek_cons_head(cell.as.ref));\n", ind, elem);
    lower_d_printf(d, "%sfirst = 0;\n", ind);
    lower_d_concat_obj(d, ind, "acc", "part");
    lower_d_printf(d, "%scell = ek_cons_tail(cell.as.ref);\n", ind);
    lower_d_printf(d, "    }\n");
    lower_d_concat_lit(d, "    ", "acc", "}");
    lower_d_printf(d, "    return acc;\n");
}

static void emit_dict_show(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *ks = show_of(c, t->u.dict.key);
    const char *vs = insp_of(c, t->u.dict.val);
    const char *ind = "        ";

    if (!ks || !vs) {
        lower_d_printf(d, "    return ek_str_from_cstr(\"<opaque>\");\n");
        return;
    }
    lower_d_printf(d, "    ek_obj *acc = ek_str_from_cstr(%s);\n",
                   lower_d_cstr(d->c, "{"));
    lower_d_printf(d, "    ek_val k, x;\n");
    lower_d_printf(d, "    size_t cur = 0;\n");
    lower_d_printf(d, "    int first = 1;\n\n");
    lower_d_printf(d, "    while (ek_dict_next(v, &cur, &k, &x)) {\n");
    lower_d_printf(d, "        ek_obj *kt;\n");
    lower_d_printf(d, "        ek_obj *pv;\n\n");
    lower_d_printf(d, "%sif (!first)\n", ind);
    lower_d_concat_lit(d, ind, "acc", ", ");
    lower_d_printf(d, "%skt = %s(k);\n", ind, ks);
    emit_quoted(d, ind, "acc", "kt");
    lower_d_concat_lit(d, ind, "acc", ": ");
    lower_d_printf(d, "%spv = %s(x);\n", ind, vs);
    lower_d_concat_obj(d, ind, "acc", "pv");
    lower_d_printf(d, "%sfirst = 0;\n", ind);
    lower_d_printf(d, "    }\n");
    lower_d_concat_lit(d, "    ", "acc", "}");
    lower_d_printf(d, "    return acc;\n");
}

/* Field-wise rendering of a record or tuple layout. */
static void emit_fields_show(lower_ctx *c, lower_demit *d, const char *stem,
                             const char **fnames, const ek_type **ftypes,
                             size_t nf)
{
    size_t i;

    lower_d_printf(d, "    ek_obj *acc = ek_str_from_cstr(%s);\n",
                   lower_d_cstr(d->c, "{"));
    for (i = 0; i < nf; i++) {
        const char *slot = fnames ? lower_str(c, "f_%s", fnames[i])
                                  : lower_str(c, "f_%zu", i);
        const char *f = insp_of(c, ftypes[i]);
        const char *part;

        if (i) {
            lower_d_concat_lit(d, "    ", "acc", ", ");
        }
        if (!f) {
            lower_d_concat_lit(d, "    ", "acc", "<opaque>");
            continue;
        }
        if (fnames) {
            lower_d_concat_lit(d, "    ", "acc", fnames[i]);
            lower_d_concat_lit(d, "    ", "acc", ": ");
        }
        part = lower_d_temp(d);
        lower_d_printf(d, "    ek_obj *%s = %s(((ekS_%s *)v.as.ref)->%s);\n",
                       part, f, stem, slot);
        lower_d_concat_obj(d, "    ", "acc", part);
    }
    lower_d_concat_lit(d, "    ", "acc", "}");
    lower_d_printf(d, "    return acc;\n");
}

/* The constructor-application form of an algebraic value. */
static void emit_adt_show(lower_ctx *c, lower_demit *d, lower_variant *vs,
                          size_t nv)
{
    size_t i;

    for (i = 0; i < nv; i++) {
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_%s) {\n",
                       vs[i].stem);
        if (!vs[i].payload) {
            lower_d_printf(d, "        return ek_str_from_cstr(%s);\n",
                           lower_d_cstr(d->c, vs[i].name));
        } else {
            const char *f = insp_of(c, vs[i].payload);
            const char *part = lower_d_temp(d);

            if (!f) {
                lower_d_printf(d, "        return ek_str_from_cstr("
                                  "\"<opaque>\");\n");
            } else {
                lower_d_printf(d, "        ek_obj *acc = ek_str_from_cstr("
                                  "%s);\n", lower_d_cstr(d->c, vs[i].name));
                lower_d_printf(d, "        ek_obj *%s = %s(((ekS_%s *)v.as."
                                  "ref)->p0);\n", part, f, vs[i].stem);
                lower_d_concat_lit(d, "        ", "acc", " ");
                lower_d_concat_obj(d, "        ", "acc", part);
                lower_d_printf(d, "        return acc;\n");
            }
        }
        lower_d_printf(d, "    }\n");
    }
    lower_d_printf(d, "    return ek_str_from_cstr(\"<opaque>\");\n");
}

void lower_dsvc_show_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int inspect)
{
    lower_demit d;
    ek_buf b;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    lower_variant *vs;
    size_t nf, nv;
    lower_scalar sk = lower_scalar_kind(t);

    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static ek_obj *%s(ek_val v)\n{\n", name);
    if (sk != EKSC_OTHER) {
        emit_scalar_show(&d, sk, inspect);
    } else if (!inspect) {
        const char *is = insp_of(c, t);

        if (is) {
            lower_d_printf(&d, "    return %s(v);\n", is);
        } else {
            lower_d_printf(&d, "    return ek_str_from_cstr(\"<cap>\");\n");
        }
    } else if (t->kind == EK_T_UNIT) {
        lower_d_printf(&d, "    return ek_str_from_cstr(\"_\");\n");
    } else if (t->kind == EK_T_FN) {
        lower_d_printf(&d, "    return ek_str_from_cstr(\"<fn>\");\n");
    } else if (t->kind == EK_T_LIST) {
        emit_list_show(c, &d, t);
    } else if (t->kind == EK_T_DICT) {
        emit_dict_show(c, &d, t);
    } else if (t->kind == EK_T_TUPLE || t->kind == EK_T_RECORD) {
        if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
            emit_fields_show(c, &d, g->stem, fnames, ftypes, nf);
        } else {
            lower_d_printf(&d, "    return ek_str_from_cstr("
                               "\"<opaque>\");\n");
        }
    } else if (t->kind == EK_T_NOMINAL
               && lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g)
               && g) {
        emit_fields_show(c, &d, g->stem, fnames, ftypes, nf);
    } else if (t->kind == EK_T_NOMINAL
               && lower_variants_of(c, NULL, t, &vs, &nv)) {
        emit_adt_show(c, &d, vs, nv);
    } else {
        lower_d_printf(&d, "    return ek_str_from_cstr(\"<cap>\");\n");
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}
