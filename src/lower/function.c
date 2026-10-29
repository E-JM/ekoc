#include <string.h>

#include "lower/internal.h"

/* The emitted functions: the signature and its borrowed ABI slots, the
   owned-parameter prologue, the retry label of the self tail-call
   transform, and the return discipline. */

/* The parameter scope of one signature: the written names, the
   unnamed ones numbered. Each parameter keeps its borrowed ABI slot
   into an owned local of the visible name. */
static size_t add_params(lower_ctx *c, lower_scope *ps, const ek_node *ann)
{
    const ek_nlist *params = &ann->u.t_fn.params;
    size_t i;

    for (i = 0; i < params->count; i++) {
        const ek_node *p = params->data[i];
        const char *name = NULL;
        const char *cname;
        const char *abi = lower_str(c, "ek_p%zu", i);

        if (p && p->kind == EK_N_T_PARAM && p->u.t_field.name
            && strcmp(p->u.t_field.name, "_") != 0) {
            name = p->u.t_field.name;
        }
        /* the owned local never shares the ABI slot's spelling, so the
           prologue's keep reads the borrowed slot */
        cname = lower_c_name(c, ps, name ? name : lower_str(c, "a%zu", i));
        lower_scope_add(c, ps, name ? name : cname, cname, 0, abi);
    }
    return params->count;
}
/* The body node of one function binding: a plain binding's value, or
   a namespace member's value. */
static const ek_node *decl_body(const ek_symbol *sym)
{
    const ek_node *decl = sym->decl;

    if (!decl) {
        return NULL;
    }
    if (decl->kind == EK_N_LET) {
        return decl->u.bind.value;
    }
    if (decl->kind == EK_N_NAMESPACE) {
        return decl->u.namespace.value;
    }
    return NULL;
}

/* The C signature of one emitted function, shared by its forward
   declaration and its definition: the borrowed ABI slots. */
static const char *signature(lower_ctx *c, const char *mangled,
                             const lower_scope *ps)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    if (!ek_buf_append_str(&b, "static ek_val ")
        || !ek_buf_append_str(&b, mangled) || !ek_buf_append_str(&b, "(")) {
        c->oom = 1;
    }
    if (ps->nloc == 0) {
        if (!ek_buf_append_str(&b, "void")) {
            c->oom = 1;
        }
    }
    for (i = 0; i < ps->nloc; i++) {
        if (!ek_buf_printf(&b, "%sek_val %s", i ? ", " : "",
                           ps->locs[i].abi ? ps->locs[i].abi
                                           : ps->locs[i].cname)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_append_char(&b, ')')) {
        c->oom = 1;
    }
    return lower_take(c, &b);
}
/* The capability name of one parameter's written type: the final name
   of the type node, parens unwrapped. */
static const char *param_cap_name(const ek_node *p)
{
    const ek_node *t;

    if (!p || p->kind != EK_N_T_PARAM) {
        return NULL;
    }
    t = p->u.t_field.type;
    while (t && t->kind == EK_N_T_PAREN) {
        t = t->u.wrap.inner;
    }
    if (!t || t->kind != EK_N_T_NAME || t->u.path.count == 0) {
        return NULL;
    }
    return t->u.path.data[t->u.path.count - 1];
}

/* Whether the annotation is exactly one parameter of type Out: the
   capability the entry shim can supply. */
static int out_main_param(const ek_node *ann)
{
    const char *name;

    if (!ann || ann->kind != EK_N_T_FN
        || ann->u.t_fn.params.count != 1) {
        return 0;
    }
    name = param_cap_name(ann->u.t_fn.params.data[0]);
    return name && strcmp(name, "Out") == 0;
}

/* The declaration of one function: the signature check, the parameter
   scope, the entry bookkeeping, and the stored forward declaration.
   Returns the signature, or NULL when the function has no lowering. */
static const char *declare_function(lower_ctx *c, const ek_symbol *sym,
                                    lower_scope *ps, size_t *nparams)
{
    const ek_node *ann = sym->type;
    const ek_node *decl = sym->decl;
    const char *mangled;
    const char *sig;
    const char **slot;

    if (!decl || (decl->kind != EK_N_LET && decl->kind != EK_N_NAMESPACE)
        || !ann || ann->kind != EK_N_T_FN) {
        if (decl) {
            lower_error(c, decl, "the signature of `%s` has no lowering",
                        sym->name);
        }
        return NULL;
    }
    memset(ps, 0, sizeof *ps);
    *nparams = add_params(c, ps, ann);
    mangled = lower_mangled(c, sym);
    if (sym == c->entry) {
        c->entry_mangled = mangled;
        if (*nparams == 0) {
            c->has_main = 1;
        } else if (out_main_param(ann)) {
            /* the shim builds the standard output's Out bundle and
               supplies it by position */
            c->has_main = 1;
            c->main_cap_out = 1;
        } else {
            lower_error(c, decl, "a `main` with parameters is not "
                                 "supported by this lowerer");
        }
    }
    sig = signature(c, mangled, ps);
    slot = lower_grow(c, c->sigs, &c->csig, c->nsig + 1, sizeof *slot);
    if (slot) {
        slot[c->nsig++] = sig;
        c->sigs = slot;
    }
    return sig;
}

/* The entry text of one function: the owned-parameter prologue and,
   when the body has a tail self-call, the retry label. */
static void emit_prologue(lower_ctx *c, lower_scope *params,
                          const ek_symbol *sym)
{
    size_t i;

    for (i = 0; i < params->nloc; i++) {
        lower_emitf(c, "    ek_val %s = ek_keep(%s);\n",
                    params->locs[i].cname, params->locs[i].abi);
    }
    c->tce_self = sym;
    c->tce_params = params;
    c->tce_label = lower_has_self_tail(c, sym, decl_body(sym))
                       ? "retry" : NULL;
    if (c->tce_label) {
        lower_emitf(c, "retry: ;\n");
    }
}

/* One emitted function: signature, the owned-parameter prologue, the
   optional retry label, the body, and the return discipline — the
   body's value is held in a temp while the owned locals, parameters
   included, release in reverse order. */
void lower_function(lower_ctx *c, const ek_symbol *sym)
{
    lower_scope params;
    lower_scope top;
    const char *sig;
    size_t nparams;
    lower_val v;
    const char *ret;
    unsigned d;
    int saved;

    c->temp = 0;
    c->file = surface_file(c, sym->module);
    sig = declare_function(c, sym, &params, &nparams);
    if (!sig) {
        return;
    }
    memset(&top, 0, sizeof top);
    top.parent = &params;
    lower_emitf(c, "%s\n{\n", sig);
    d = c->depth;
    c->depth = 1;
    emit_prologue(c, &params, sym);
    saved = c->in_tail;
    c->in_tail = 1;
    v = lower_block_value(c, &top, decl_body(sym));
    c->in_tail = saved;
    if (!v.text) {
        v = lower_val_unit(); /* the tail jumped to retry */
    }
    if (v.is_temp) {
        ret = v.text;
    } else {
        ret = lower_temp(c, &top);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", ret, v.text);
    }
    lower_release_scope(c, &top);
    lower_release_scope(c, &params);
    lower_pad(c);
    lower_emitf(c, "return %s;\n", ret);
    c->depth = d;
    lower_emitf(c, "}\n\n");
    c->fn_count++;
    c->tce_self = NULL;
    c->tce_params = NULL;
    c->tce_label = NULL;
    lower_flush_pending(c);
}