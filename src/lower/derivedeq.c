#include <string.h>

#include "lower/internal.h"

/* The structural equality service over an applied type. Scalars
   compare by payload, Strings and Bytes by their bytes, containers
   element by element, records and tuples field by field in layout
   order with short-circuit, and algebraic values by constructor
   identity then payload. */

static const char *eq_of(lower_ctx *c, const ek_type *t)
{
    return lower_dsvc_request(c, NULL, DSV_EQ, t);
}

static void emit_scalar_eq(lower_ctx *c, lower_demit *d, lower_scalar sk)
{
    switch (sk) {
    case EKSC_UINT:
        lower_d_printf(d, "    return a.as.u == b.as.u;\n");
        break;
    case EKSC_FLOAT:
        lower_d_printf(d, "    return a.as.f == b.as.f;\n");
        break;
    case EKSC_CHAR:
        lower_d_printf(d, "    return a.as.c == b.as.c;\n");
        break;
    case EKSC_STRING:
        lower_d_printf(d, "    return ek_str_eq(a.as.ref, b.as.ref);\n");
        break;
    case EKSC_BYTES:
        c->uses_string_h = 1;
        lower_d_printf(d, "    if (ek_bytes_len(a.as.ref) != "
                          "ek_bytes_len(b.as.ref))\n");
        lower_d_printf(d, "        return 0;\n");
        lower_d_printf(d, "    return memcmp(ek_bytes_data(a.as.ref), "
                          "ek_bytes_data(b.as.ref),\n");
        lower_d_printf(d, "               ek_bytes_len(a.as.ref)) == 0;\n");
        break;
    default:
        lower_d_printf(d, "    return a.as.i == b.as.i;\n");
        break;
    }
}

static void emit_list_eq(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *elem = eq_of(c, t->u.list.elem);

    if (!elem) {
        lower_d_printf(d, "    return 0;\n");
        return;
    }
    lower_d_printf(d, "    while (ek_val_is_cons(a) && "
                      "ek_val_is_cons(b)) {\n");
    lower_d_printf(d, "        if (!%s(ek_cons_head(a.as.ref), "
                      "ek_cons_head(b.as.ref)))\n", elem);
    lower_d_printf(d, "            return 0;\n");
    lower_d_printf(d, "        a = ek_cons_tail(a.as.ref);\n");
    lower_d_printf(d, "        b = ek_cons_tail(b.as.ref);\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    return ek_val_is_nil(a) && ek_val_is_nil(b);\n");
}

static void emit_dict_eq(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *val = eq_of(c, t->u.dict.val);

    if (!val) {
        lower_d_printf(d, "    return 0;\n");
        return;
    }
    lower_d_printf(d, "    ek_val k, x, y;\n");
    lower_d_printf(d, "    ek_dict_iter it;\n");
    lower_d_printf(d, "    int ok = 1;\n\n");
    lower_d_printf(d, "    if (ek_dict_len(a) != ek_dict_len(b))\n");
    lower_d_printf(d, "        return 0;\n");
    lower_d_printf(d, "    ek_dict_iter_begin(&it, a);\n");
    lower_d_printf(d, "    while (ok && ek_dict_iter_next(&it, &k, &x)) {\n");
    lower_d_printf(d, "        if (!ek_dict_get(b, k, &y))\n");
    lower_d_printf(d, "            ok = 0;\n");
    lower_d_printf(d, "        else if (!%s(x, y))\n", val);
    lower_d_printf(d, "            ok = 0;\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    ek_dict_iter_end(&it);\n");
    lower_d_printf(d, "    return ok;\n");
}

