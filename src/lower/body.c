#include <string.h>

#include "lower/internal.h"

/* Function bodies: the scope bookkeeping, the statement members
   (initial bindings, rebinds, and expressions), the value of a block,
   and the ownership discipline at block ends. A block's own bindings
   are released in reverse declaration order when the block closes;
   parameters are borrowed and never released. */

static void emit_stmt(lower_ctx *c, lower_scope *scope, const ek_node *m);
static void emit_value_stmt(lower_ctx *c, lower_scope *s, const ek_node *m);
static void emit_let(lower_ctx *c, lower_scope *s, const ek_node *m);
static void emit_rebind(lower_ctx *c, lower_scope *s, const ek_node *m);

/* Appends one binding to a scope. */
static void scope_add(lower_ctx *c, lower_scope *s, const char *eko,
                      const char *cname, int is_param)
{
    lower_local *slots = lower_grow(c, s->locs, &s->cloc, s->nloc + 1,
                                    sizeof *slots);

    if (slots) {
        s->locs = slots;
        s->locs[s->nloc].eko = eko;
        s->locs[s->nloc].cname = cname;
        s->locs[s->nloc].is_param = is_param;
        s->nloc++;
    }
}

/* The innermost visible binding of a written name. */
const lower_local *scope_find_local(lower_scope *scope, const char *name)
{
    size_t i;

    for (; scope; scope = scope->parent) {
        for (i = 0; i < scope->nloc; i++) {
            if (strcmp(scope->locs[i].eko, name) == 0) {
                return &scope->locs[i];
            }
        }
    }
    return NULL;
}

/* Releases the block's own bindings, most recent first. */
void lower_release_scope(lower_ctx *c, lower_scope *s)
{
    size_t i = s->nloc;

    while (i > 0) {
        i--;
        if (!s->locs[i].is_param) {
            lower_pad(c);
            lower_emitf(c, "ek_release(%s);\n", s->locs[i].cname);
        }
    }
}

/* The value as an owned temp: an inline expression is materialized
   into a fresh temp first. */
const char *lower_as_temp(lower_ctx *c, lower_scope *scope, lower_val *v)
{
    const char *t;

    if (v->is_temp) {
        return v->text;
    }
    t = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s;\n", t, v->text);
    return t;
}

/* A fresh local's C name never collides with the temps of its own
   block, so a later read in the same block keeps its target. */
static int mentions_name(const ek_node *e, const char *name)
{
    size_t i;

    if (!e) {
        return 0;
    }
    if (e->kind == EK_N_NAME && e->u.path.count == 1
        && strcmp(e->u.path.data[0], name) == 0) {
        return 1;
    }
    switch (e->kind) {
    case EK_N_BINARY:
        return mentions_name(e->u.binary.lhs, name)
            || mentions_name(e->u.binary.rhs, name);
    case EK_N_UNARY:
    case EK_N_PAREN:
        return mentions_name(e->u.wrap.inner, name);
    case EK_N_CALL:
        if (mentions_name(e->u.call.callee, name)) {
            return 1;
        }
        for (i = 0; i < e->u.call.args.count; i++) {
            if (mentions_name(e->u.call.args.data[i], name)) {
                return 1;
            }
        }
        return 0;
    case EK_N_IF:
        return mentions_name(e->u.ifexpr.cond, name)
            || mentions_name(e->u.ifexpr.then, name)
            || mentions_name(e->u.ifexpr.els, name);
    case EK_N_STRING_LIT:
        for (i = 0; i < e->u.str.count; i++) {
            if (mentions_name(e->u.str.segs[i].expr, name)) {
                return 1;
            }
        }
        return 0;
    default:
        return 0;
    }
}

/* One statement member: a binding, a control form, or an expression
   whose value is discarded. */
static void emit_stmt(lower_ctx *c, lower_scope *scope, const ek_node *m)
{
    if (m->kind == EK_N_LET) {
        emit_let(c, scope, m);
    } else if (m->kind == EK_N_REBIND) {
        emit_rebind(c, scope, m);
    } else if (m->kind == EK_N_IF) {
        lower_val v;

        lower_if(c, scope, m, &v);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", v.text);
    } else {
        emit_value_stmt(c, scope, m);
    }
}

/* An expression member: its owned value is released, since nothing
   consumes it. */
static void emit_value_stmt(lower_ctx *c, lower_scope *scope, const ek_node *m)
{
    lower_val v = lower_expr(c, scope, m);

    if (v.is_temp) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", v.text);
        return;
    }
    {
        const char *t = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", t, v.text);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
    }
}

/* An initial binding: the right-hand side transfers into the local. A
   right-hand side that reads the name being bound goes through a temp,
   because the C declarator would otherwise capture the new variable.
   The discard form binds nothing: the value dies. */
