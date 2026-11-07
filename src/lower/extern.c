#include <string.h>

#include "lower/internal.h"

/* The native call path. A reachable extern binding emits nothing
   itself: the declaration's prototype joins the unit at its first
   call site, every call site marshals each argument per the
   whitelist, makes the one C call, and marshals the result back. The
   world bit of a declaration is compile-time only and lowers the same
   either way. */

/* The call's arguments evaluated and dying: the balance of a shape or
   arity the boundary refuses. */
static lower_val discard_extern(lower_ctx *c, lower_scope *scope,
                                const ek_node *e)
{
    const ek_nlist *args = &e->u.call.args;
    size_t i;

    for (i = 0; i < args->count; i++) {
        lower_val a = lower_sub(c, scope, args->data[i]);
        const char *t = lower_as_temp(c, scope, &a);

        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
    return lower_val_unit();
}

/* Releases one call's temps, newest first: a callback context dies
   with its closure and kept slot, an argument temp with its value. */
static void release_call_temps(lower_ctx *c, const char **argv,
                               const char **ctxv,
                               const unsigned char *moved, size_t n)
{
    size_t i = n;

    while (i > 0) {
        i--;
        if (ctxv[i]) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s.keep);\n", ctxv[i]);
            lower_pad(c);
            lower_emitf(c, "ek_release(%s.clo);\n", ctxv[i]);
        } else if (!moved[i]) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", argv[i]);
        }
    }
}

/* The fresh String of a C string result: the bytes must be valid
   UTF-8, and a violation panics. */
static void emit_string_result(lower_ctx *c, const char *out,
                               const char *call)
{
    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = ek_str_from_cstr(%s);\n", out, call);
    lower_pad(c);
    lower_emitf(c, "if (!ek_utf8_valid(ek_str_cstr(%s), ek_str_len(%s))) "
                   "{\n", out, out);
    c->depth++;
    lower_pad(c);
    lower_emitf(c, "ek_panic(\"P-panic\", \"a String result is not "
                   "UTF-8\");\n");
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
}

/* The copy-in list: the caller's list copies into a C element array,
   valid for the call alone; the walk reads one element per spine
   cell. */
static int marshal_cin_list(lower_ctx *c, lower_scope *scope, ek_buf *b,
                            const ek_node *e, const ek_symbol *sym,
                            const lower_mshape *s, const char *arg, size_t i)
{
    const char *ct = lower_word_ctype(s->word);
    const char *out = lower_scalar_out_expr(
        c, &(lower_mshape){ MS_WORD, s->word, NULL, NULL },
        "ek_cons_head(li.as.ref)");
    const char *arr = lower_temp(c, scope);
    const char *n = lower_temp(c, scope);

    if (!out) {
        lower_error(c, e, "the argument `%zu` of `%s` has no lowering "
                          "at the boundary", i, sym->name);
        return 0;
    }
    lower_pad(c);
    lower_emitf(c, "size_t %s = ek_list_len(%s);\n", n, arg);
    lower_pad(c);
    lower_emitf(c, "%s *%s = ek_cin_array(%s, sizeof(%s));\n", ct, arr, n,
                ct);
    lower_pad(c);
    lower_emitf(c, "{\n");
    lower_emitf(c, "    ek_val li = %s;\n", arg);
    lower_emitf(c, "    size_t j = 0;\n");
    lower_emitf(c, "    while (ek_val_is_cons(li)) {\n");
    lower_emitf(c, "        %s[j++] = %s;\n", arr, out);
    lower_emitf(c, "        li = ek_cons_tail(li.as.ref);\n");
    lower_emitf(c, "    }\n");
    lower_emitf(c, "}\n");
    return ek_buf_append_str(b, arr);
}

/* The callback: the context struct minted once, the closure moving
   into it, and the adapter and context traveling as the pair. */
static int marshal_closure(lower_ctx *c, lower_scope *scope, ek_buf *b,
                           const ek_symbol *sym, const char *arg, size_t i,
                           const char **ctx)
{
    const char *fn, *ctxty;
    const char *cb = lower_temp(c, scope);

    if (!lower_cbr_ensure(c, sym, i, &fn, &ctxty)) {
        return 0;
    }
    lower_pad(c);
    lower_emitf(c, "%s %s;\n", ctxty, cb);
    lower_pad(c);
    lower_emitf(c, "%s.clo = %s;\n", cb, arg);
    lower_pad(c);
    lower_emitf(c, "%s.keep = ek_val_unit();\n", cb);
    *ctx = cb;
    return ek_buf_printf(b, "%s, (void *)&%s", fn, cb);
}

/* The C argument text of one marshalled parameter; 0 when the shape
   has no argument lowering. The closure's temp moves into its
   context. */
static int marshal_arg(lower_ctx *c, lower_scope *scope, ek_buf *b,
                       const ek_node *e, const ek_symbol *sym,
                       const ek_node *tn, const char *arg, size_t i,
                       const char **ctx)
{
    lower_mshape s = lower_ms_of(c, tn);
    const char *expr = lower_scalar_out_expr(c, &s, arg);

    if (i && !ek_buf_append_str(b, ", ")) {
        c->oom = 1;
    }
    if (expr) {
        return ek_buf_append_str(b, expr);
    }
    if (s.kind == MS_CTYPE || s.kind == MS_CIN) {
        const char *cs = lower_temp(c, scope);
        const char *packed = lower_ctype_pack_expr(c, s.ctype, arg);

        lower_pad(c);
        lower_emitf(c, "ekC_%s$%s %s = %s;\n", s.ctype->module,
                    s.ctype->name, cs, packed ? packed : "{ 0 }");
        return ek_buf_append_str(b, s.kind == MS_CIN
                                       ? lower_str(c, "&%s", cs) : cs);
    }
    if (s.kind == MS_CIN_LIST) {
        return marshal_cin_list(c, scope, b, e, sym, &s, arg, i);
    }
    if (s.kind == MS_CLOSURE) {
        return marshal_closure(c, scope, b, sym, arg, i, ctx);
    }
    lower_error(c, e, "the argument `%zu` of `%s` has no lowering at "
                      "the boundary", i, sym->name);
    return 0;
}

