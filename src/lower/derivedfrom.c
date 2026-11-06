#include <string.h>

#include "lower/internal.h"

/* The decode side of the derived pair: from_json reads one applied
   type out of the std Json value and answers the std Result, Err the
   ParseError the std source closes over. A missing field, an unknown
   key or variant, or a payload type mismatch yields Err; the Ok path
   satisfies the spec's involution. */



/* The machine-word row a nominal names, for the checked conversions. */
static const char *word_name(const ek_type *t)
{
    static const struct {
        const char *name;
        const char *word;
    } names[] = {
        {"I8", "i8"}, {"I16", "i16"}, {"I32", "i32"}, {"I64", "i64"},
        {"U8", "u8"}, {"U16", "u16"}, {"U32", "u32"}, {"U64", "u64"}
    };
    size_t i;

    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym) {
        return NULL;
    }
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        if (strcmp(names[i].name, t->u.nominal.sym->name) == 0) {
            return names[i].word;
        }
    }
    return NULL;
}

static void emit_from_scalar(lower_ctx *c, lower_demit *d, lower_scalar sk,
                             const ek_type *t)
{
    const char *word = word_name(t);

    if (sk == EKSC_INT && !word) {
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Int)\n");
        lower_json_ok_emit(d, "        ", lower_json_slot(c, "Int", "v"));
        lower_json_err_lit(d, "    ", "");
        return;
    }
    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Int) {\n");
    lower_d_printf(d, "        ek_val p = %s;\n", lower_json_slot(c, "Int",
                                                                 "v"));
    if (sk == EKSC_UINT) {
        lower_d_printf(d, "        if (p.as.i >= 0)\n");
        lower_json_ok_emit(d, "            ", "ek_val_uint((uint64_t)p.as.i)");
    } else if (word) {
        const char *v = word[0] == 'u' ? "uo" : "io";

        lower_d_printf(d, "        %s %s;\n\n",
                       word[0] == 'u' ? "uint64_t" : "int64_t", v);
        lower_d_printf(d, "        if (ek_try_to_%s(p, &%s))\n", word, v);
        lower_json_ok_emit(d, "            ",
                word[0] == 'u' ? "ek_val_uint(uo)" : "ek_val_int(io)");
    } else {
        lower_d_printf(d, "        (void)p;\n");
    }
    lower_d_printf(d, "    }\n");
    lower_json_err_lit(d, "    ", "");
}

static void emit_from_simple(lower_ctx *c, lower_demit *d, lower_scalar sk)
{
    switch (sk) {
    case EKSC_FLOAT:
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Float)\n");
        lower_json_ok_emit(d, "        ", lower_json_slot(c, "Float", "v"));
        break;
    case EKSC_BOOL:
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Bool)\n");
        lower_json_ok_emit(d, "        ", lower_json_slot(c, "Bool", "v"));
        break;
    case EKSC_STRING:
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Str)\n");
        lower_json_ok_emit(d, "        ", lower_json_slot(c, "Str", "v"));
        break;
    case EKSC_CHAR:
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Str) {\n");
        lower_d_printf(d, "        ek_obj *s = %s.as.ref;\n",
                       lower_json_slot(c, "Str", "v"));
        lower_d_printf(d, "        uint32_t code;\n\n");
        lower_d_printf(d, "        if (ek_str_length(s) == 1"
                          " && ek_str_at(s, 0, &code))\n");
        lower_json_ok_emit(d, "            ", "ek_val_char(code)");
        lower_d_printf(d, "    }\n");
        break;
    case EKSC_BYTES:
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Str) {\n");
        lower_d_printf(d, "        ek_obj *s = %s.as.ref;\n",
                       lower_json_slot(c, "Str", "v"));
        lower_d_printf(d, "        ek_obj *o = (ek_obj *)0;\n");
        lower_d_printf(d, "        ek_obj *y = (ek_obj *)0;\n\n");
        lower_d_printf(d, "        if (ek_bytes_from_hex(s, &o, &y)) {\n");
        lower_d_printf(d, "            ek_val p = ek_val_ref(o);\n\n");
        lower_json_ok_emit(d, "            ", "p");
        lower_d_printf(d, "            ek_release(p);\n");
        lower_d_printf(d, "        }\n");
        lower_json_err_obj(d, "        ", "ek_val_ref(y)");
        lower_d_printf(d, "    }\n");
        break;
    default:
        lower_json_err_lit(d, "    ", "");
        return;
    }
    lower_json_err_lit(d, "    ", "");
}

/* The recursive cell decode behind an Arr: the walk answers the std
   Result, so the first failing element short-circuits. */
