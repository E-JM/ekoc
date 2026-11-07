#include <string.h>

#include "lower/internal.h"

/* The prototype side of the native boundary: the C prototype of one
   extern declaration joins the unit at its first call site, with the
   closure parameters spelling their target-callable form and a shape
   without a lowering diagnosed once. */

/* The prototype text of the closure parameter: the target-callable
   function pointer, whose tail slot receives the context, and the
   extern's context data parameter beside it. */
static int closure_prototype(lower_ctx *c, ek_buf *b, const ek_node *cb,
                             int first)
{
    const ek_nlist *cps = &cb->u.t_fn.params;
    lower_mshape cr = lower_ms_of(c, cb->u.t_fn.result);
    const char *ret = lower_cbr_ret_type(c, &cr);
    int bad = !ret;
    size_t j;

    if (!ek_buf_printf(b, "%s%s (*)(", first ? ", " : "", ret)) {
        c->oom = 1;
    }
    for (j = 0; j < cps->count; j++) {
        lower_mshape cs = lower_ms_of(c, cps->data[j]->u.t_field.type);
        const char *ct = lower_cbr_param_type(c, &cs);

        if (!ct || !ek_buf_printf(b, "%s%s", j ? ", " : "", ct)) {
            c->oom = 1;
            bad = 1;
        }
    }
    if (!ek_buf_printf(b, "%svoid *)", cps->count ? ", " : "")
        || !ek_buf_printf(b, ", void *")) {
        c->oom = 1;
    }
    return !bad;
}

/* The parameter list of the prototype, left to right; a shape without
   a parameter lowering answers 0. */
static int prototype_params(lower_ctx *c, ek_buf *b, const ek_node *ann)
{
    const ek_nlist *params = &ann->u.t_fn.params;
    size_t i;

    for (i = 0; i < params->count; i++) {
        const ek_node *tn = params->data[i]->u.t_field.type;
        lower_mshape s = lower_ms_of(c, tn);

        if (s.kind == MS_CLOSURE) {
            if (!closure_prototype(c, b, tn, i)) {
                return 0;
            }
            continue;
        }
        {
            const char *pt = lower_ms_param_type(c, &s);

            if (!pt || s.kind == MS_UNIT
                || !ek_buf_printf(b, "%s%s", i ? ", " : "", pt)) {
                return 0;
            }
        }
    }
    return 1;
}

/* The prototype line: the result must carry a lowering too. */
static int emit_prototype(lower_ctx *c, const ek_symbol *sym,
                          const ek_node *ann)
{
    ek_buf b;
    lower_mshape res = lower_ms_of(c, ann->u.t_fn.result);
    const char *ret = res.kind == MS_UNIT ? "void"
                                          : lower_ms_param_type(c, &res);
    int ok = ret && res.kind != MS_BYTES && res.kind != MS_CIN
        && res.kind != MS_CIN_LIST
        && res.kind != MS_CLOSURE && res.kind != MS_BAD;

    if (!ok) {
        return 0;
    }
    ek_buf_init(&b);
    if (!prototype_params(c, &b, ann)) {
        ek_buf_destroy(&b);
        return 0;
    }
    if (!ek_buf_printf(&c->cext, "extern %s %s(", ret, sym->extern_symbol)
        || (b.data && !ek_buf_append_bytes(&c->cext, b.data, b.len))
        || !ek_buf_printf(&c->cext, b.len ? ");\n\n" : "void);\n\n")) {
        c->oom = 1;
    }
    ek_buf_destroy(&b);
    return 1;
}

/* The registry entry of one extern declaration, created at its first
   call site: the shapes classify (registering the ctype structs), the
   prototype joins the unit, and a shape without a lowering is
   diagnosed once. */
lower_ext *extern_entry(lower_ctx *c, const ek_symbol *sym)
{
    const ek_node *ann = sym->type;
    lower_ext *slots;
    lower_ext *ent = NULL;
    size_t i;

    for (i = 0; i < c->next_r; i++) {
        if (c->exts[i].sym == sym) {
            return &c->exts[i];
        }
    }
    slots = lower_grow(c, c->exts, &c->cext_r, c->next_r + 1, sizeof *slots);
    if (!slots) {
        return NULL;
    }
    c->exts = slots;
    ent = &slots[c->next_r++];
    ent->sym = sym;
    ent->bad = 0;
    if (!ann || ann->kind != EK_N_T_FN) {
        lower_error(c, sym->decl, "the signature of `%s` is not a "
                                  "function type", sym->name);
        ent->bad = 1;
        return ent;
    }
    if (!emit_prototype(c, sym, ann)) {
        lower_error(c, sym->decl, "the signature of `%s` has no lowering at "
                                  "the boundary", sym->name);
        ent->bad = 1;
    }
    return ent;
}
