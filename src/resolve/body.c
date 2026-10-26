#include <string.h>

#include "resolve/internal.h"

/* Body resolution: the lexical scopes of Clause 7.4.2 (the module top
   level, each block, each lambda body, each case arm with its guard).
   A local binding is visible from its binding line to the end of the
   enclosing block, so a value is resolved before its pattern binds;
   a block-level self-reference therefore reaches only an outer binding
   (Clauses 7.4.2, 8.2.1). */

static void r_let(r_ctx *c, r_file *f, const ek_node *m);

void r_block(r_ctx *c, r_file *f, const ek_node *block, scope *parent)
{
    scope *s = r_scope_new(c, parent);
    scope *old = f->locals;
    size_t i;

    if (c->oom) {
        return;
    }
    f->locals = s;
    for (i = 0; i < block->u.kids.count; i++) {
        const ek_node *m = block->u.kids.data[i];

        switch (m->kind) {
        case EK_N_LET:
            r_let(c, f, m);
            break;
        case EK_N_REBIND:
            r_expr(c, f, m->u.bind.value);
            r_pattern_refs(c, f, m->u.bind.pattern);
            r_pattern_dup(c, f, m->u.bind.pattern);
            r_pattern_binds(c, f, m->u.bind.pattern);
            break;
        case EK_N_INVERT:
            r_expr(c, f, m->u.bind.value);
            if (m->u.bind.pattern) {
                r_pattern_refs(c, f, m->u.bind.pattern);
                r_pattern_dup(c, f, m->u.bind.pattern);
                r_pattern_binds(c, f, m->u.bind.pattern);
            }
            break;
        default:
            r_expr(c, f, m);
            break;
        }
    }
    f->locals = old;
}

/* One body: a block, or one expression resolved in `parent`. */
void r_body(r_ctx *c, r_file *f, const ek_node *body, scope *parent)
{
    scope *old;

    if (c->oom || !body) {
        return;
    }
    if (body->kind == EK_N_BLOCK) {
        r_block(c, f, body, parent);
        return;
    }
    old = f->locals;
    f->locals = parent;
    r_expr(c, f, body);
    f->locals = old;
}

static void r_params(r_ctx *c, r_file *f, const ek_node *params, scope *into)
{
    size_t i;

    for (i = 0; i < params->u.kids.count; i++) {
        const ek_node *p = params->u.kids.data[i];

        if (p->kind == EK_N_LAMBDA_PARAMS) {
            r_params(c, f, p, into);
        } else if (p->kind == EK_N_NAME && p->u.path.count == 1
                   && strcmp(p->u.path.data[0], "_") != 0) {
            ek_symbol *sym = r_sym(c, p->u.path.data[0], EK_SYM_PARAM,
                                   f->mod->stem, p->line, p->col);

            if (sym) {
                r_scope_add(c, into, sym->name, sym);
            }
        }
    }
}

void r_lambda(r_ctx *c, r_file *f, const ek_node *lam)
{
    scope *ps = r_scope_new(c, f->locals);
    scope *old = f->locals;
    const ek_node *body;

    if (c->oom) {
        return;
    }
    f->locals = ps;
    r_params(c, f, lam->u.pair.a, ps);
    body = lam->u.pair.b;
    if (body && body->kind == EK_N_BLOCK) {
        r_block(c, f, body, ps);
    } else {
        r_expr(c, f, body);
    }
    f->locals = old;
}

void r_case(r_ctx *c, r_file *f, const ek_node *cs)
{
    size_t i;

    if (c->oom) {
        return;
    }
    for (i = 0; i < cs->u.caseexpr.scrutinees.count; i++) {
        r_expr(c, f, cs->u.caseexpr.scrutinees.data[i]);
    }
    for (i = 0; i < cs->u.caseexpr.arms.count; i++) {
        const ek_node *arm = cs->u.caseexpr.arms.data[i];
        scope *as = r_scope_new(c, f->locals);
        scope *old = f->locals;
        size_t k;

        if (c->oom) {
            return;
        }
        f->locals = as;
        for (k = 0; k < arm->u.arm.patterns.count; k++) {
            r_pattern_refs(c, f, arm->u.arm.patterns.data[k]);
            r_pattern_dup(c, f, arm->u.arm.patterns.data[k]);
            r_pattern_binds(c, f, arm->u.arm.patterns.data[k]);
        }
        if (arm->u.arm.guard) {
            r_expr(c, f, arm->u.arm.guard);
        }
        r_body(c, f, arm->u.arm.body, as);
        f->locals = old;
    }
}


