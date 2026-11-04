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

/* The owned object spelling of an interned literal: the keep the body
   releases balances the cache's own immortal reference. */
static const char *lit_obj(lower_ctx *c, const char *s)
{
    return lower_str(c, "ek_keep(%s).as.ref",
                     lower_str_cache(c, s, strlen(s)));
}

static void emit_bytes_show(lower_demit *d)
{
    lower_d_printf(d, "    ek_obj *acc = %s;\n",
                   lit_obj(d->c, "Bytes.from_hex(\""));
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
        /* The character's display channel is bare; the structural
           channel quotes it as the literal form. */
        if (inspect) {
            lower_d_printf(d, "    return ek_str_quote(ek_show_char(v), "
                              "'\\'');\n");
        } else {
            lower_d_printf(d, "    return ek_show_char(v);\n");
        }
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

/* The literal form a component renders when it carries no service
   body of its own: a function shows its own shape rather than an
   opaque placeholder. NULL leaves the defensive arm. */
static const char *bare_form(const ek_type *t)
{
    if (t && t->kind == EK_T_FN) {
        return "<fn>";
    }
    return NULL;
}

/* Emits one rendered part into a declared temp: through the
   component's service, or as the component's own literal form when it
   carries none. */
static void emit_part(lower_demit *d, const char *ind, const char *part,
                      const char *val, const char *svc, const char *form)
{
    if (svc) {
        lower_d_printf(d, "%s%s = %s(%s);\n", ind, part, svc, val);
    } else {
        lower_d_printf(d, "%s%s = %s;\n", ind, part, lit_obj(d->c, form));
    }
}

static void emit_list_show(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *elem = insp_of(c, t->u.list.elem);
    const char *form = bare_form(t->u.list.elem);
    const char *ind = "        ";

    if (!elem && !form) {
        lower_d_printf(d, "    (void)v;\n");
        lower_d_printf(d, "    return %s;\n", lit_obj(c, "<opaque>"));
        return;
    }
    lower_d_printf(d, "    ek_obj *acc = %s;\n", lit_obj(c, "{"));
    lower_d_printf(d, "    ek_val cell = v;\n");
    lower_d_printf(d, "    int first = 1;\n\n");
    lower_d_printf(d, "    while (ek_val_is_cons(cell)) {\n");
    lower_d_printf(d, "        ek_obj *part;\n\n");
    lower_d_printf(d, "%sif (!first)\n", ind);
    lower_d_concat_lit(d, ind, "acc", ", ");
    emit_part(d, ind, "part", "ek_cons_head(cell.as.ref)", elem, form);
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
    const char *form = bare_form(t->u.dict.val);
    const char *ind = "        ";

    if (!ks || (!vs && !form)) {
        lower_d_printf(d, "    (void)v;\n");
        lower_d_printf(d, "    return %s;\n", lit_obj(c, "<opaque>"));
        return;
    }
    lower_d_printf(d, "    ek_obj *acc = %s;\n", lit_obj(c, "{"));
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
    emit_part(d, ind, "pv", "x", vs, form);
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

    if (nf == 0) {
        lower_d_printf(d, "    (void)v;\n");
    }
    lower_d_printf(d, "    ek_obj *acc = %s;\n", lit_obj(c, "{"));
    for (i = 0; i < nf; i++) {
        const char *slot = fnames ? lower_str(c, "f_%s", fnames[i])
                                  : lower_str(c, "f_%zu", i);
        const char *f = insp_of(c, ftypes[i]);
        const char *form = bare_form(ftypes[i]);
        const char *part;

        if (i) {
            lower_d_concat_lit(d, "    ", "acc", ", ");
        }
        if (!f && !form) {
            lower_d_concat_lit(d, "    ", "acc", "<opaque>");
            continue;
        }
        if (fnames) {
            lower_d_concat_lit(d, "    ", "acc", fnames[i]);
            lower_d_concat_lit(d, "    ", "acc", ": ");
        }
        part = lower_d_temp(d);
        lower_d_printf(d, "    ek_obj *%s;\n", part);
        emit_part(d, "    ", part,
                  lower_str(c, "((ekS_%s *)v.as.ref)->%s", stem, slot),
                  f, form);
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
            lower_d_printf(d, "        return %s;\n",
                           lit_obj(c, vs[i].name));
        } else {
            const char *f = insp_of(c, vs[i].payload);
            const char *form = bare_form(vs[i].payload);
            const char *part = lower_d_temp(d);

            lower_d_printf(d, "        ek_obj *%s;\n", part);
            if (!f && !form) {
                lower_d_printf(d, "        return %s;\n",
                               lit_obj(c, "<opaque>"));
            } else {
                lower_d_printf(d, "        ek_obj *acc = %s;\n",
                               lit_obj(c, vs[i].name));
                emit_part(d, "        ", part,
                          lower_str(c, "((ekS_%s *)v.as.ref)->p0",
                                    vs[i].stem), f, form);
                lower_d_concat_lit(d, "        ", "acc", " ");
                lower_d_concat_obj(d, "        ", "acc", part);
                lower_d_printf(d, "        return acc;\n");
            }
        }
        lower_d_printf(d, "    }\n");
    }
    lower_d_printf(d, "    return %s;\n", lit_obj(c, "<opaque>"));
}

