#include <string.h>

#include "lower/internal.h"

/* The structural equality and ordering services over an applied type.
   Scalars compare by payload, Strings and Bytes by their bytes,
   containers element by element, records and tuples field by field in
   layout order with short-circuit, and algebraic values by constructor
   identity then payload. Ordering answers -1, 0, or 1. */

static const char *eq_of(lower_ctx *c, const ek_type *t)
{
    return lower_dsvc_request(c, NULL, DSV_EQ, t);
}

/* The comparison expression over two borrowed slots: the family's
   service, or the payload order of a scalar the operator layer owns. */
static const char *ord_call(lower_ctx *c, const ek_type *t, const char *x,
                            const char *y)
{
    const char *f = lower_dsvc_request(c, NULL, DSV_ORD, t);

    if (f) {
        return lower_str(c, "%s(%s, %s)", f, x, y);
    }
    switch (lower_scalar_kind(t)) {
    case EKSC_UINT:
        return lower_str(c, "(%s.as.u > %s.as.u) - (%s.as.u < %s.as.u)", x, y,
                         x, y);
    case EKSC_FLOAT:
        return lower_str(c, "(%s.as.f > %s.as.f) - (%s.as.f < %s.as.f)", x, y,
                         x, y);
    case EKSC_CHAR:
        return lower_str(c, "(%s.as.c > %s.as.c) - (%s.as.c < %s.as.c)", x, y,
                         x, y);
    default:
        return lower_str(c, "(%s.as.i > %s.as.i) - (%s.as.i < %s.as.i)", x, y,
                         x, y);
    }
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
    lower_d_printf(d, "    size_t cur = 0;\n\n");
    lower_d_printf(d, "    if (ek_dict_len(a) != ek_dict_len(b))\n");
    lower_d_printf(d, "        return 0;\n");
    lower_d_printf(d, "    while (ek_dict_next(a, &cur, &k, &x)) {\n");
    lower_d_printf(d, "        if (!ek_dict_get(b, k, &y))\n");
    lower_d_printf(d, "            return 0;\n");
    lower_d_printf(d, "        if (!%s(x, y))\n", val);
    lower_d_printf(d, "            return 0;\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    return 1;\n");
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

void lower_dsvc_eq_emit(lower_ctx *c, const char *name, const ek_type *t)
{
    lower_demit d;
    ek_buf b;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    lower_variant *vs;
    size_t nf, nv;

    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static int %s(ek_val a, ek_val b)\n{\n", name);
    if (lower_is_range(t)) {
        emit_range_eq(c, &d, t);
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
        if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
            /* The struct family spells a record field by its name and
               a tuple field by its position; the walk reads the same
               slots the typedef declares. */
            emit_fields_eq(c, &d, g->stem,
                           t->kind == EK_T_RECORD ? fnames : NULL, ftypes,
                           nf);
        } else {
            lower_d_printf(&d, "    return 0;\n");
        }
    } else if (t->kind == EK_T_NOMINAL
               && (lower_variants_of(c, NULL, t, &vs, &nv, 0)
                   || lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g))) {
        if (vs) {
            emit_adt_eq(c, &d, vs, nv);
        } else if (g && g->shape == GT_REC) {
            emit_fields_eq(c, &d, g->stem, fnames, ftypes, nf);
        } else if (g) {
            emit_fields_eq(c, &d, g->stem, NULL, ftypes, nf);
        } else {
            lower_d_printf(&d, "    return 0;\n");
        }
    } else {
        emit_opaque_eq(&d);
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}

/* The declaration-order ranking of an algebraic receiver: the ranks
   compare first, the payloads only within one rank. */
static void emit_adt_ord(lower_ctx *c, lower_demit *d, lower_variant *vs,
                         size_t nv)
{
    size_t i;

    lower_d_printf(d, "    int ra = -1;\n");
    lower_d_printf(d, "    int rb = -1;\n\n");
    for (i = 0; i < nv; i++) {
        lower_d_printf(d, "    if (a.as.ref->info == &ekT_%s)\n"
                           "        ra = %zu;\n", vs[i].stem, i);
        lower_d_printf(d, "    if (b.as.ref->info == &ekT_%s)\n"
                           "        rb = %zu;\n", vs[i].stem, i);
    }
    lower_d_printf(d, "    if (ra != rb)\n");
    lower_d_printf(d, "        return ra < rb ? -1 : 1;\n");
    for (i = 0; i < nv; i++) {
        const char *sa;
        const char *sb;
        const char *call;

        if (!vs[i].payload) {
            continue;
        }
        sa = lower_str(c, "((ekS_%s *)a.as.ref)->p0", vs[i].stem);
        sb = lower_str(c, "((ekS_%s *)b.as.ref)->p0", vs[i].stem);
        call = ord_call(c, vs[i].payload, sa, sb);
        lower_d_printf(d, "    if (ra == %zu)\n", i);
        lower_d_printf(d, "        return %s;\n", call);
    }
    lower_d_printf(d, "    return 0;\n");
}

/* The range's order: the endpoints lexicographically, the closed
   flag last, False before True. */
static void emit_range_ord(lower_ctx *c, lower_demit *d, const ek_type *t)
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
    lower_d_printf(d, "        return ((ekS_%s *)a.as.ref)->f_start.as.%s "
                      "< ((ekS_%s *)b.as.ref)->f_start.as.%s ? -1 : 1;\n",
                   g->stem, w, g->stem, w);
    lower_d_printf(d, "    if (((ekS_%s *)a.as.ref)->f_end.as.%s != "
                      "((ekS_%s *)b.as.ref)->f_end.as.%s)\n", g->stem, w,
                   g->stem, w);
    lower_d_printf(d, "        return ((ekS_%s *)a.as.ref)->f_end.as.%s "
                      "< ((ekS_%s *)b.as.ref)->f_end.as.%s ? -1 : 1;\n",
                   g->stem, w, g->stem, w);
    lower_d_printf(d, "    if (((ekS_%s *)a.as.ref)->f_closed.as.i != "
                      "((ekS_%s *)b.as.ref)->f_closed.as.i)\n", g->stem,
                   g->stem);
    lower_d_printf(d, "        return ((ekS_%s *)a.as.ref)->f_closed.as.i "
                      "< ((ekS_%s *)b.as.ref)->f_closed.as.i ? -1 : 1;\n",
                   g->stem, g->stem);
    lower_d_printf(d, "    return 0;\n");
}

