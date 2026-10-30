#include <string.h>

#include "lower/internal.h"

/* The std constructor wrapping of the runtime's status+out contracts:
   a hit becomes Some or Ok through the std source constructors, a
   miss becomes None or Err the ParseError of the kind. The
   constructors are std source, so their families register exactly as
   composite construction registers them and join the unit before the
   functions. Every emitter builds one owned result temp; the temps
   the call read die with the caller's release pass. */

/* The C type and neutral initializer of one out slot. */
static const char *out_type(out_kind out, const char **init)
{
    switch (out) {
    case OUT_U64: *init = "0";   return "uint64_t";
    case OUT_F64: *init = "0";   return "double";
    case OUT_U32: *init = "0";   return "uint32_t";
    case OUT_VAL: *init = "ek_val_unit()"; return "ek_val";
    default:      *init = "0";   return "int64_t";
    }
}

/* The payload expression an out slot wraps into the std domain. */
static const char *payload_of(lower_ctx *c, out_kind out, const char *o)
{
    switch (out) {
    case OUT_U64: return lower_str(c, "ek_val_uint(%s)", o);
    case OUT_F64: return lower_str(c, "ek_val_float(%s)", o);
    case OUT_U32: return lower_str(c, "ek_val_char(%s)", o);
    case OUT_VAL: return lower_str(c, "ek_keep(%s)", o);
    default:      return lower_str(c, "ek_val_int(%s)", o);
    }
}

const char *lower_ctor_helper(lower_ctx *c, const ek_node *at,
                              const char *module, const char *owner,
                              const char *name, int *nf)
{
    size_t k, i;

    *nf = 0;
    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, owner) == 0
                && strcmp(m->name, name) == 0) {
                lower_gtype *g = lower_ctor_of_symbol(c, at, m, 1);

                if (!g) {
                    return NULL;
                }
                *nf = (int)g->nf;
                return g->helper;
            }
        }
    }
    lower_error(c, at, "the constructor `%s.%s` is not supported by "
                      "this lowerer", owner, name);
    return NULL;
}

const char *lower_wrap_helper(lower_ctx *c, const ek_node *at,
                              const char *owner, const char *name, int *nf)
{
    return lower_ctor_helper(c, at, "std", owner, name, nf);
}

/* A status word and an out slot: the hit wraps the payload, the miss
   is the bare None. */
lower_val lower_wrap_option(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const char *sym,
                            const char *args, out_kind out)
{
    unsigned d = c->depth;
    const char *t = lower_temp(c, scope);
    const char *o = lower_temp(c, scope);
    const char *p = lower_temp(c, scope);
    const char *init;
    const char *oty = out_type(out, &init);
    int nsome = 0, nnone = 0;
    const char *some = lower_wrap_helper(c, e, "Option", "Some", &nsome);
    const char *none = lower_wrap_helper(c, e, "Option", "None", &nnone);

    if (!some || !none) {
        return lower_val_unit();
    }
    lower_pad(c);
    lower_emitf(c, "%s %s = %s;\n", oty, o, init);
    lower_pad(c);
    lower_emitf(c, "ek_val %s;\n", t);
    lower_pad(c);
    lower_emitf(c, "if (%s(%s, &%s)) {\n", sym, args, o);
    c->depth = d + 1;
    if (out == OUT_VAL) {
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", p, o);
        lower_pad(c);
        lower_emitf(c, "%s = %s(%s);\n", t, some, p);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", p);
    } else {
        lower_pad(c);
        lower_emitf(c, "%s = %s(%s);\n", t, some, payload_of(c, out, o));
    }
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "%s = %s();\n", t, none);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
    return (lower_val){ t, 1 };
}

/* An owned object or NULL: the block bridges report their miss as
   NULL and their hit as a fresh object. */
lower_val lower_wrap_option_obj(lower_ctx *c, lower_scope *scope,
                                const ek_node *e, const char *sym,
                                const char *args)
{
    unsigned d = c->depth;
    const char *t = lower_temp(c, scope);
    const char *o = lower_temp(c, scope);
    const char *p = lower_temp(c, scope);
    int nsome = 0, nnone = 0;
    const char *some = lower_wrap_helper(c, e, "Option", "Some", &nsome);
    const char *none = lower_wrap_helper(c, e, "Option", "None", &nnone);

    if (!some || !none) {
        return lower_val_unit();
    }
    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = %s(%s);\n", o, sym, args);
    lower_pad(c);
    lower_emitf(c, "ek_val %s;\n", t);
    lower_pad(c);
    lower_emitf(c, "if (%s != (ek_obj *)0) {\n", o);
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", p, o);
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, some, p);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", p);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "%s = %s();\n", t, none);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
    return (lower_val){ t, 1 };
}

/* The miss arms of the parse wrap: each kind becomes the ParseError
   the std source closes over, with the detail the runtime surface
   leaves empty. */
