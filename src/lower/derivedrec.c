#include <string.h>

#include "lower/internal.h"

/* The record and tuple decoders of from_json: a record reads its
   fields out of an Obj by key bytes, admitting no extra key and no
   missing field; a tuple reads its components out of an Arr in order,
   admitting no other length. Every failing decode releases the ones
   decoded before it, and the kept Json field values beside them. */

/* The component decodes of a tuple, each failing release keeping the
   ones decoded before it. */
void lower_from_tuple_emit(lower_ctx *c, lower_demit *d, lower_gtype *g,
                           const ek_type **ftypes, size_t nf)
{
    const char **rs;
    size_t cap = 0, i;

    rs = lower_grow(c, NULL, &cap, nf ? nf : 1, sizeof *rs);
    if (!rs) {
        return;
    }
    g->constructed = 1;
    lower_d_printf(d, "    if (v.as.ref->info != &ekT_std$Json$Arr)\n");
    lower_json_err_lit(d, "    ", "");
    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        ek_val cell = %s;\n",
                   lower_json_slot(c, "Arr", "v"));
    lower_d_printf(d, "        ek_val probe = cell;\n");
    lower_d_printf(d, "        size_t n = 0;\n\n");
    lower_d_printf(d, "        while (ek_val_is_cons(probe)) {\n");
    lower_d_printf(d, "            n++;\n");
    lower_d_printf(d, "            probe = ek_cons_tail(probe.as.ref);\n");
    lower_d_printf(d, "        }\n");
    lower_d_printf(d, "        if (n != %zu)\n", nf);
    lower_json_err_lit(d, "            ", "");
    for (i = 0; i < nf; i++) {
        const char *f = lower_fromj_of(c, ftypes[i]);

        rs[i] = lower_str(c, "r%zu", i);
        lower_d_printf(d, "        ek_val %s = %s(ek_cons_head(cell.as.ref))"
                          ";\n", rs[i], f ? f : "0");
        if (!f) {
            lower_json_err_lit(d, "            ", "");
            lower_d_printf(d, "        }\n");
            return;
        }
        if (i) {
            lower_d_printf(d, "        if (%s.as.ref->info != %s) {\n",
                           rs[i], LOWER_OK_INFO);
            for (cap = 0; cap < i; cap++) {
                lower_d_printf(d, "            ek_release(%s);\n", rs[cap]);
            }
            lower_d_printf(d, "            return %s;\n", rs[i]);
            lower_d_printf(d, "        }\n");
        } else {
            lower_d_printf(d, "        if (%s.as.ref->info != %s)\n", rs[i],
                           LOWER_OK_INFO);
            lower_d_printf(d, "            return %s;\n", rs[i]);
        }
        if (i + 1 < nf) {
            lower_d_printf(d, "        cell = ek_cons_tail(cell.as.ref);\n");
        }
    }
    lower_d_printf(d, "        ek_val out = %s(", g->helper);
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "%s((ekS_std$Result$Ok *)%s.as.ref)->p0",
                       i ? ", " : "", rs[i]);
    }
    lower_d_printf(d, ");\n");
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "        ek_release(%s);\n", rs[i]);
        lower_d_printf(d, "        ek_release(f%zu);\n", i);
    }
    lower_d_printf(d, "        w = %s(out);\n", lower_result_ctor(c, "Ok"));
    lower_d_printf(d, "        ek_release(out);\n");
    lower_d_printf(d, "        return w;\n");
    lower_d_printf(d, "    }\n");
}

/* The content lookup of an Obj's entries: equal bytes, whichever
   object spelled the key. */
static void emit_find_helper(lower_ctx *c, const char *fn)
{
    lower_demit d;
    ek_buf b;

    c->uses_string_h = 1;
    lower_d_begin(c, &d, &b);
    lower_push_sig(c, lower_str(c, "static int %s(ek_val d, const char *name,"
                                   " ek_val *out)", fn));
    lower_d_printf(&d, "static int %s(ek_val d, const char *name, ek_val "
                       "*out)\n{\n", fn);
    lower_d_printf(&d, "    ek_val k, x;\n");
    lower_d_printf(&d, "    ek_dict_iter it;\n");
    lower_d_printf(&d, "    int hit = 0;\n\n");
    lower_d_printf(&d, "    ek_dict_iter_begin(&it, d);\n");
    lower_d_printf(&d, "    while (!hit && ek_dict_iter_next(&it, &k, &x))"
                       " {\n");
    lower_d_printf(&d, "        if (k.tag == EK_VAL_REF && k.as.ref != "
                       "(ek_obj *)0\n");
    lower_d_printf(&d, "            && k.as.ref->info->kind == EK_KIND_STR\n");
    lower_d_printf(&d, "            && ek_str_len(k.as.ref) == "
                       "strlen(name)\n");
    lower_d_printf(&d, "            && memcmp(ek_str_cstr(k.as.ref), name, "
                       "strlen(name)) == 0) {\n");
    lower_d_printf(&d, "            *out = x;\n");
    lower_d_printf(&d, "            hit = 1;\n");
    lower_d_printf(&d, "        }\n");
    lower_d_printf(&d, "    }\n");
    lower_d_printf(&d, "    ek_dict_iter_end(&it);\n");
    lower_d_printf(&d, "    return hit;\n");
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}