void lower_dsvc_ord_emit(lower_ctx *c, const char *name, const ek_type *t)
{
    lower_demit d;
    ek_buf b;
    lower_variant *vs = NULL;
    size_t nv = 0;

    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static int %s(ek_val a, ek_val b)\n{\n", name);
    if (lower_is_range(t)) {
        emit_range_ord(c, &d, t);
    } else if (t->kind == EK_T_NOMINAL
               && lower_variants_of(c, NULL, t, &vs, &nv, 0)) {
        emit_adt_ord(c, &d, vs, nv);
    } else if (lower_scalar_kind(t) == EKSC_STRING) {
        lower_d_printf(&d, "    return ek_str_cmp(a.as.ref, b.as.ref);\n");
    } else if (lower_scalar_kind(t) == EKSC_BYTES) {
        c->uses_string_h = 1;
        lower_d_printf(&d, "    size_t la = ek_bytes_len(a.as.ref);\n");
        lower_d_printf(&d, "    size_t lb = ek_bytes_len(b.as.ref);\n");
        lower_d_printf(&d, "    size_t m = la < lb ? la : lb;\n");
        lower_d_printf(&d, "    int order = memcmp(ek_bytes_data(a.as.ref), "
                           "ek_bytes_data(b.as.ref), m);\n\n");
        lower_d_printf(&d, "    if (order != 0)\n");
        lower_d_printf(&d, "        return order < 0 ? -1 : 1;\n");
        lower_d_printf(&d, "    return la < lb ? -1 : la > lb ? 1 : 0;\n");
    } else {
        lower_d_printf(&d, "    return 0;\n");
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}