static void emit_let(lower_ctx *c, lower_scope *scope, const ek_node *m)
{
    const ek_node *pat = m->u.bind.pattern;
    const char *name;
    const char *cname;
    lower_val v;

    if (pat && pat->kind == EK_N_P_WILDCARD) {
        v = lower_expr(c, scope, m->u.bind.value);
        cname = lower_as_temp(c, scope, &v);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", cname);
        return;
    }
    if (!pat || pat->kind != EK_N_P_VAR || pat->u.path.count != 1) {
        lower_error(c, m, "the destructuring pattern is not supported by "
                          "this lowerer");
        emit_value_stmt(c, scope, m->u.bind.value);
        return;
    }
    name = pat->u.path.data[0];
    v = lower_expr(c, scope, m->u.bind.value);
    cname = lower_c_name(c, scope, name);
    if (v.is_temp || !mentions_name(m->u.bind.value, name)
        || !scope_find_local(scope, name)) {
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", cname, v.text);
    } else {
        const char *t = lower_temp(c, scope);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", t, v.text);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", cname, t);
    }
    scope_add(c, scope, name, cname, 0);
}

/* A rebind: the right-hand side evaluates to a temp, the old value is
   released (unless the binding is a borrowed parameter), and the temp
   moves in. The discard form releases the value and binds nothing. A
   name no scope carries yet is bound fresh, exactly the first binding
   check's own reading gives it. */
static void emit_rebind(lower_ctx *c, lower_scope *scope, const ek_node *m)
{
    const ek_node *pat = m->u.bind.pattern;
    const char *name;
    const lower_local *loc;
    lower_val v;
    const char *t;

    if (pat && pat->kind == EK_N_P_WILDCARD) {
        v = lower_expr(c, scope, m->u.bind.value);
        t = lower_as_temp(c, scope, &v);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
        return;
    }
    if (!pat || pat->kind != EK_N_P_VAR || pat->u.path.count != 1) {
        lower_error(c, m, "the destructuring pattern is not supported by "
                          "this lowerer");
        emit_value_stmt(c, scope, m->u.bind.value);
        return;
    }
    name = pat->u.path.data[0];
    v = lower_expr(c, scope, m->u.bind.value);
    t = lower_as_temp(c, scope, &v);
    loc = scope_find_local(scope, name);
    if (!loc) {
        const char *cname = lower_c_name(c, scope, name);

        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", cname, t);
        scope_add(c, scope, name, cname, 0);
        return;
    }
    if (!loc->is_param) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", loc->cname);
    }
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", loc->cname, t);
}

/* The value of a body: a block is its statements followed by the value
   of its final member, and a bare expression is the value itself. A
   trailing binding's value is an owned copy of the local. */
lower_val lower_block_value(lower_ctx *c, lower_scope *scope,
                            const ek_node *body)
{
    const ek_nlist *kids;
    size_t n, i;
    const ek_node *m;

    if (!body) {
        return lower_val_unit();
    }
    if (body->kind != EK_N_BLOCK) {
        return lower_expr(c, scope, body);
    }
    kids = &body->u.kids;
    n = kids->count;
    for (i = 0; i + 1 < n; i++) {
        emit_stmt(c, scope, kids->data[i]);
    }
    if (n == 0) {
        return lower_val_unit();
    }
    m = kids->data[n - 1];
    if (m->kind == EK_N_LET || m->kind == EK_N_REBIND) {
        const ek_node *pat = m->u.bind.pattern;
        const lower_local *loc = NULL;

        emit_stmt(c, scope, m);
        if (pat && pat->kind == EK_N_P_VAR && pat->u.path.count == 1) {
            loc = scope_find_local(scope, pat->u.path.data[0]);
        }
        if (loc) {
            return (lower_val){ lower_str(c, "ek_keep(%s)", loc->cname), 0 };
        }
        return lower_val_unit();
    }
    return lower_expr(c, scope, m);
}

/* The parameter scope of one signature: the written names, the
   unnamed ones numbered. */
static size_t add_params(lower_ctx *c, lower_scope *ps, const ek_node *ann)
{
    const ek_nlist *params = &ann->u.t_fn.params;
    size_t i;

    for (i = 0; i < params->count; i++) {
        const ek_node *p = params->data[i];
        const char *name = NULL;
        const char *cname;

        if (p && p->kind == EK_N_T_PARAM && p->u.t_field.name
            && strcmp(p->u.t_field.name, "_") != 0) {
            name = p->u.t_field.name;
        }
        cname = name ? lower_c_name(c, ps, name) : lower_str(c, "a%zu", i);
        scope_add(c, ps, name ? name : cname, cname, 1);
    }
    return params->count;
}

/* The C signature of one emitted function, shared by its forward
   declaration and its definition. */
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
                           ps->locs[i].cname)) {
            c->oom = 1;
        }
    }
    if (!ek_buf_append_char(&b, ')')) {
        c->oom = 1;
    }
    return lower_take(c, &b);
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

    if (!decl || decl->kind != EK_N_LET || !ann || ann->kind != EK_N_T_FN) {
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

/* One emitted function: signature, body, and the return discipline —
   the body's value is held in a temp while the function's own bindings
   release in reverse order. */
void lower_function(lower_ctx *c, const ek_symbol *sym)
{
    lower_scope params;
    lower_scope top;
    const char *sig;
    size_t nparams;
    lower_val v;
    const char *ret;
    unsigned d;

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
    v = lower_block_value(c, &top, sym->decl->u.bind.value);
    if (v.is_temp) {
        ret = v.text;
    } else {
        ret = lower_temp(c, &top);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = %s;\n", ret, v.text);
    }
    lower_release_scope(c, &top);
    lower_pad(c);
    lower_emitf(c, "return %s;\n", ret);
    c->depth = d;
    lower_emitf(c, "}\n\n");
    c->fn_count++;
}
