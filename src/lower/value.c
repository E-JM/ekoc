#include <string.h>

#include "lower/internal.h"

/* The top-level value bindings: a module-level binding whose
   annotation is not a function type holds one value for the whole
   run. The binding lowers to a lazily initialized static slot behind
   a getter, so a reference reads the getter, initialization order
   resolves at first use, and a binding nothing reads joins no body.
   The initializer lowers like any function body: its own references
   pull their definitions into the unit. */

/* True when the symbol is a top-level value binding: a plain binding
   whose annotation is not a function type. A generic one keeps the
   value false and is diagnosed at its emission. */
int lower_is_value(const ek_symbol *sym)
{
    return sym && sym->kind == EK_SYM_FUNC && sym->decl
        && sym->decl->kind == EK_N_LET && sym->type
        && sym->type->kind != EK_N_T_FN;
}

/* The getter's C name of one value binding. */
const char *lower_value_getter(lower_ctx *c, const ek_symbol *sym)
{
    return lower_str(c, "eks_%s$%s", sym->module, sym->name);
}

/* The slot's C name of one value binding. */
static const char *value_slot(lower_ctx *c, const ek_symbol *sym)
{
    return lower_str(c, "ekv_%s$%s", sym->module, sym->name);
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

/* One value binding: the slot and its initialized flag, the getter
   that runs the initializer once, and the keep discipline the
   references read through. The initializer's owned result moves into
   the slot before the binding scope releases. */
void lower_value_binding(lower_ctx *c, const lower_item *item)
{
    const ek_symbol *sym = item->sym;
    const ek_node *value = sym->decl->u.bind.value;
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
    v = lower_block_value(c, &top, value);
    c->in_tail = 0;
    if (!v.text) {
        v = lower_val_unit(); /* a jumped tail falls back to the unit */
    }
    t = lower_as_temp(c, &top, &v);
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", slot, t);
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
