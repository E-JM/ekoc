#include <string.h>

#include "lower/internal.h"

/* The structural hash service. The hash mirrors the equality
   service's shape walk: the family identity (the typeinfo pointer)
   mixes in first, then each field's tag and payload -- integers by
   value, floats by their bits with negative zero read as plain zero
   and a NaN by its own bits, and a reference through the child
   typeinfo's hash or, when the child carries none, the child's
   address. Lists walk the spine and dictionaries and sets accumulate
   their entries order-insensitively, the way their equalities ignore
   order. One FNV-1a word step is the mixing primitive, and the
   determinism it needs holds within one process run. */

/* The mixing step every generated hash shares, emitted once per unit
   ahead of the first body that calls it. */
static void emit_mix_helper(lower_ctx *c, lower_demit *d)
{
    if (c->hash_mix) {
        return;
    }
    c->hash_mix = 1;
    /* a service minted mid-body can land ahead of the first body, so
       the definition travels with a declaration ahead of them all */
    lower_push_sig(c, "static unsigned long ek_hash_mix(unsigned long h, "
                      "unsigned long x);");
    lower_d_printf(d, "static unsigned long ek_hash_mix(unsigned long h, "
                      "unsigned long x)\n{\n");
    lower_d_printf(d, "    h ^= x;\n");
    lower_d_printf(d, "    return h * 1099511628211ULL;\n");
    lower_d_printf(d, "}\n\n");
}

/* The float payload mixes by its bits, negative zero read as plain
   zero and a NaN by its own bits. */
static void emit_float_mix(lower_ctx *c, lower_demit *d, const char *slot)
{
    c->uses_string_h = 1;
    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        double x = %s.as.f;\n", slot);
    lower_d_printf(d, "        unsigned long b;\n\n");
    lower_d_printf(d, "        if (x == 0.0) {\n");
    lower_d_printf(d, "            x = 0.0;\n");
    lower_d_printf(d, "        }\n");
    lower_d_printf(d, "        memcpy(&b, &x, sizeof b);\n");
    lower_d_printf(d, "        h = ek_hash_mix(h, b);\n");
    lower_d_printf(d, "    }\n");
}

/* The text families hash their own bytes: no service exists to call,
   and the runtime's key hash belongs to its table. */
static void emit_text_mix(lower_demit *d, lower_scalar sk, const char *slot)
{
    int is_str = sk == EKSC_STRING;

    lower_d_printf(d, "    {\n");
    lower_d_printf(d, "        const unsigned char *ekp = (const "
                      "unsigned char *)%s(%s.as.ref);\n",
                   is_str ? "ek_str_cstr" : "ek_bytes_data", slot);
    lower_d_printf(d, "        size_t ekn = %s(%s.as.ref);\n",
                   is_str ? "ek_str_len" : "ek_bytes_len", slot);
    lower_d_printf(d, "        size_t eki;\n\n");
    lower_d_printf(d, "        for (eki = 0; eki < ekn; eki++)\n");
    lower_d_printf(d, "            h = ek_hash_mix(h, "
                      "(unsigned long)ekp[eki]);\n");
    lower_d_printf(d, "    }\n");
}

/* A reference whose family carries no hash of its own keys on its
   address, the identity fallback the key domains without per-value
   families read. */
static void emit_ref_mix(lower_ctx *c, lower_demit *d, const ek_type *t,
                         const char *slot)
{
    const char *svc = t ? lower_dsvc_request(c, NULL, DSV_HASH, t) : NULL;

    if (svc) {
        lower_d_printf(d, "    h = ek_hash_mix(h, %s(%s));\n", svc, slot);
        return;
    }
    lower_d_printf(d, "    if (%s.tag == EK_VAL_REF && %s.as.ref"
                      " != NULL) {\n", slot, slot);
    lower_d_printf(d, "        h = ek_hash_mix(h, "
                      "(unsigned long)%s.as.ref);\n", slot);
    lower_d_printf(d, "    } else {\n");
    lower_d_printf(d, "        h = ek_hash_mix(h, "
                      "(unsigned long)%s.as.u);\n", slot);
    lower_d_printf(d, "    }\n");
}

/* The mixing statements over one slot read through the given
   expression: the tag first, then the payload in the slot's own form. */
