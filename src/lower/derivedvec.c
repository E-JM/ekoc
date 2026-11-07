#include "lower/internal.h"

/* The derived services of the runtime-backed vector: the five bodies
   the registry mints over the std placeholder, one per family. Every
   body walks the vector through the runtime's index-order list -- the
   brace form of the two display channels, the index-for-index
   equality, the element mixing, the Arr of the to_json direction, and
   the decode that rebuilds the vector by push -- so one reading of
   the type carries all five: the applied element stands in for the
   walk, and the list dies once the body's own value stands. */

/* The brace form over a vector: the runtime's index-order list, which
   the walk owns and releases. */
void lower_vec_show_emit(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    lower_d_printf(d, "    ek_val list = ek_vec_to_list(v);\n");
    lower_seq_show_emit(c, d, t->u.nominal.args[0], "list", 1);
}

/* Vector equality is index-order equality: the lengths agree and the
   elements compare equal at every index. The element comparison is
   the derived service, so a vector of structural values compares
   structurally. */
void lower_vec_eq_emit(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    const char *eq = elem ? lower_dsvc_request(c, NULL, DSV_EQ, elem) : NULL;

    if (!eq) {
        lower_d_printf(d, "    return 0;\n");
        return;
    }
    lower_d_printf(d, "    ek_val la = ek_vec_to_list(a);\n");
    lower_d_printf(d, "    ek_val lb = ek_vec_to_list(b);\n");
    lower_d_printf(d, "    int same = ek_list_len(la) == ek_list_len(lb);\n");
    lower_d_printf(d, "    ek_val p = la;\n");
    lower_d_printf(d, "    ek_val q = lb;\n\n");
    lower_d_printf(d, "    while (same && ek_val_is_cons(p)\n");
    lower_d_printf(d, "                   && ek_val_is_cons(q)) {\n");
    lower_d_printf(d, "        if (!%s(ek_cons_head(p.as.ref),\n", eq);
    lower_d_printf(d, "                   ek_cons_head(q.as.ref)))\n");
    lower_d_printf(d, "            same = 0;\n");
    lower_d_printf(d, "        p = ek_cons_tail(p.as.ref);\n");
    lower_d_printf(d, "        q = ek_cons_tail(q.as.ref);\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    ek_release(la);\n");
    lower_d_printf(d, "    ek_release(lb);\n");
    lower_d_printf(d, "    return same;\n");
}

/* Element mixing over a vector, in index order: the walk runs in the
   equality's own order, so vectors equal under the derived eq mix the
   same word. The runtime's list dies once the walk ends. */
void lower_vec_hash_emit(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    const char *eh = elem ? lower_dsvc_request(c, NULL, DSV_HASH, elem)
                          : NULL;

    if (!eh) {
        lower_d_printf(d, "    return h;\n");
        return;
    }
    lower_d_printf(d, "    ek_val list = ek_vec_to_list(v);\n");
    lower_d_printf(d, "    ek_val p = list;\n\n");
    lower_d_printf(d, "    while (ek_val_is_cons(p)) {\n");
    lower_d_printf(d, "        h = ek_hash_mix(h, %s(ek_cons_head"
                      "(p.as.ref)));\n", eh);
    lower_d_printf(d, "        p = ek_cons_tail(p.as.ref);\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    ek_release(list);\n");
    lower_d_printf(d, "    return h;\n");
}

/* A vector maps to the Arr of its elements in index order: the walk
   over the runtime's list, released once the Arr stands. */
void lower_vec_tojson_emit(lower_ctx *c, lower_demit *d, const char *name,
                           const ek_type *t)
{
    const char *aux = lower_str(c, "%s_aux", name);
    const char *arr = lower_d_temp(d);

    lower_to_list_aux(c, aux, t->u.nominal.args[0]);
    lower_d_printf(d, "    ek_val list = ek_vec_to_list(v);\n");
    lower_d_printf(d, "    ek_val %s = %s(list);\n", arr, aux);
    lower_d_printf(d, "    ek_release(list);\n");
    lower_d_printf(d, "    return %s(%s);\n", lower_json_ctor(c, "Arr"), arr);
}

/* A vector decodes through the list walk in array order, then builds
   over the decoded elements by push, so the array's order is the
   index order; the list dies once the vector holds its own refs. */
void lower_vec_fromjson_emit(lower_ctx *c, lower_demit *d, const char *name,
                             const ek_type *t)
{
    const char *aux = lower_str(c, "%s_aux", name);
    const char *ok = lower_result_ctor(c, "Ok");

    lower_from_list_aux(c, aux, t->u.nominal.args[0]);
    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Arr) {\n");
    lower_d_printf(d, "        ek_val r = %s(%s);\n", aux,
                   lower_json_slot(c, "Arr", "v"));
    lower_d_printf(d, "        ek_val list, vec, out;\n\n");
    lower_d_printf(d, "        if (r.as.ref->info != %s)\n", LOWER_OK_INFO);
    lower_d_printf(d, "            return r;\n");
    lower_d_printf(d, "        list = ((ekS_std$Result$Ok *)r.as.ref)->p0;\n");
    lower_d_printf(d, "        vec = ek_vec_new();\n");
    lower_d_printf(d, "        while (ek_val_is_cons(list)) {\n");
    lower_d_printf(d, "            ek_val next = ek_vec_push(vec, "
                       "ek_cons_head(list.as.ref));\n");
    lower_d_printf(d, "            ek_release(vec);\n");
    lower_d_printf(d, "            vec = next;\n");
    lower_d_printf(d, "            list = ek_cons_tail(list.as.ref);\n");
    lower_d_printf(d, "        }\n");
    lower_d_printf(d, "        out = %s(vec);\n", ok);
    lower_d_printf(d, "        ek_release(vec);\n");
    lower_d_printf(d, "        ek_release(r);\n");
    lower_d_printf(d, "        return out;\n");
    lower_d_printf(d, "    }\n");
    lower_json_err_lit(d, "    ", "");
}