/* The operator form of a range value: the two endpoints in their own
   literal forms joined by the closed flag's operator, so neither
   channel prints a constructor name. */
static void emit_range_show(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    lower_gtype *g = elem ? lower_range_family(c, lower_scalar_kind(elem))
                          : NULL;
    const char *f = elem ? insp_of(c, elem) : NULL;
    const char *part;

    if (!g || !f) {
        lower_d_printf(d, "    (void)v;\n");
        lower_d_printf(d, "    return %s;\n", lit_obj(c, "<range>"));
        return;
    }
    lower_d_printf(d, "    ek_obj *acc = %s(((ekS_%s *)v.as.ref)"
                      "->f_start);\n", f, g->stem);
    lower_d_printf(d, "    if (((ekS_%s *)v.as.ref)->f_closed.as.i) {\n",
                   g->stem);
    lower_d_concat_lit(d, "        ", "acc", "..=");
    lower_d_printf(d, "    } else {\n");
    lower_d_concat_lit(d, "        ", "acc", "..");
    lower_d_printf(d, "    }\n");
    part = lower_d_temp(d);
    lower_d_printf(d, "    ek_obj *%s = %s(((ekS_%s *)v.as.ref)"
                      "->f_end);\n", part, f, g->stem);
    lower_d_concat_obj(d, "    ", "acc", part);
    lower_d_printf(d, "    return acc;\n");
}

/* The struct side of the dispatch: the composite and nominal shapes. */
static void emit_struct_show(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    lower_variant *vs;
    size_t nf, nv;

    if (lower_is_handle(t)) {
        lower_d_printf(d, "    (void)v;\n");
        lower_d_printf(d, "    return %s;\n", lit_obj(c, "<handle>"));
    } else if (t->kind == EK_T_IVAR) {
        /* the bridge: an opaque crossing whose shape the walk could not
           name -- the pointer family, whose address never prints */
        lower_d_printf(d, "    (void)v;\n");
        lower_d_printf(d, "    return %s;\n", lit_obj(c, "<opaque>"));
    } else if (t->kind == EK_T_LIST) {
        emit_list_show(c, d, t);
    } else if (t->kind == EK_T_DICT) {
        emit_dict_show(c, d, t);
    } else if (lower_is_range(t)) {
        emit_range_show(c, d, t);
    } else if (t->kind == EK_T_TUPLE || t->kind == EK_T_RECORD) {
        if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
            /* The struct family spells a record field by its name and
               a tuple field by its position; the walk reads the same
               slots the typedef declares, and a tuple renders its
               components bare. */
            emit_fields_show(c, d, g->stem,
                             t->kind == EK_T_RECORD ? fnames : NULL, ftypes,
                             nf);
        } else {
            lower_d_printf(d, "    (void)v;\n");
            lower_d_printf(d, "    return %s;\n", lit_obj(c, "<opaque>"));
        }
    } else if (t->kind == EK_T_NOMINAL
               && lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g)
               && g) {
        emit_fields_show(c, d, g->stem, fnames, ftypes, nf);
    } else if (t->kind == EK_T_NOMINAL
               && lower_variants_of(c, NULL, t, &vs, &nv, 0)) {
        emit_adt_show(c, d, vs, nv);
    } else {
        lower_d_printf(d, "    (void)v;\n");
        lower_d_printf(d, "    return %s;\n", lit_obj(c, "<cap>"));
    }
}

void lower_dsvc_show_emit(lower_ctx *c, const char *name, const ek_type *t,
                          int inspect)
{
    lower_demit d;
    ek_buf b;
    lower_scalar sk = lower_scalar_kind(t);
    const ek_symbol *user = NULL;

    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static ek_obj *%s(ek_val v)\n{\n", name);
    /* The display channel loses to a declared member: a namespace
       `show` over the receiver's type renders through it, the same
       reading the member call gives, and inspect stays structural. */
    if (!inspect && t && t->kind == EK_T_NOMINAL) {
        user = lower_member_symbol(c, t, "show");
    }
    if (user) {
        lower_enqueue(c, user);
        lower_d_printf(&d, "    return %s(v).as.ref;\n",
                       lower_mangled(c, user));
    } else if (sk != EKSC_OTHER) {
        emit_scalar_show(&d, sk, inspect);
    } else if (!inspect) {
        const char *is = insp_of(c, t);

        if (is) {
            lower_d_printf(&d, "    return %s(v);\n", is);
        } else {
            lower_d_printf(&d, "    (void)v;\n");
            lower_d_printf(&d, "    return %s;\n", lit_obj(c, "<fn>"));
        }
    } else if (t->kind == EK_T_UNIT) {
        lower_d_printf(&d, "    (void)v;\n");
        lower_d_printf(&d, "    return %s;\n", lit_obj(c, "_"));
    } else if (t->kind == EK_T_FN) {
        lower_d_printf(&d, "    (void)v;\n");
        lower_d_printf(&d, "    return %s;\n", lit_obj(c, "<fn>"));
    } else {
        emit_struct_show(c, &d, t);
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}