static void emit_slot_mix(lower_ctx *c, lower_demit *d, const ek_type *t,
                          const char *slot)
{
    lower_scalar sk = lower_scalar_kind(t);

    lower_d_printf(d, "    h = ek_hash_mix(h, (unsigned long)%s.tag);\n",
                   slot);
    switch (sk) {
    case EKSC_INT:
    case EKSC_BOOL:
        lower_d_printf(d, "    h = ek_hash_mix(h, (unsigned long)%s.as.i);\n",
                       slot);
        return;
    case EKSC_UINT:
        lower_d_printf(d, "    h = ek_hash_mix(h, (unsigned long)%s.as.u);\n",
                       slot);
        return;
    case EKSC_CHAR:
        lower_d_printf(d, "    h = ek_hash_mix(h, (unsigned long)%s.as.c);\n",
                       slot);
        return;
    case EKSC_FLOAT:
        emit_float_mix(c, d, slot);
        return;
    case EKSC_STRING:
    case EKSC_BYTES:
        emit_text_mix(d, sk, slot);
        return;
    default:
        emit_ref_mix(c, d, t, slot);
        return;
    }
}

/* The family identity: the typeinfo pointer separates the
   constructors before any field mixes. */
static void emit_identity_mix(lower_demit *d)
{
    lower_d_printf(d, "    h = ek_hash_mix(h, (unsigned long)"
                      "v.as.ref->info);\n");
}

/* Field-wise mixing over a record or tuple layout. */
static void emit_fields_hash(lower_ctx *c, lower_demit *d, const char *stem,
                             const char **fnames, const ek_type **ftypes,
                             size_t nf)
{
    size_t i;

    for (i = 0; i < nf; i++) {
        const char *slot = fnames ? lower_str(c, "f_%s", fnames[i])
                                  : lower_str(c, "f_%zu", i);

        emit_slot_mix(c, d, ftypes[i],
                      lower_str(c, "((ekS_%s *)v.as.ref)->%s", stem, slot));
    }
    lower_d_printf(d, "    return h;\n");
}

/* The constructor dispatch: the payload mixes within the constructor
   the identity already chose. */
static void emit_adt_hash(lower_ctx *c, lower_demit *d, lower_variant *vs,
                          size_t nv)
{
    size_t i;

    for (i = 0; i < nv; i++) {
        lower_d_printf(d, "    if (v.as.ref->info == &ekT_%s) {\n",
                       vs[i].stem);
        if (vs[i].payload) {
            emit_slot_mix(c, d, vs[i].payload,
                          lower_str(c, "((ekS_%s *)v.as.ref)->p0",
                                    vs[i].stem));
        }
        lower_d_printf(d, "        return h;\n");
        lower_d_printf(d, "    }\n");
    }
    lower_d_printf(d, "    return h;\n");
}

/* The range's three slots: the endpoints in their element form and the
   closed flag last. */
static void emit_range_hash(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    lower_gtype *g = elem ? lower_range_family(c, lower_scalar_kind(elem))
                          : NULL;
    static const char *slots[3] = { "f_start", "f_end", "f_closed" };
    size_t i;

    if (!g) {
        lower_d_printf(d, "    return h;\n");
        return;
    }
    for (i = 0; i < 3; i++) {
        emit_slot_mix(c, d, i < 2 ? elem : NULL,
                      lower_str(c, "((ekS_%s *)v.as.ref)->%s", g->stem,
                                slots[i]));
    }
    lower_d_printf(d, "    return h;\n");
}

/* Set mixing is order-insensitive: the element hashes add, so two sets
   built in different orders mix the same word, matching the set
   equality that ignores order. */
static void emit_set_hash(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const ek_type *elem = t->u.nominal.argc == 1 ? t->u.nominal.args[0]
                                                 : NULL;
    const char *eh = elem ? lower_dsvc_request(c, NULL, DSV_HASH, elem)
                          : NULL;

    if (!eh) {
        lower_d_printf(d, "    return h;\n");
        return;
    }
    lower_d_printf(d, "    ek_val list = ek_set_to_list(v);\n");
    lower_d_printf(d, "    ek_val p = list;\n\n");
    lower_d_printf(d, "    while (ek_val_is_cons(p)) {\n");
    lower_d_printf(d, "        h += %s(ek_cons_head(p.as.ref));\n", eh);
    lower_d_printf(d, "        p = ek_cons_tail(p.as.ref);\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    ek_release(list);\n");
    lower_d_printf(d, "    return h;\n");
}

