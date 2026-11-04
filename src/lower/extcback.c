#include <string.h>

#include "lower/internal.h"

/* The callback adapters. Where the target takes a function pointer
   and a context, the closure marshals as the pair: the adapter is the
   target-callable function, the context carries the closure value,
   and the adapter's lifetime is the call. The context struct also
   holds one kept slot for a reference-shaped result, so the bytes the
   adapter hands back live until the C call returns. */

/* The registry entry of one callback parameter, keyed by the extern
   symbol and the parameter position; a failed shape registers once
   with its flag. */
static lower_cbr *cbr_entry(lower_ctx *c, const ek_symbol *esym,
                            size_t param)
{
    lower_cbr *slots;
    size_t i;

    for (i = 0; i < c->ncbr; i++) {
        if (c->cbrs[i].esym == esym && c->cbrs[i].param == param) {
            return &c->cbrs[i];
        }
    }
    slots = lower_grow(c, c->cbrs, &c->ccbr, c->ncbr + 1, sizeof *slots);
    if (!slots) {
        return NULL;
    }
    c->cbrs = slots;
    memset(&slots[c->ncbr], 0, sizeof slots[c->ncbr]);
    slots[c->ncbr].esym = esym;
    slots[c->ncbr].param = param;
    return &slots[c->ncbr++];
}

/* The value expression of one callback parameter; NULL for the String
   row, which copies fresh through statements. */
static const char *param_value(lower_ctx *c, const lower_mshape *s,
                               const char *a)
{
    switch (s->kind) {
    case MS_BOOL:
        return lower_str(c, "ek_bool(%s)", a);
    case MS_INT:
        return lower_str(c, "ek_val_int(%s)", a);
    case MS_UINT:
        return lower_str(c, "ek_val_uint(%s)", a);
    case MS_FLOAT:
        return lower_str(c, "ek_val_float(%s)", a);
    case MS_WORD:
        return lower_word_in(c, s->word, a);
    case MS_SIZE:
        return lower_str(c, "ek_val_uint((uint64_t)(%s))", a);
    case MS_PTR:
        return lower_str(c, "ek_ptr_wrap(%s)", a);
    case MS_CTYPE:
        return lower_ctype_unpack(c, s->ctype, a);
    default:
        return NULL;
    }
}

/* The back-conversion of the callback result: the reference-shaped
   rows park the value in the context's keep slot, a ctype packs into
   a local, and the scalars read their slot. */
static void emit_result_convert(lower_ctx *c, ek_buf *b, const char *ctxty,
                                const lower_mshape *res)
{
    switch (res->kind) {
    case MS_UNIT:
        if (!ek_buf_printf(b, "    ek_release(r);\n")) {
            c->oom = 1;
        }
        return;
    case MS_STRING:
    case MS_BYTES:
        if (!ek_buf_printf(b, "    ((%s *)ctx)->keep = ek_keep(r);\n"
                              "    %s out = %s;\n"
                              "    ek_release(r);\n"
                              "    return out;\n",
                           ctxty,
                           res->kind == MS_STRING ? "const char *"
                                                  : "const void *",
                           res->kind == MS_STRING
                               ? "ek_str_cstr(r.as.ref)"
                               : "ek_bytes_data(r.as.ref)")) {
            c->oom = 1;
        }
        return;
    case MS_CTYPE:
        if (!ek_buf_printf(b, "    ekC_%s$%s out = %s;\n"
                              "    ek_release(r);\n"
                              "    return out;\n",
                           res->ctype->module, res->ctype->name,
                           lower_ctype_pack_expr(c, res->ctype, "r"))) {
            c->oom = 1;
        }
        return;
    default:
        if (!ek_buf_printf(b, "    %s out = %s;\n"
                              "    ek_release(r);\n"
                              "    return out;\n",
                           lower_cbr_ret_type(c, res),
                           lower_scalar_out_expr(c, res, "r"))) {
            c->oom = 1;
        }
        return;
    }
}

/* The adapter's parameter declarations and conversions: the String
   row copies fresh with its UTF-8 claim checked, the rest are
   expressions. */
static void emit_cb_params(lower_ctx *c, ek_buf *b, const ek_nlist *cps)
{
    size_t j;

    for (j = 0; j < cps->count; j++) {
        lower_mshape s = lower_ms_of(c, cps->data[j]->u.t_field.type);

        if (s.kind == MS_STRING) {
            if (!ek_buf_printf(b, "    ek_obj *s%zu = "
                                  "ek_str_from_cstr(a%zu);\n"
                                  "    if (!ek_utf8_valid("
                                  "ek_str_cstr(s%zu), "
                                  "ek_str_len(s%zu))) {\n"
                                  "        ek_panic(\"P-panic\", "
                                  "\"a String result is not UTF-8\");\n"
                                  "    }\n"
                                  "    ek_val v%zu = "
                                  "ek_val_ref(s%zu);\n",
                               j, j, j, j, j, j)) {
                c->oom = 1;
            }
            continue;
        }
        if (!ek_buf_printf(b, "    ek_val v%zu = %s;\n", j,
                           param_value(c, &s, lower_str(c, "a%zu", j)))) {
            c->oom = 1;
        }
    }
}

/* One adapter definition: the parameters convert to the uniform ABI's
   argument array, the context's closure runs, and the result converts
   back. */