static void emit_from_list_aux(lower_ctx *c, const char *aux,
                               const ek_type *elemt)
{
    const char *f = lower_fromj_of(c, elemt);
    const char *ok = lower_result_ctor(c, "Ok");
    lower_demit d;
    ek_buf b;

    lower_d_begin(c, &d, &b);
    lower_push_sig(c, lower_str(c, "static ek_val %s(ek_val cell)", aux));
    lower_d_printf(&d, "static ek_val %s(ek_val cell)\n{\n", aux);
    if (!f) {
        lower_json_err_lit(&d, "    ", "");
    } else {
        lower_d_printf(&d, "    ek_val r, t, out, w;\n\n");
        lower_d_printf(&d, "    if (!ek_val_is_cons(cell))\n");
        lower_d_printf(&d, "        return %s(ek_nil_val());\n", ok);
        lower_d_printf(&d, "    r = %s(ek_cons_head(cell.as.ref));\n", f);
        lower_d_printf(&d, "    if (r.as.ref->info != %s)\n", LOWER_OK_INFO);
        lower_d_printf(&d, "        return r;\n");
        lower_d_printf(&d, "    t = %s(ek_cons_tail(cell.as.ref));\n", aux);
        lower_d_printf(&d, "    if (t.as.ref->info != %s) {\n", LOWER_OK_INFO);
        lower_d_printf(&d, "        ek_release(r);\n");
        lower_d_printf(&d, "        return t;\n");
        lower_d_printf(&d, "    }\n");
        lower_d_printf(&d, "    out = ek_cons(((ekS_std$Result$Ok *)r.as.ref)"
                           "->p0,\n");
        lower_d_printf(&d, "                ((ekS_std$Result$Ok *)t.as.ref)"
                           "->p0);\n");
        lower_d_printf(&d, "    ek_release(r);\n");
        lower_d_printf(&d, "    ek_release(t);\n");
        lower_d_printf(&d, "    w = %s(out);\n", ok);
        lower_d_printf(&d, "    ek_release(out);\n");
        lower_d_printf(&d, "    return w;\n");
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}

static void emit_from_list(lower_ctx *c, lower_demit *d, const char *name,
                           const ek_type *t)
{
    const char *aux = lower_str(c, "%s_aux", name);

    emit_from_list_aux(c, aux, t->u.list.elem);
    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Arr)\n");
    lower_d_printf(d, "        return %s(%s);\n", aux,
                   lower_json_slot(c, "Arr", "v"));
    lower_json_err_lit(d, "    ", "");
}

/* A set decodes through the list walk, then builds the set over the
   decoded elements under the element domain's hook pair; the list dies
   once the set holds its own refs. */
static void emit_from_set(lower_ctx *c, lower_demit *d, const char *name,
                          const ek_type *t)
{
    const char *aux = lower_str(c, "%s_aux", name);
    const char *ok = lower_result_ctor(c, "Ok");
    const char *kq = "ek_key_eq";
    const char *kh = "ek_key_hash";

    if (t->u.nominal.args[0]) {
        lower_key_pair(c, t->u.nominal.args[0], &kq, &kh);
    }
    emit_from_list_aux(c, aux, t->u.nominal.args[0]);
    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Arr) {\n");
    lower_d_printf(d, "        ek_val r = %s(%s);\n", aux,
                   lower_json_slot(c, "Arr", "v"));
    lower_d_printf(d, "        ek_val list, set, out;\n\n");
    lower_d_printf(d, "        if (r.as.ref->info != %s)\n", LOWER_OK_INFO);
    lower_d_printf(d, "            return r;\n");
    lower_d_printf(d, "        list = ((ekS_std$Result$Ok *)r.as.ref)->p0;\n");
    lower_d_printf(d, "        set = ek_set_of(list, %s, %s);\n", kq, kh);
    lower_d_printf(d, "        out = %s(set);\n", ok);
    lower_d_printf(d, "        ek_release(set);\n");
    lower_d_printf(d, "        ek_release(r);\n");
    lower_d_printf(d, "        return out;\n");
    lower_d_printf(d, "    }\n");
    lower_json_err_lit(d, "    ", "");
}

static void emit_from_dict(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *f = lower_fromj_of(c, t->u.dict.val);

    if (!f) {
        lower_json_err_lit(d, "    ", "");
        return;
    }
    lower_d_printf(d, "    if (v.as.ref->info != &ekT_std$Json$Obj)\n");
    lower_json_err_lit(d, "    ", "");
    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        ek_val src = %s;\n", lower_json_slot(c, "Obj",
                                                                   "v"));
    lower_d_printf(d, "        ek_val acc = ek_dict_new(ek_key_eq, "
                       "ek_key_hash);\n");
    lower_d_printf(d, "        ek_val k, x, e, w;\n");
    lower_d_printf(d, "        size_t cur = 0;\n\n");
    lower_d_printf(d, "        while (ek_dict_next(src, &cur, &k, &x)) {\n");
    lower_d_printf(d, "            ek_val r = %s(x);\n", f);
    lower_d_printf(d, "            if (r.as.ref->info != %s) {\n", LOWER_OK_INFO);
    lower_d_printf(d, "                ek_release(acc);\n");
    lower_d_printf(d, "                return r;\n");
    lower_d_printf(d, "            }\n");
    lower_d_printf(d, "            e = ek_dict_set(acc, k, "
                       "((ekS_std$Result$Ok *)r.as.ref)->p0);\n");
    lower_d_printf(d, "            ek_release(r);\n");
    lower_d_printf(d, "            ek_release(acc);\n");
    lower_d_printf(d, "            acc = e;\n");
    lower_d_printf(d, "        }\n");
    lower_d_printf(d, "        w = %s(acc);\n", lower_result_ctor(c, "Ok"));
    lower_d_printf(d, "        ek_release(acc);\n");
    lower_d_printf(d, "        return w;\n");
    lower_d_printf(d, "    }\n");
}