/* Field-wise equality over a record or tuple layout. */
static void emit_fields_eq(lower_ctx *c, lower_demit *d, const char *stem,
                           const char **fnames, const ek_type **ftypes,
                           size_t nf)
{
    size_t i;

    for (i = 0; i < nf; i++) {
        const char *slot = fnames ? lower_str(c, "f_%s", fnames[i])
                                  : lower_str(c, "f_%zu", i);
        const char *f = eq_of(c, ftypes[i]);

        if (!f) {
            lower_d_printf(d, "    return 0;\n");
            return;
        }
        lower_d_printf(d, "    if (!%s(((ekS_%s *)a.as.ref)->%s, "
                          "((ekS_%s *)b.as.ref)->%s))\n", f, stem, slot,
                       stem, slot);
        lower_d_printf(d, "        return 0;\n");
    }
    lower_d_printf(d, "    return 1;\n");
}

static void emit_adt_eq(lower_ctx *c, lower_demit *d, lower_variant *vs,
                        size_t nv)
{
    size_t i;

    lower_d_printf(d, "    if (a.as.ref->info != b.as.ref->info)\n");
    lower_d_printf(d, "        return 0;\n");
    for (i = 0; i < nv; i++) {
        const char *f = vs[i].payload ? eq_of(c, vs[i].payload) : NULL;

        if (!vs[i].payload) {
            continue;
        }
        if (!f) {
            lower_d_printf(d, "    if (a.as.ref->info == &ekT_%s)\n",
                           vs[i].stem);
            lower_d_printf(d, "        return 0;\n");
            continue;
        }
        lower_d_printf(d, "    if (a.as.ref->info == &ekT_%s)\n", vs[i].stem);
        lower_d_printf(d, "        return %s(((ekS_%s *)a.as.ref)->p0, "
                          "((ekS_%s *)b.as.ref)->p0);\n", f, vs[i].stem,
                       vs[i].stem);
    }
    lower_d_printf(d, "    return 1;\n");
}

/* Set equality is set equality: the lengths agree and every element of
   one has its counterpart in the other, whatever order each was built
   in. The element comparison is the derived service, so a set of
   structural values compares structurally. */
static void emit_set_eq(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    const char *eq = elem ? eq_of(c, elem) : NULL;

    if (!eq) {
        lower_d_printf(d, "    return 0;\n");
        return;
    }
    lower_d_printf(d, "    ek_val la = ek_set_to_list(a);\n");
    lower_d_printf(d, "    ek_val lb = ek_set_to_list(b);\n");
    lower_d_printf(d, "    int same = ek_list_len(la) == ek_list_len(lb);\n");
    lower_d_printf(d, "    ek_val p = la;\n\n");
    lower_d_printf(d, "    while (same && ek_val_is_cons(p)) {\n");
    lower_d_printf(d, "        ek_val x = ek_cons_head(p.as.ref);\n");
    lower_d_printf(d, "        ek_val q = lb;\n");
    lower_d_printf(d, "        int found = 0;\n\n");
    lower_d_printf(d, "        while (ek_val_is_cons(q)) {\n");
    lower_d_printf(d, "            if (%s(x, ek_cons_head(q.as.ref))) {\n",
                   eq);
    lower_d_printf(d, "                found = 1;\n");
    lower_d_printf(d, "                break;\n");
    lower_d_printf(d, "            }\n");
    lower_d_printf(d, "            q = ek_cons_tail(q.as.ref);\n");
    lower_d_printf(d, "        }\n");
    lower_d_printf(d, "        same = found;\n");
    lower_d_printf(d, "        p = ek_cons_tail(p.as.ref);\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    ek_release(la);\n");
    lower_d_printf(d, "    ek_release(lb);\n");
    lower_d_printf(d, "    return same;\n");
}

/* An undetermined payload compares by its tag and payload bytes. */
static void emit_opaque_eq(lower_demit *d)
{
    lower_d_printf(d, "    if (a.tag != b.tag)\n");
    lower_d_printf(d, "        return 0;\n");
    lower_d_printf(d, "    if (a.tag == EK_VAL_REF)\n");
    lower_d_printf(d, "        return a.as.ref == b.as.ref;\n");
    lower_d_printf(d, "    return a.as.u == b.as.u;\n");
}