static void emit_adapter(lower_ctx *c, const char *name, const char *ctxty,
                         const ek_node *cb, const lower_mshape *res)
{
    const ek_nlist *cps = &cb->u.t_fn.params;
    lower_pending *p;
    ek_buf b;
    size_t j;

    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "static %s %s(", lower_cbr_ret_type(c, res),
                       name)) {
        c->oom = 1;
    }
    for (j = 0; j < cps->count; j++) {
        lower_mshape s = lower_ms_of(c, cps->data[j]->u.t_field.type);

        if (!ek_buf_printf(&b, "%s%s a%zu", j ? ", " : "",
                           lower_cbr_param_type(c, &s), j)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_printf(&b, "%svoid *ctx)\n{\n", cps->count ? ", " : "")) {
        c->oom = 1;
    }
    emit_cb_params(c, &b, cps);
    if (cps->count) {
        if (!ek_buf_printf(&b, "    ek_val args[%zu] = {", cps->count)) {
            c->oom = 1;
        }
        for (j = 0; j < cps->count; j++) {
            if (!ek_buf_printf(&b, "%sv%zu", j ? ", " : "", j)) {
                c->oom = 1;
            }
        }
        if (!ek_buf_printf(&b, "};\n")) {
            c->oom = 1;
        }
    }
    if (!ek_buf_printf(&b, "    ek_val r = ek_closure_call(((%s *)ctx)"
                           "->clo, %s, %zu);\n",
                       ctxty, cps->count ? "args" : "(ek_val *)0",
                       cps->count)) {
        c->oom = 1;
    }
    emit_result_convert(c, &b, ctxty, res);
    if (!ek_buf_printf(&b, "}\n\n")) {
        c->oom = 1;
    }
    p = ek_arena_alloc(c->arena, sizeof *p);
    if (!p) {
        c->oom = 1;
        ek_buf_destroy(&b);
        return;
    }
    memset(p, 0, sizeof *p);
    p->text = lower_take(c, &b);
    lower_pending_add(c, p);
}

/* The callback arrow's shapes over the whitelist's two directions;
   0 with the diagnostic placed when some shape has none. */
static int cbr_shapes_ok(lower_ctx *c, const ek_symbol *esym,
                         const ek_node *cb, lower_cbr *ent,
                         lower_mshape *res)
{
    size_t j;
    int bad = 0;

    for (j = 0; j < cb->u.t_fn.params.count; j++) {
        lower_mshape s = lower_ms_of(c,
                                     cb->u.t_fn.params.data[j]
                                         ->u.t_field.type);

        if (!lower_cbr_param_type(c, &s)) {
            bad = 1;
        }
    }
    *res = lower_ms_of(c, cb->u.t_fn.result);
    if (!lower_cbr_ret_type(c, res)) {
        bad = 1;
    }
    if (bad) {
        ent->bad = 1;
        lower_error(c, esym->decl, "the callback of `%s` has no lowering "
                                   "at the boundary", esym->name);
        return 0;
    }
    return 1;
}

/* The context struct's typedef beside the preamble, so every call
   site sees the type. */
static void emit_ctx_typedef(lower_ctx *c, const char *cty)
{
    ek_buf b;

    ek_buf_init(&b);
    if (!ek_buf_printf(&b, "typedef struct { ek_val clo; ek_val keep; "
                           "} %s;\n\n", cty)
        || (b.data && !ek_buf_append_bytes(&c->cext, b.data, b.len))) {
        c->oom = 1;
    }
    ek_buf_destroy(&b);
}

/* The adapter's forward declaration, shared with the other emitted
   functions. */
static void push_adapter_sig(lower_ctx *c, const char *name,
                             const ek_node *cb, const lower_mshape *res)
{
    const ek_nlist *cps = &cb->u.t_fn.params;
    ek_buf sigb;
    size_t j;

    ek_buf_init(&sigb);
    if (!ek_buf_printf(&sigb, "static %s %s(", lower_cbr_ret_type(c, res),
                       name)) {
        c->oom = 1;
    }
    for (j = 0; j < cps->count; j++) {
        lower_mshape s = lower_ms_of(c, cps->data[j]->u.t_field.type);

        if (!ek_buf_printf(&sigb, "%s%s a%zu", j ? ", " : "",
                           lower_cbr_param_type(c, &s), j)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_printf(&sigb, "%svoid *ctx)",
                       cps->count ? ", " : "")) {
        c->oom = 1;
    }
    lower_push_sig(c, lower_take(c, &sigb));
}

int lower_cbr_ensure(lower_ctx *c, const ek_symbol *esym, size_t param,
                     const char **fn, const char **ctxty)
{
    const ek_node *ann = esym->type;
    const ek_node *cb;
    lower_mshape resbuf;
    lower_cbr *ent;
    const char *name, *cty;

    ent = cbr_entry(c, esym, param);
    if (!ent) {
        return 0;
    }
    if (ent->bad) {
        return 0; /* diagnosed at its first use */
    }
    if (ent->fn) {
        *fn = ent->fn;
        *ctxty = ent->ctxty;
        return 1;
    }
    if (!ann || ann->kind != EK_N_T_FN
        || param >= ann->u.t_fn.params.count) {
        return 0;
    }
    cb = ann->u.t_fn.params.data[param]->u.t_field.type;
    if (!cb || cb->kind != EK_N_T_FN) {
        return 0;
    }
    if (!cbr_shapes_ok(c, esym, cb, ent, &resbuf)) {
        return 0;
    }
    name = lower_str(c, "eko_cbr_%u", c->cbr);
    cty = lower_str(c, "eko_cb_%u", c->cbr);
    c->cbr++;
    ent->fn = name;
    ent->ctxty = cty;
    emit_ctx_typedef(c, cty);
    push_adapter_sig(c, name, cb, &resbuf);
    emit_adapter(c, name, cty, cb, &resbuf);
    *fn = name;
    *ctxty = cty;
    return 1;
}
