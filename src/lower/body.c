#include <string.h>

#include "lower/internal.h"

/* Function bodies: the scope bookkeeping, the statement members
   (initial bindings, rebinds, and expressions), the value of a block,
   and the ownership discipline at block ends. Owned bindings release
   at their scope's close in reverse order; borrowed scope entries
   (reads through a closure environment) never release. */

static void emit_stmt(lower_ctx *c, lower_scope *scope, const ek_node *m);
static void emit_value_stmt(lower_ctx *c, lower_scope *s, const ek_node *m);
static void emit_let(lower_ctx *c, lower_scope *s, const ek_node *m);
static void emit_rebind(lower_ctx *c, lower_scope *s, const ek_node *m);

/* Appends one binding to a scope. */
void lower_scope_add(lower_ctx *c, lower_scope *s, const char *eko,
                     const char *cname, int is_param, const char *abi)
{
    lower_local *slots = lower_grow(c, s->locs, &s->cloc, s->nloc + 1,
                                    sizeof *slots);

    if (slots) {
        s->locs = slots;
        s->locs[s->nloc].eko = eko;
        s->locs[s->nloc].cname = cname;
        s->locs[s->nloc].is_param = is_param;
        s->locs[s->nloc].abi = abi;
        s->locs[s->nloc].aliased = 0;
        s->nloc++;
    }
}

/* The innermost visible binding of a written name: the latest binding
   of the name in the first scope that carries one. A re-let of the
   name in the same scope replaces the earlier entry's reads while
   both entries release at the scope's close. */
const lower_local *scope_find_local(lower_scope *scope, const char *name)
{
    size_t i;

    for (; scope; scope = scope->parent) {
        for (i = scope->nloc; i > 0; i--) {
            if (strcmp(scope->locs[i - 1].eko, name) == 0) {
                return &scope->locs[i - 1];
            }
        }
    }
    return NULL;
}

/* Releases the block's own owned bindings, most recent first. A
   borrowed binding (a capture read) and a parameter local that
   aliases its ABI slot hold no reference of their own and release
   nothing. */
void lower_release_scope(lower_ctx *c, lower_scope *s)
{
    size_t i = s->nloc;

    while (i > 0) {
        i--;
        if (!s->locs[i].is_param && !s->locs[i].aliased) {
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
    if (pat && pat->kind == EK_N_P_PAREN
        && lower_tuple_let(c, scope, m, pat)) {
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
    if (m->u.bind.type) {
        /* the annotation's expectation adapts a nominal record value
           into the anonymous shape it declares */
        const ek_ck_row *arow = lower_row(c, m->u.bind.type);

        v = lower_shape_adapt(c, scope, v, m,
                              lower_row_type(c, m->u.bind.value),
                              arow ? arow->type : NULL);
    }
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
    lower_scope_add(c, scope, name, cname, 0, NULL);
}

/* A rebind: the right-hand side evaluates to a temp, the old value is
   released, and the temp moves in. The discard form releases the value
   and binds nothing. A name no scope carries yet is bound fresh,
   exactly the first binding check's own reading gives it. A borrowed
   binding (a capture) cannot rebind: the value dies instead. */
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
        lower_scope_add(c, scope, name, cname, 0, NULL);
        return;
    }
    if (loc->is_param) {
        lower_error(c, m, "the rebinding of the captured `%s` has no "
                          "lowering", name);
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", t);
        return;
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", loc->cname);
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", loc->cname, t);
}

/* The members of a body from one slice: the statements, then the value
   of the final member. An inverted binding consumes the members after
   it as its callback body, so the walk ends there. A final binding's
   value is an owned copy of the local. The tail marker is inherited by
   the final member and cleared for the statements; it is restored at
   every exit. */
lower_val lower_members(lower_ctx *c, lower_scope *scope,
                        const ek_node **kids, size_t n)
{
    const ek_node *m;
    size_t i;
    int saved = c->in_tail;
    lower_val out;

    c->in_tail = 0;
    for (i = 0; i + 1 < n; i++) {
        m = kids[i];
        if (m->kind == EK_N_INVERT) {
            out = lower_invert(c, scope, m, kids + i + 1, n - i - 1);
            c->in_tail = saved;
            return out;
        }
        emit_stmt(c, scope, m);
    }
    if (n == 0) {
        c->in_tail = saved;
        return lower_val_unit();
    }
    m = kids[n - 1];
    if (m->kind == EK_N_INVERT) {
        out = lower_invert(c, scope, m, NULL, 0);
        c->in_tail = saved;
        return out;
    }
    if (m->kind == EK_N_LET || m->kind == EK_N_REBIND) {
        const ek_node *pat = m->u.bind.pattern;
        const lower_local *loc = NULL;

        emit_stmt(c, scope, m);
        if (pat && pat->kind == EK_N_P_VAR && pat->u.path.count == 1) {
            loc = scope_find_local(scope, pat->u.path.data[0]);
        }
        c->in_tail = saved;
        if (loc) {
            return (lower_val){ lower_str(c, "ek_keep(%s)", loc->cname), 0 };
        }
        return lower_val_unit();
    }
    c->in_tail = saved;
    out = lower_expr(c, scope, m);
    c->in_tail = saved;
    return out;
}

/* The value of a body: a block is its members, and a bare expression
   is the value itself. */
lower_val lower_block_value(lower_ctx *c, lower_scope *scope,
                            const ek_node *body)
{
    if (!body) {
        return lower_val_unit();
    }
    if (body->kind == EK_N_BLOCK) {
        return lower_members(c, scope, (const ek_node **)body->u.kids.data,
                             body->u.kids.count);
    }
    return lower_members(c, scope, &body, 1);
}






