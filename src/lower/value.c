#include <string.h>

#include "lower/internal.h"

/* The top-level value bindings: a module-level binding whose
   annotation is not a function type holds one value for the whole
   run. The binding lowers to a lazily initialized static slot behind
   a getter, so a reference reads the getter, initialization order
   resolves at first use, and a binding nothing reads joins no body.
   The initializer lowers like any function body: its own references
   pull their definitions into the unit. */

/* True when the symbol is a top-level value binding: a plain or
   namespace binding whose annotation is not a function type. An
   unannotated binding is a value unless its right-hand side is a
   lambda, which names a function. */
int lower_is_value(const ek_symbol *sym)
{
    const ek_node *ann;
    const ek_node *v;

    if (!(sym && (sym->kind == EK_SYM_FUNC
                  || sym->kind == EK_SYM_NAMESPACE)
          && sym->decl
          && (sym->decl->kind == EK_N_LET
              || sym->decl->kind == EK_N_NAMESPACE))) {
        return 0;
    }
    ann = sym->decl->kind == EK_N_NAMESPACE ? sym->decl->u.namespace.type
                                            : sym->decl->u.bind.type;
    if (ann) {
        return ann->kind != EK_N_T_FN;
    }
    if (sym->decl->kind != EK_N_LET) {
        return 1;
    }
    v = sym->decl->u.bind.value;
    while (v && v->kind == EK_N_PAREN) {
        v = v->u.wrap.inner;
    }
    return !(v && v->kind == EK_N_LAMBDA);
}

/* The C name pieces of one value binding: the kind letter joins the
   module, the namespace owner when the binding carries one, and the
   name on `$`, so no two bindings share a spelling. */
static const char *value_id(lower_ctx *c, const ek_symbol *sym,
                            const char *kind)
{
    if (sym->owner) {
        return lower_str(c, "%s_%s$%s$%s", kind, sym->module, sym->owner,
                         sym->name);
    }
    return lower_str(c, "%s_%s$%s", kind, sym->module, sym->name);
}

/* The getter's C name of one value binding. */
const char *lower_value_getter(lower_ctx *c, const ek_symbol *sym)
{
    return value_id(c, sym, "eks");
}

/* The slot's C name of one value binding. */
static const char *value_slot(lower_ctx *c, const ek_symbol *sym)
{
    return value_id(c, sym, "ekv");
}

/* The slot, its flag, and the getter's declaration: the text the
   initializer body sits inside. */
static void emit_slot_head(lower_ctx *c, const ek_symbol *sym)
{
    const char *slot = value_slot(c, sym);
    const char *flag = lower_str(c, "%s_on", slot);
    const char *get = lower_value_getter(c, sym);

    lower_push_sig(c, lower_str(c, "static ek_val %s(void)", get));
    lower_emitf(c, "static ek_val %s;\n", slot);
    lower_emitf(c, "static int %s;\n", flag);
    lower_emitf(c, "static ek_val %s(void)\n{\n", get);
    lower_emitf(c, "    if (!%s) {\n", flag);
}

/* A pattern-headed binding: the initializer lowers once, the
   pattern's names bind as locals, and this symbol's slot takes its
   own component; the siblings die with the scope. */
static void value_slot_from_pattern(lower_ctx *c, lower_scope *top,
                                    const ek_node *decl, const ek_node *pat,
                                    const ek_symbol *sym, const char *slot)
{
    const lower_local *loc = NULL;

    if (!lower_value_destructure(c, top, decl, pat, sym->name)) {
        lower_error(c, decl, "the destructuring pattern is not "
                             "supported by this lowerer");
        return;
    }
    loc = scope_find_local(top, sym->name);
    if (loc) {
        lower_pad(c);
        lower_emitf(c, "%s = ek_keep(%s);\n", slot, loc->cname);
    }
}

/* One value binding: the slot and its initialized flag, the getter
   that runs the initializer once, and the keep discipline the
   references read through. The initializer's owned result moves into
   the slot before the binding scope releases. */
void lower_value_binding(lower_ctx *c, const lower_item *item)
{
    const ek_symbol *sym = item->sym;
    const ek_node *decl = sym->decl;
    const ek_node *value = decl->kind == EK_N_NAMESPACE
                               ? decl->u.namespace.value
                               : decl->u.bind.value;
    const ek_node *pat = decl->kind == EK_N_LET
                             ? decl->u.bind.pattern : NULL;
    int pattern_head = pat && pat->kind != EK_N_P_VAR
                       && pat->kind != EK_N_P_WILDCARD;
    const ek_ck_row *row = lower_row(c, sym->type);
    const char *slot = value_slot(c, sym);
    const char *flag = lower_str(c, "%s_on", slot);
    lower_scope top;
    lower_val v;
    const char *t;
    unsigned d;

    if (row && row->type && lower_type_has_var(row->type)) {
        lower_error(c, sym->decl, "the generic value binding `%s` has no "
                                  "lowering", sym->name);
        return;
    }
    c->file = surface_file(c, sym->module);
    emit_slot_head(c, sym);
    d = c->depth;
    c->depth = 2;
    c->temp = 0;
    c->tce_self = NULL;
    c->tce_params = NULL;
    c->tce_label = NULL;
    c->in_tail = 1;
    memset(&top, 0, sizeof top);
    if (pattern_head) {
        value_slot_from_pattern(c, &top, decl, pat, sym, slot);
    } else {
        v = lower_block_value(c, &top, value);
        if (!v.text) {
            v = lower_val_unit(); /* a jumped tail falls back to the unit */
        }
        t = lower_as_temp(c, &top, &v);
        lower_pad(c);
        lower_emitf(c, "%s = %s;\n", slot, t);
    }
    c->in_tail = 0;
    lower_pad(c);
    lower_emitf(c, "%s = 1;\n", flag);
    lower_release_scope(c, &top);
    c->depth = 1;
    lower_emitf(c, "    }\n");
    lower_emitf(c, "    return ek_keep(%s);\n", slot);
    c->depth = d;
    lower_emitf(c, "}\n\n");
    c->fn_count++;
    lower_flush_pending(c);
}
