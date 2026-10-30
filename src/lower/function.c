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

/* The bundle constructor the entry shim builds for one capability of
   the manifest, or NULL when the lowerer cannot supply it. */
static const char *shim_ctor_of(const char *cap)
{
    static const struct {
        const char *cap;
        const char *ctor;
    } ctors[] = {
        {"Out",   "ek_cap_out_stdout"},
        {"In",    "ek_cap_in_stdin"},
        {"Fs",    "ek_cap_fs_cwd"},
        {"Env",   "ek_cap_env_os"},
        {"Args",  "ek_cap_args"},
        {"Clock", "ek_cap_clock_wall"},
        {"Rand",  "ek_cap_rand_os"},
        {"Proc",  "ek_cap_proc"},
    };
    size_t i;

    for (i = 0; i < sizeof ctors / sizeof *ctors; i++) {
        if (strcmp(ctors[i].cap, cap) == 0) {
            return ctors[i].ctor;
        }
    }
    return NULL;
}

/* The authorization manifest: every parameter is a capability the shim
   supplies by position, and the runtime's shared families register
   once for the manifest. A parameter outside the table keeps the
   no-shim balance. */
static int entry_manifest(lower_ctx *c, const ek_node *decl,
                          const ek_node *ann)
{
    const ek_nlist *params = &ann->u.t_fn.params;
    size_t i;

    for (i = 0; i < params->count; i++) {
        const char *cap = param_cap_name(params->data[i]);
        const char *ctor = cap ? shim_ctor_of(cap) : NULL;

        if (!ctor) {
            lower_error(c, decl, "a `main` parameter of `%s` is not "
                                 "supported by this lowerer",
                        cap ? cap : "?");
            c->nshim = 0;
            return 0;
        }
        c->shim[c->nshim++] = ctor;
    }
    lower_cap_families(c, decl);
    return 1;
}

/* The declaration of one function: the signature check, the parameter
   scope, the entry bookkeeping, and the stored forward declaration.
   name is the emitted spelling, plain or instantiation-suffixed.
   Returns the signature, or NULL when the function has no lowering. */
static const char *declare_function(lower_ctx *c, const ek_symbol *sym,
                                    const char *name, lower_scope *ps,
                                    size_t *nparams)
{
    const ek_node *ann = sym->type;
    const ek_node *decl = sym->decl;
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
    if (sym == c->entry) {
        c->entry_mangled = name;
        if (*nparams == 0) {
            c->has_main = 1;
        } else if (ann->u.t_fn.params.count <= sizeof c->shim
                       / sizeof *c->shim
                   && entry_manifest(c, decl, ann)) {
            /* the shim builds every declared capability by position */
            c->has_main = 1;
        }
    }
    sig = signature(c, name, ps);
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

/* The return discipline: the body's value lands in a temp while the
   owned locals and parameters release in reverse order. */
static void finish_function(lower_ctx *c, lower_scope *params,
                            lower_scope *top, lower_val v)
{
    const char *ret;

    if (!v.text) {
        v = lower_val_unit(); /* the tail jumped to retry */
    }
    if (v.is_temp) {
        ret = v.text;
    } else {
        ret = lower_temp(c, top);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", ret, v.text);
    }
    lower_release_scope(c, top);
    lower_release_scope(c, params);
    lower_pad(c);
    lower_emitf(c, "return %s;\n", ret);
}

/* One emitted function: signature, the owned-parameter prologue, the
   optional retry label, the body, and the return discipline. A
   specialized item builds its binder substitution first, so every
   row-type read in the body sees the instantiation's types. */
void lower_function(lower_ctx *c, const lower_item *item)
{
    lower_scope params;
    lower_scope top;
    const ek_symbol *sym = item->sym;
    const char **saved_names = c->env_names;
    const ek_type **saved_args = c->env_args;
    size_t saved_n = c->nenv, saved_cap = c->cenv_cap;
    const char *sig;
    size_t nparams;
    lower_val v;
    unsigned d;
    int saved;

    c->temp = 0;
    c->file = surface_file(c, sym->module);
    c->env_names = NULL;
    c->env_args = NULL;
    c->nenv = 0;
    c->cenv_cap = 0;
    if (item->nargs) {
        lower_env_build(c, sym, item->fn);
    }
    sig = declare_function(c, sym, item->name, &params, &nparams);
    if (!sig) {
        c->env_names = saved_names;
        c->env_args = saved_args;
        c->nenv = saved_n;
        c->cenv_cap = saved_cap;
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
    finish_function(c, &params, &top, v);
    c->depth = d;
    lower_emitf(c, "}\n\n");
    c->fn_count++;
    c->tce_self = NULL;
    c->tce_params = NULL;
    c->tce_label = NULL;
    c->env_names = saved_names;
    c->env_args = saved_args;
    c->nenv = saved_n;
    c->cenv_cap = saved_cap;
    lower_flush_pending(c);
}