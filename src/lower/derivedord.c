#include "lower/internal.h"

/* The ordering service over an applied type. Scalars order by
   payload, Strings and Bytes by their bytes, an algebraic type by
   declaration rank with the payloads compared only within one rank,
   and the range's endpoints lexicographically with the closed flag
   last. Ordering answers -1, 0, or 1. */

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
