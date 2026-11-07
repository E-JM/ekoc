#include "lower/internal.h"

/* The algebraic decoders of from_json: external tagging answers a
   zero-payload variant from the bare Str naming it and a payload
   variant from the single-entry Obj whose key names it, the payload
   decoding through its own decode service. Every other Str or Obj is
   the Invalid refusal of the variant search. */

/* The variant-name match one decode arm emits: the Str arm reads the
   string object the slot carries, the Obj arm the entry key. A detail
   closes the match with the Invalid refusal naming it; NULL opens the
   construct block the arm builds. */
static void emit_variant_is(lower_demit *d, const char *slot,
                            const char *name, int key, const char *detail)
{
    lower_d_printf(d, "        if (");
    if (key) {
        lower_val_text_is(d, slot, name);
    } else {
        lower_obj_text_is(d, slot, name);
    }
    if (detail) {
        lower_d_printf(d, ")\n");
        lower_json_err_lit(d, "            ", detail);
        return;
    }
    lower_d_printf(d, ") {\n");
}

/* The Str arm: a zero-payload variant decodes from its bare name, a
   payload variant refuses with its name, and an unmatched string is
   the unknown variant's Invalid. */
static void emit_from_adt_tag(lower_ctx *c, lower_demit *d,
                              lower_variant *vs, size_t nv)
{
    size_t i;

    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Str) {\n");
    lower_d_printf(d, "        ek_obj *s = %s.as.ref;\n\n",
                   lower_json_slot(c, "Str", "v"));
    for (i = 0; i < nv; i++) {
        if (vs[i].payload) {
            continue;
        }
        emit_variant_is(d, "s", vs[i].name, 0, NULL);
        lower_d_printf(d, "            ek_val q = ekn_%s();\n", vs[i].stem);
        lower_d_printf(d, "            ek_val w = %s(q);\n",
                       lower_result_ctor(c, "Ok"));
        lower_d_printf(d, "            ek_release(q);\n");
        lower_d_printf(d, "            return w;\n");
        lower_d_printf(d, "        }\n");
    }
    for (i = 0; i < nv; i++) {
        if (!vs[i].payload) {
            continue;
        }
        emit_variant_is(d, "s", vs[i].name, 0, vs[i].name);
    }
    lower_json_err_obj(d, "        ", "ek_keep(ek_val_ref(s))");
    lower_d_printf(d, "    }\n");
}

/* The Obj arm: the single entry's key names the variant and its
   payload decodes through the payload's own decode service. */
static void emit_from_adt_payload(lower_ctx *c, lower_demit *d,
                                  lower_variant *vs, size_t nv)
{
    size_t i;

    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Obj) {\n");
    lower_d_printf(d, "        ek_val d = %s;\n",
                   lower_json_slot(c, "Obj", "v"));
    lower_d_printf(d, "        ek_val k, x;\n");
    lower_d_printf(d, "        ek_dict_iter it;\n\n");
    lower_d_printf(d, "        if (ek_dict_len(d) != 1)\n");
    lower_json_err_lit(d, "            ", "");
    lower_d_printf(d, "        ek_dict_iter_begin(&it, d);\n");
    lower_d_printf(d, "        (void)ek_dict_iter_next(&it, &k, &x);\n");
    lower_d_printf(d, "        ek_dict_iter_end(&it);\n");
    for (i = 0; i < nv; i++) {
        if (vs[i].payload) {
            continue;
        }
        emit_variant_is(d, "k", vs[i].name, 1, vs[i].name);
    }
    for (i = 0; i < nv; i++) {
        const char *f = vs[i].payload ? lower_fromj_of(c, vs[i].payload) : NULL;

        if (!vs[i].payload) {
            continue;
        }
        if (!f) {
            emit_variant_is(d, "k", vs[i].name, 1, "");
            continue;
        }
        emit_variant_is(d, "k", vs[i].name, 1, NULL);
        lower_d_printf(d, "            ek_val r = %s(x);\n", f);
        lower_d_printf(d, "            if (r.as.ref->info != %s)\n", LOWER_OK_INFO);
        lower_d_printf(d, "                return r;\n");
        lower_d_printf(d, "            ek_val q = ekn_%s("
                          "((ekS_std$Result$Ok *)r.as.ref)->p0);\n",
                       vs[i].stem);
        lower_d_printf(d, "            ek_release(r);\n");
        lower_d_printf(d, "            ek_val w = %s(q);\n",
                       lower_result_ctor(c, "Ok"));
        lower_d_printf(d, "            ek_release(q);\n");
        lower_d_printf(d, "            return w;\n");
        lower_d_printf(d, "        }\n");
    }
    lower_json_err_obj(d, "        ", "ek_keep(k)");
    lower_d_printf(d, "    }\n");
}

/* External tagging, decoded: the bare string names a zero-payload
   variant, the single-key object a payload variant. */
void lower_from_adt_emit(lower_ctx *c, lower_demit *d, lower_variant *vs,
                         size_t nv)
{
    emit_from_adt_tag(c, d, vs, nv);
    emit_from_adt_payload(c, d, vs, nv);
    lower_json_err_lit(d, "    ", "");
}