/* Element-wise mixing over a list, walking the spine. */
static void emit_list_hash(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *elem = lower_dsvc_request(c, NULL, DSV_HASH, t->u.list.elem);

    if (!elem) {
        lower_d_printf(d, "    return h;\n");
        return;
    }
    lower_d_printf(d, "    while (ek_val_is_cons(v)) {\n");
    lower_d_printf(d, "        h = ek_hash_mix(h, %s(ek_cons_head"
                      "(v.as.ref)));\n", elem);
    lower_d_printf(d, "        v = ek_cons_tail(v.as.ref);\n");
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    return h;\n");
}

/* Entry mixing over a dictionary: each key's and each value's hash
   adds, so two dictionaries built in different orders mix the same
   word, matching the dictionary equality that ignores order. */
static void emit_dict_hash(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    const char *kh = lower_dsvc_request(c, NULL, DSV_HASH, t->u.dict.key);
    const char *vh = lower_dsvc_request(c, NULL, DSV_HASH, t->u.dict.val);

    if (!kh || !vh) {
        lower_d_printf(d, "    return h;\n");
        return;
    }
    lower_d_printf(d, "    ek_val k, x;\n");
    lower_d_printf(d, "    ek_dict_iter it;\n\n");
    lower_d_printf(d, "    ek_dict_iter_begin(&it, v);\n");
    lower_d_printf(d, "    while (ek_dict_iter_next(&it, &k, &x)) {\n");
    lower_d_printf(d, "        h += %s(k);\n", kh);
    lower_d_printf(d, "        h += %s(x);\n", vh);
    lower_d_printf(d, "    }\n");
    lower_d_printf(d, "    ek_dict_iter_end(&it);\n");
    lower_d_printf(d, "    return h;\n");
}

/* The nominal receiver: the algebraic dispatch, or the representation
   walk over the family's fields; a nominal with neither mixes the
   identity fallback. */
static void emit_nominal_hash(lower_ctx *c, lower_demit *d, const ek_type *t)
{
    lower_variant *vs = NULL;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    size_t nv = 0, nf;

    if (lower_variants_of(c, NULL, t, &vs, &nv, 0)
        || lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g)) {
        emit_identity_mix(d);
        if (vs) {
            emit_adt_hash(c, d, vs, nv);
        } else if (g && g->shape == GT_REC) {
            emit_fields_hash(c, d, g->stem, fnames, ftypes, nf);
        } else if (g) {
            emit_fields_hash(c, d, g->stem, NULL, ftypes, nf);
        } else {
            lower_d_printf(d, "    return h;\n");
        }
        return;
    }
    emit_slot_mix(c, d, NULL, "v");
    lower_d_printf(d, "    return h;\n");
}

void lower_dsvc_hash_emit(lower_ctx *c, const char *name, const ek_type *t)
{
    lower_demit d;
    ek_buf b;
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    size_t nf;

    lower_d_begin(c, &d, &b);
    emit_mix_helper(c, &d);
    lower_d_printf(&d, "static unsigned long %s(ek_val v)\n{\n", name);
    lower_d_printf(&d, "    unsigned long h = 14695981039346656037ULL;\n\n");
    if (lower_is_range(t)) {
        emit_identity_mix(&d);
        emit_range_hash(c, &d, t);
    } else if (lower_is_set(t)) {
        emit_identity_mix(&d);
        emit_set_hash(c, &d, t);
    } else if (lower_is_vector(t)) {
        emit_identity_mix(&d);
        lower_vec_hash_emit(c, &d, t);
    } else if (t->kind == EK_T_UNIT) {
        lower_d_printf(&d, "    return h;\n");
    } else if (t->kind == EK_T_LIST) {
        emit_list_hash(c, &d, t);
    } else if (t->kind == EK_T_DICT) {
        emit_identity_mix(&d);
        emit_dict_hash(c, &d, t);
    } else if (lower_scalar_kind(t) != EKSC_OTHER) {
        emit_slot_mix(c, &d, t, "v");
        lower_d_printf(&d, "    return h;\n");
    } else if (t->kind == EK_T_TUPLE || t->kind == EK_T_RECORD) {
        if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
            emit_identity_mix(&d);
            emit_fields_hash(c, &d, g->stem, fnames, ftypes, nf);
        } else {
            emit_slot_mix(c, &d, NULL, "v");
            lower_d_printf(&d, "    return h;\n");
        }
    } else if (t->kind == EK_T_NOMINAL) {
        emit_nominal_hash(c, &d, t);
    } else {
        emit_slot_mix(c, &d, NULL, "v");
        lower_d_printf(&d, "    return h;\n");
    }
    lower_d_printf(&d, "}\n\n");
    lower_d_end(c, &b);
}