/* The C argument list over the marshalled parameters; a refusing
   shape releases what it minted and answers 0. */
static int marshal_args(lower_ctx *c, lower_scope *scope, ek_buf *b,
                        const ek_node *e, const ek_symbol *sym,
                        const ek_node *ann, const char **argv,
                        const char **ctxv, unsigned char *moved, size_t n)
{
    size_t i;

    ek_buf_init(b);
    for (i = 0; i < n; i++) {
        if (!marshal_arg(c, scope, b, e, sym,
                         ann->u.t_fn.params.data[i]->u.t_field.type,
                         argv[i], i, &ctxv[i])) {
            release_call_temps(c, argv, ctxv, moved, i + 1);
            return 0;
        }
        if (ctxv[i]) {
            moved[i] = 1; /* the closure moves into its context */
        }
    }
    return 1;
}

/* The result marshalling: the unit runs bare, a String copies fresh
   with its UTF-8 claim checked, a ctype rebuilds through the helper,
   the scalars read their slots. */
static lower_val emit_extern_result(lower_ctx *c, lower_scope *scope,
                                    const ek_node *e, const ek_symbol *sym,
                                    const lower_mshape *res,
                                    const char *call)
{
    const char *t;

    if (res->kind == MS_UNIT) {
        lower_pad(c);
        lower_emitf(c, "%s;\n", call);
        return lower_val_unit();
    }
    if (res->kind == MS_STRING) {
        const char *s = lower_temp(c, scope);

        emit_string_result(c, s, call);
        t = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", t, s);
        return (lower_val){ t, 1 };
    }
    if (res->kind == MS_CTYPE) {
        const char *cs = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ekC_%s$%s %s = %s;\n", res->ctype->module,
                    res->ctype->name, cs, call);
        t = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", t,
                    lower_ctype_unpack(c, res->ctype, cs));
        return (lower_val){ t, 1 };
    }
    {
        const char *expr = lower_scalar_in_expr(c, res, call);

        if (!expr) {
            lower_error(c, e, "the result of `%s` has no lowering at the "
                              "boundary", sym->name);
            expr = "ek_val_unit()";
        }
        t = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", t, expr);
        return (lower_val){ t, 1 };
    }
}

/* The written arguments as the call's owned temps, in source order. */
static void eval_args(lower_ctx *c, lower_scope *scope, const ek_node *e,
                      size_t n, const char **argv, const char **ctxv,
                      unsigned char *moved)
{
    const ek_nlist *args = &e->u.call.args;
    size_t i;

    memset(moved, 0, n);
    for (i = 0; i < n; i++) {
        lower_val a = lower_sub(c, scope, args->data[i]);

        argv[i] = lower_as_temp(c, scope, &a);
        ctxv[i] = NULL;
    }
}

/* One call to an extern declaration: the argument temps evaluate in
   source order, the struct and callback shapes marshal into C
   locals, the one C call runs, and the result marshals back before
   the argument temps die. */
lower_val lower_extern_call(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const ek_symbol *sym,
                            const char *extra)
{
    const ek_node *ann = sym->type;
    lower_ext *ent = extern_entry(c, sym);
    const char **argv = NULL;
    const char **ctxv = NULL;
    unsigned char *moved = NULL;
    size_t acap = 0, xcap = 0, mcap = 0, n;
    ek_buf b;
    lower_mshape res;
    const char *call;
    lower_val out;

    if (!ent || !ann || ann->kind != EK_N_T_FN) {
        return lower_val_unit();
    }
    n = ann->u.t_fn.params.count;
    if (extra || e->u.call.args.count != n) {
        lower_error(c, e, "the call of `%s` does not take the chain form",
                    sym->name);
        if (extra) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", extra);
        }
        return discard_extern(c, scope, e);
    }
    if (ent->bad) {
        return discard_extern(c, scope, e);
    }
    argv = lower_grow(c, NULL, &acap, n ? n : 1, sizeof *argv);
    ctxv = lower_grow(c, NULL, &xcap, n ? n : 1, sizeof *ctxv);
    moved = lower_grow(c, NULL, &mcap, n ? n : 1, sizeof *moved);
    if (!argv || !ctxv || !moved) {
        return lower_val_unit();
    }
    eval_args(c, scope, e, n, argv, ctxv, moved);
    if (!marshal_args(c, scope, &b, e, sym, ann, argv, ctxv, moved, n)) {
        return lower_val_unit();
    }
    call = lower_str(c, "%s(%s)", sym->extern_symbol,
                     b.len ? b.data : "");
    ek_buf_destroy(&b);
    res = lower_ms_of(c, ann->u.t_fn.result);
    out = emit_extern_result(c, scope, e, sym, &res, call);
    release_call_temps(c, argv, ctxv, moved, n);
    return out;
}