void lower_from_record_emit(lower_ctx *c, lower_demit *d, const char *name,
                            const char **fnames, const ek_type **ftypes,
                            size_t nf, lower_gtype *g)
{
    const char *find = lower_str(c, "ekdJ_%s", name);
    const char **rs;
    size_t cap = 0, i;

    rs = lower_grow(c, NULL, &cap, nf ? nf : 1, sizeof *rs);
    if (!rs) {
        return;
    }
    emit_find_helper(c, find);
    g->constructed = 1;
    lower_d_printf(d, "    if (v.as.ref->info != &ekT_std$Json$Obj)\n");
    lower_json_err_lit(d, "    ", "");
    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        ek_val d = %s;\n", lower_json_slot(c, "Obj",
                                                                 "v"));
    lower_d_printf(d, "        ek_val k, x, w;\n");
    for (i = 0; i < nf; i++) {
        rs[i] = lower_str(c, "r%zu", i);
        lower_d_printf(d, "        ek_val %s, f%zu;\n", rs[i], i);
    }
    lower_d_printf(d, "        ek_dict_iter it;\n");
    lower_d_printf(d, "        int hit;\n");
    lower_d_printf(d, "        int bad = 0;\n\n");
    lower_d_printf(d, "        ek_dict_iter_begin(&it, d);\n");
    lower_d_printf(d, "        while (!bad && ek_dict_iter_next(&it, &k, &x))"
                       " {\n");
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "%s!",
                       i ? "                && " : "            if (");
        lower_val_text_is(d, "k", fnames ? fnames[i] : "");
        lower_d_printf(d, "\n");
    }
    if (nf) {
        lower_d_printf(d, "            )\n");
    }
    if (nf) {
        lower_d_printf(d, "            bad = 1;\n");
    } else {
        lower_d_printf(d, "            if (ek_dict_len(d) != 0)\n");
        lower_d_printf(d, "                bad = 1;\n");
    }
    lower_d_printf(d, "        }\n");
    lower_d_printf(d, "        ek_dict_iter_end(&it);\n");
    lower_d_printf(d, "        if (bad)\n");
    lower_json_err_obj(d, "            ", "ek_keep(k)");
    for (i = 0; i < nf; i++) {
        const char *lit = lower_d_cstr(c, fnames ? fnames[i] : "");

        lower_d_printf(d, "        hit = %s(d, %s, &x);\n", find, lit);
        lower_d_printf(d, "        if (!hit) {\n");
        for (cap = 0; cap < i; cap++) {
            lower_d_printf(d, "            ek_release(f%zu);\n", cap);
        }
        lower_json_err_lit(d, "            ", fnames ? fnames[i] : "");
        lower_d_printf(d, "        }\n");
        lower_d_printf(d, "        f%zu = ek_keep(x);\n", i);
    }
    for (i = 0; i < nf; i++) {
        const char *f = lower_fromj_of(c, ftypes[i]);

        lower_d_printf(d, "        %s = %s(f%zu);\n", rs[i], f ? f : "0",
                       i);
        if (!f) {
            size_t k2;

            for (k2 = 0; k2 < i; k2++) {
                lower_d_printf(d, "        ek_release(%s);\n", rs[k2]);
            }
            lower_json_err_lit(d, "            ", "");
            lower_d_printf(d, "    }\n");
            return;
        }
        lower_d_printf(d, "        if (%s.as.ref->info != %s) {\n", rs[i],
                       LOWER_OK_INFO);
        for (cap = 0; cap < i; cap++) {
            lower_d_printf(d, "            ek_release(%s);\n", rs[cap]);
        }
        for (cap = 0; cap < nf; cap++) {
            lower_d_printf(d, "            ek_release(f%zu);\n", cap);
        }
        lower_d_printf(d, "            return %s;\n", rs[i]);
        lower_d_printf(d, "        }\n");
    }
    lower_d_printf(d, "        ek_val out = %s(", g->helper);
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "%s((ekS_std$Result$Ok *)%s.as.ref)->p0",
                       i ? ", " : "", rs[i]);
    }
    lower_d_printf(d, ");\n");
    for (i = 0; i < nf; i++) {
        lower_d_printf(d, "        ek_release(%s);\n", rs[i]);
        lower_d_printf(d, "        ek_release(f%zu);\n", i);
    }
    lower_d_printf(d, "        w = %s(out);\n", lower_result_ctor(c, "Ok"));
    lower_d_printf(d, "        ek_release(out);\n");
    lower_d_printf(d, "        return w;\n");
    lower_d_printf(d, "    }\n");
}