/* External tagging, decoded: the bare string names a zero-payload
   variant, the single-key object a payload variant. */
static void emit_from_adt(lower_ctx *c, lower_demit *d, lower_variant *vs,
                          size_t nv)
{
    size_t i;

    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Str) {\n");
    lower_d_printf(d, "        ek_obj *s = %s.as.ref;\n\n",
                   lower_json_slot(c, "Str", "v"));
    for (i = 0; i < nv; i++) {
        if (vs[i].payload) {
            continue;
        }
        lower_d_printf(d, "        if (");
        lower_obj_text_is(d, "s", vs[i].name);
        lower_d_printf(d, ") {\n");
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
        lower_d_printf(d, "        if (");
        lower_obj_text_is(d, "s", vs[i].name);
        lower_d_printf(d, ")\n");
        lower_json_err_lit(d, "            ", vs[i].name);
    }
    lower_json_err_obj(d, "        ", "ek_keep(ek_val_ref(s))");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Obj) {\n");
    lower_d_printf(d, "        ek_val d = %s;\n", lower_json_slot(c, "Obj",
                                                                 "v"));
    lower_d_printf(d, "        ek_val k, x;\n");
    lower_d_printf(d, "        size_t cur = 0;\n\n");
    lower_d_printf(d, "        if (ek_dict_len(d) != 1)\n");
    lower_json_err_lit(d, "            ", "");
    lower_d_printf(d, "        (void)ek_dict_next(d, &cur, &k, &x);\n");
    for (i = 0; i < nv; i++) {
        if (vs[i].payload) {
            continue;
        }
        lower_d_printf(d, "        if (");
        lower_val_text_is(d, "k", vs[i].name);
        lower_d_printf(d, ")\n");
        lower_json_err_lit(d, "            ", vs[i].name);
    }
    for (i = 0; i < nv; i++) {
        const char *f = vs[i].payload ? lower_fromj_of(c, vs[i].payload) : NULL;

        if (!vs[i].payload) {
            continue;
        }
        if (!f) {
            lower_d_printf(d, "        if (");
            lower_val_text_is(d, "k", vs[i].name);
            lower_d_printf(d, ")\n");
            lower_json_err_lit(d, "            ", "");
            continue;
        }
        lower_d_printf(d, "        if (");
        lower_val_text_is(d, "k", vs[i].name);
        lower_d_printf(d, ") {\n");
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
    lower_json_err_lit(d, "    ", "");
}

/* The shape dispatch of the decode side, after the scalar slots. */
static void emit_from_shape(lower_ctx *c, lower_demit *d, const char *name,
                            const ek_type *t)
{
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    lower_variant *vs;
    size_t nf, nv;

    if (t->kind == EK_T_UNIT) {
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_std$Json$Null)\n");
        lower_json_ok_emit(d, "        ", "ek_val_unit()");
        lower_json_err_lit(d, "    ", "");
        return;
    }
    if (t->kind == EK_T_LIST) {
        emit_from_list(c, d, name, t);
        return;
    }
    if (lower_is_set(t)) {
        emit_from_set(c, d, name, t);
        return;
    }
    if (t->kind == EK_T_DICT) {
        emit_from_dict(c, d, t);
        return;
    }
    if ((t->kind == EK_T_TUPLE || t->kind == EK_T_RECORD
         || t->kind == EK_T_NOMINAL)
        && lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
        if (g->shape == GT_TUP) {
            lower_from_tuple_emit(c, d, g, ftypes, nf);
        } else {
            lower_from_record_emit(c, d, name, fnames, ftypes, nf, g);
        }
        return;
    }
    if (t->kind == EK_T_NOMINAL
        && lower_variants_of(c, NULL, t, &vs, &nv, 1)) {
        /* The decode body constructs each variant it answers, so the
           families register as constructed even when the source never
           builds one: the helpers join the unit beside the typeinfo. */
        emit_from_adt(c, d, vs, nv);
        return;
    }
    lower_json_err_lit(d, "    ", "");
}

void lower_dsvc_from_emit(lower_ctx *c, const char *name, const ek_type *t)
{
    lower_demit d;
    ek_buf b;
    lower_scalar sk = lower_scalar_kind(t);

    /* the body reads the std Json shapes it tests and unwraps */
    lower_json_shapes(c, NULL);
    lower_d_begin(c, &d, &b);
    lower_d_printf(&d, "static ek_val %s(ek_val v)\n{\n", name);
    if (sk == EKSC_INT || sk == EKSC_UINT) {
        emit_from_scalar(c, &d, sk, t);
    } else if (sk != EKSC_OTHER) {
        emit_from_simple(c, &d, sk);
    } else {
        emit_from_shape(c, &d, name, t);
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}