static void emit_parse_miss(lower_ctx *c, lower_scope *scope, unsigned d,
                            const char *t, const char *s, const char *err,
                            const char *empty, const char *inv,
                            const char *range)
{
    lower_pad(c);
    lower_emitf(c, "} else if (%s == EK_PARSE_EMPTY) {\n", s);
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s());\n", t, err, empty);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else if (%s == EK_PARSE_INVALID) {\n", s);
    c->depth = d + 1;
    {
        const char *p = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s(ek_val_ref(ek_str_from_cstr(\"\"))"
                       ");\n", p, inv);
        lower_pad(c);
        lower_emitf(c, "%s = %s(%s);\n", t, err, p);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", p);
    }
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s());\n", t, err, range);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
}

/* A parse status and an out slot: the kind becomes the ParseError the
   std source closes over, with the detail the runtime surface leaves
   empty. */
lower_val lower_wrap_parse(lower_ctx *c, lower_scope *scope,
                           const ek_node *e, const char *sym,
                           const char *args, out_kind out, int radix)
{
    unsigned d = c->depth;
    const char *t = lower_temp(c, scope);
    const char *o = lower_temp(c, scope);
    const char *s = lower_temp(c, scope);
    const char *init;
    const char *oty = out_type(out, &init);
    int nok = 0, nerr = 0, nempty = 0, ninv = 0, nrange = 0;
    const char *ok = lower_wrap_helper(c, e, "Result", "Ok", &nok);
    const char *err = lower_wrap_helper(c, e, "Result", "Err", &nerr);
    const char *empty = lower_wrap_helper(c, e, "ParseError", "Empty",
                                          &nempty);
    const char *inv = lower_wrap_helper(c, e, "ParseError", "Invalid",
                                        &ninv);
    const char *range = lower_wrap_helper(c, e, "ParseError", "OutOfRange",
                                          &nrange);

    if (!ok || !err || !empty || !inv || !range) {
        return lower_val_unit();
    }
    lower_pad(c);
    lower_emitf(c, "%s %s = %s;\n", oty, o, init);
    lower_pad(c);
    if (radix) {
        lower_emitf(c, "ek_parse_status %s = %s(%s, %d, &%s);\n", s, sym,
                    args, radix, o);
    } else {
        lower_emitf(c, "ek_parse_status %s = %s(%s, &%s);\n", s, sym,
                    args, o);
    }
    lower_pad(c);
    lower_emitf(c, "ek_val %s;\n", t);
    lower_pad(c);
    lower_emitf(c, "if (%s == EK_PARSE_OK) {\n", s);
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, ok, payload_of(c, out, o));
    c->depth = d;
    emit_parse_miss(c, scope, d, t, s, err, empty, inv, range);
    return (lower_val){ t, 1 };
}


/* The miss arm of the decode wrap: the detail object becomes the
   Invalid reason and dies with its own reference once Err's retention
   stands. */
static void emit_decode_miss(lower_ctx *c, lower_scope *scope, unsigned d,
                             const char *t, const char *y,
                             const char *err, const char *inv)
{
    const char *p = lower_temp(c, scope);
    const char *q = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", q, y);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(%s);\n", p, inv, q);
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, err, p);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", p);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", q);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
}

/* An ok flag with an out object and a detail object: the failure
   becomes Err the Invalid reason the detail spells. */
lower_val lower_wrap_decode(lower_ctx *c, lower_scope *scope,
                            const ek_node *e, const char *sym,
                            const char *args)
{
    unsigned d = c->depth;
    const char *t = lower_temp(c, scope);
    const char *o = lower_temp(c, scope);
    const char *y = lower_temp(c, scope);
    const char *ok = lower_temp(c, scope);
    const char *p = lower_temp(c, scope);
    int nok = 0, nerr = 0, ninv = 0;
    const char *okh = lower_wrap_helper(c, e, "Result", "Ok", &nok);
    const char *err = lower_wrap_helper(c, e, "Result", "Err", &nerr);
    const char *inv = lower_wrap_helper(c, e, "ParseError", "Invalid",
                                        &ninv);

    if (!okh || !err || !inv) {
        return lower_val_unit();
    }
    lower_pad(c);
    lower_emitf(c, "ek_obj *%s = (ek_obj *)0, *%s = (ek_obj *)0;\n", o, y);
    lower_pad(c);
    lower_emitf(c, "int %s = %s(%s, &%s, &%s);\n", ok, sym, args, o, y);
    lower_pad(c);
    lower_emitf(c, "ek_val %s;\n", t);
    lower_pad(c);
    lower_emitf(c, "if (%s) {\n", ok);
    c->depth = d + 1;
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_ref(%s);\n", p, o);
    lower_pad(c);
    lower_emitf(c, "%s = %s(%s);\n", t, okh, p);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", p);
    c->depth = d;
    emit_decode_miss(c, scope, d, t, y, err, inv);
    return (lower_val){ t, 1 };
}