/* The range's equality: the two endpoints by their payload, the
   closed flag last. */
static void emit_range_eq(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    lower_gtype *g = elem ? lower_range_family(c, lower_scalar_kind(elem))
                          : NULL;
    const char *w;

    if (!g) {
        lower_d_printf(d, "    return 0;\n");
        return;
    }
    switch (lower_scalar_kind(elem)) {
    case EKSC_UINT: w = "u"; break;
    case EKSC_CHAR: w = "c"; break;
    default:        w = "i"; break;
    }
    lower_d_printf(d, "    if (((ekS_%s *)a.as.ref)->f_start.as.%s != "
                      "((ekS_%s *)b.as.ref)->f_start.as.%s)\n", g->stem, w,
                   g->stem, w);
    lower_d_printf(d, "        return 0;\n");
    lower_d_printf(d, "    if (((ekS_%s *)a.as.ref)->f_end.as.%s != "
                      "((ekS_%s *)b.as.ref)->f_end.as.%s)\n", g->stem, w,
                   g->stem, w);
    lower_d_printf(d, "        return 0;\n");
    lower_d_printf(d, "    return ((ekS_%s *)a.as.ref)->f_closed.as.i == "
                      "((ekS_%s *)b.as.ref)->f_closed.as.i;\n", g->stem,
                   g->stem);
}

/* The tuple or record receiver: the fields of the shape's own layout.
   The struct family spells a record field by its name and a tuple
   field by its position; the walk reads the same slots the typedef
   declares. */
static void emit_layout_eq(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    size_t nf;

    if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
        emit_fields_eq(c, d, g->stem, t->kind == EK_T_RECORD ? fnames : NULL,
                       ftypes, nf);
    } else {
        lower_d_printf(d, "    return 0;\n");
    }
}

/* The nominal receiver: the algebraic identity test, or the declared
   representation's field walk. */
static void emit_nominal_eq(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    lower_variant *vs = NULL;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    size_t nv = 0, nf;

    if (lower_variants_of(c, NULL, t, &vs, &nv, 0)
        || lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g)) {
        if (vs) {
            emit_adt_eq(c, d, vs, nv);
        } else if (g && g->shape == GT_REC) {
            emit_fields_eq(c, d, g->stem, fnames, ftypes, nf);
        } else if (g) {
            emit_fields_eq(c, d, g->stem, NULL, ftypes, nf);
        } else {
            lower_d_printf(d, "    return 0;\n");
        }
        return;
    }
    lower_d_printf(d, "    return 0;\n");
}

void lower_dsvc_eq_emit(lower_ctx *c, const char *name, const ek_type *t)
{
    lower_demit d;
    ek_buf b;

    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static int %s(ek_val a, ek_val b)\n{\n", name);
    if (lower_is_range(t)) {
        emit_range_eq(c, &d, t);
    } else if (lower_is_set(t)) {
        emit_set_eq(c, &d, t);
    } else if (lower_is_vector(t)) {
        lower_vec_eq_emit(c, &d, t);
    } else if (lower_scalar_kind(t) != EKSC_OTHER) {
        emit_scalar_eq(c, &d, lower_scalar_kind(t));
    } else if (t->kind == EK_T_UNIT) {
        /* every unit value is equal; the signature's slots stay
           unread */
        lower_d_printf(&d, "    (void)a;\n");
        lower_d_printf(&d, "    (void)b;\n");
        lower_d_printf(&d, "    return 1;\n");
    } else if (t->kind == EK_T_LIST) {
        emit_list_eq(c, &d, t);
    } else if (t->kind == EK_T_DICT) {
        emit_dict_eq(c, &d, t);
    } else if (t->kind == EK_T_TUPLE || t->kind == EK_T_RECORD) {
        emit_layout_eq(c, &d, t);
    } else if (t->kind == EK_T_NOMINAL) {
        emit_nominal_eq(c, &d, t);
    } else {
        emit_opaque_eq(&d);
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}