/* A named function's parameters come from its annotation; only the
   top-level parameter list binds, not the parameters of nested
   function types (Clause 7.4.2). */
static void bind_annotation_params(r_ctx *c, r_file *f, const ek_node *ann,
                                   scope *into)
{
    size_t i;

    if (!ann || ann->kind != EK_N_T_FN || c->oom) {
        return;
    }
    for (i = 0; i < ann->u.t_fn.params.count; i++) {
        const ek_node *p = ann->u.t_fn.params.data[i];

        if (p->kind == EK_N_T_PARAM && p->u.t_field.name
            && strcmp(p->u.t_field.name, "_") != 0) {
            ek_symbol *sym = r_sym(c, p->u.t_field.name, EK_SYM_PARAM,
                                   f->mod->stem, p->line, p->col);

            if (sym) {
                r_scope_add(c, into, sym->name, sym);
            }
        }
    }
}

/* One annotated binding: the annotation's parameters scope over the
   right-hand side; the pattern binds in the enclosing scope
   (Clauses 7.4.2, 8.2.1). At the top level the surface holds the
   binding and there is no lexical scope to enter, but the pattern's
   constructor references are still recorded (Clause 7.4.2 row 3). */
static void r_let(r_ctx *c, r_file *f, const ek_node *m)
{
    scope *old = f->locals;

    if (m->u.bind.type && m->u.bind.type->kind == EK_N_T_FN) {
        f->locals = r_scope_new(c, f->locals);
        bind_annotation_params(c, f, m->u.bind.type, f->locals);
    }
    r_type(c, f, m->u.bind.type);
    r_expr(c, f, m->u.bind.value);
    f->locals = old;
    r_pattern_refs(c, f, m->u.bind.pattern);
    r_pattern_dup(c, f, m->u.bind.pattern);
    if (!old) {
        return;
    }
    r_pattern_binds(c, f, m->u.bind.pattern);
}

/* The top-level bodies of one module: bindings are mutually visible
   including their own right-hand sides (Clause 7.4.2). */
void r_tops(r_ctx *c, r_file *f, const ek_node *mod)
{
    size_t i, k;

    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *d = mod->u.kids.data[i];

        switch (d->kind) {
        case EK_N_IMPORT:
            continue;
        case EK_N_LET:
        case EK_N_REBIND:
        case EK_N_INVERT:
            r_let(c, f, d);
            break;
        case EK_N_NAMESPACE:
            r_type(c, f, d->u.namespace.type);
            if (d->u.namespace.type
                && d->u.namespace.type->kind == EK_N_T_FN) {
                scope *ps = r_scope_new(c, NULL);

                bind_annotation_params(c, f, d->u.namespace.type, ps);
                r_body(c, f, d->u.namespace.value, ps);
            } else {
                r_body(c, f, d->u.namespace.value, NULL);
            }
            break;
        case EK_N_TYPE_DECL:
            for (k = 0; k < d->u.type_decl.variants.count; k++) {
                r_type(c, f,
                       d->u.type_decl.variants.data[k]->u.variant.payload);
            }
            r_type(c, f, d->u.type_decl.type);
            break;
        case EK_N_CTYPE_DECL:
            r_type(c, f, d->u.decl.body);
            break;
        case EK_N_TEST_DECL:
            r_body(c, f, d->u.decl.body, NULL);
            break;
        case EK_N_EXTERN_DECL:
            r_type(c, f, d->u.extern_decl.type);
            break;
        default:
            r_expr(c, f, d);
            break;
        }
    }
}
