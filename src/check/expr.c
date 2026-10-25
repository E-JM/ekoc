#include <string.h>

#include "check/internal.h"

/* Expression typing: the lexical scopes of Clause 7.4.2, the surface
   schemes of Clause 7.6.3, the name readings, and the dispatch to the
   per-concern files (operators, literals, calls, patterns, control
   forms). */

void ck_scope_push(ck_ctx *c)
{
    ck_scope *s = ek_arena_alloc(c->arena, sizeof *s);

    if (!s) {
        c->oom = 1;
        return;
    }
    memset(s, 0, sizeof *s);
    s->parent = c->locals;
    c->locals = s;
}

void ck_scope_pop(ck_ctx *c)
{
    if (c->locals) {
        c->locals = c->locals->parent;
    }
}

void ck_scope_bind_scheme(ck_ctx *c, const char *name, ck_scheme *s)
{
    ck_scope *sc = c->locals;

    if (c->oom || !sc || !name || !s || strcmp(name, "_") == 0) {
        return;
    }
    if (sc->count == sc->cap) {
        size_t ncap = sc->cap ? sc->cap * 2 : 8;
        const char **nn = ek_arena_alloc(c->arena, ncap * sizeof *nn);
        ck_scheme **nv = ek_arena_alloc(c->arena, ncap * sizeof *nv);

        if (!nn || !nv) {
            c->oom = 1;
            return;
        }
        if (sc->count) {
            memcpy(nn, sc->names, sc->count * sizeof *nn);
            memcpy(nv, sc->vals, sc->count * sizeof *nv);
        }
        sc->names = nn;
        sc->vals = nv;
        sc->cap = ncap;
    }
    sc->names[sc->count] = name;
    sc->vals[sc->count] = s;
    sc->count++;
}

void ck_scope_bind(ck_ctx *c, const char *name, const ek_type *t)
{
    ck_scheme *s = ek_arena_alloc(c->arena, sizeof *s);

    if (!s) {
        c->oom = 1;
        return;
    }
    memset(s, 0, sizeof *s);
    s->type = t;
    ck_scope_bind_scheme(c, name, s);
}

ck_scheme *ck_scope_get_scheme(const ck_ctx *c, const char *name)
{
    const ck_scope *s;

    if (!name) {
        return NULL;
    }
    for (s = c->locals; s; s = s->parent) {
        size_t i;

        for (i = s->count; i > 0; i--) {
            if (strcmp(s->names[i - 1], name) == 0) {
                return s->vals[i - 1];
            }
        }
    }
    return NULL;
}

const ek_type *ck_scope_get(const ck_ctx *c, const char *name)
{
    const ck_scheme *s = ck_scope_get_scheme(c, name);

    return s ? s->type : NULL;
}

/* The scheme of a surface symbol, built once from its annotation
   (Clause 7.6.3) and instantiated per use (Clause 7.6.4). */
static ck_scheme *scheme_of(ck_ctx *c, const ek_symbol *sym)
{
    ck_scheme *s = (ck_scheme *)ck_map_get(&c->schemes, sym);
    const ek_type *t;

    if (s) {
        return s;
    }
    s = ek_arena_alloc(c->arena, sizeof *s);
    if (!s) {
        c->oom = 1;
        return NULL;
    }
    memset(s, 0, sizeof *s);
    if (sym->type) {
        ck_tvars tv;

        memset(&tv, 0, sizeof tv);
        t = ck_elaborate(c, sym->type, &tv);
        *s = ck_generalize(c, t);
    }
    ck_map_put(c, &c->schemes, sym, s);
    return s;
}

const ek_type *ck_type_of(ck_ctx *c, const ek_symbol *sym)
{
    ck_scheme *s;

    if (!sym) {
        return ck_ivar(c);
    }
    s = scheme_of(c, sym);
    if (!s || !s->type) {
        return ck_ivar(c); /* unannotated interface rows: opacity */
    }
    return ck_instantiate(c, s);
}

/* A name in value position: its kind comes from the resolver, its
   type from the checked environment. */
static const ek_type *name_type(ck_ctx *c, const ek_node *n,
                                const ek_type *expected)
{
    const ek_ref *ref = NULL;
    const char *name = n->u.path.count == 1 ? n->u.path.data[0] : NULL;

    if (ck_ref_at(c, n, 0, &ref)) {
        switch (ref->kind) {
        case EK_SYM_PARAM:
        case EK_SYM_LOCAL:
            if (ref->symbol) {
                ck_scheme *s = ck_scope_get_scheme(c, ref->symbol->name);

                if (s && s->type) {
                    return ck_instantiate(c, s);
                }
            }
            return ck_ivar(c);
        case EK_SYM_FUNC:
        case EK_SYM_EXTERN:
        case EK_SYM_NAMESPACE:
        case EK_SYM_TEST:
            return ck_type_of(c, ref->symbol);
        case EK_SYM_TYPE:
            /* A Pascal head in a chain: the std placeholder types
               carry their symbol, user types always do. */
            return ref->symbol ? ck_nominal(c, ref->symbol, NULL, 0)
                               : ck_ivar(c);
        case EK_SYM_EXTERNAL:
            if (ref->symbol && ref->symbol->kind == EK_SYM_TYPE) {
                return ck_nominal(c, ref->symbol, NULL, 0);
            }
            return ck_ivar(c);
        case EK_SYM_CONSTRUCTOR: {
            const ck_ctor *ct = ref->symbol ? ck_ctor_of(c, ref->symbol)
                                            : NULL;
            const ck_decl *d = ct && ct->owner ? ck_decl_of(c, ct->owner)
                                               : NULL;
            const ek_type *exp = ck_prune(expected);

            if (ct && ct->owner && (!d || d->nbinders == 0)) {
                return ck_nominal(c, ct->owner, NULL, 0);
            }
            if (exp && exp->kind == EK_T_NOMINAL && ct
                && exp->u.nominal.sym == ct->owner) {
                return exp; /* the expected type anchors it (7.6.4) */
            }
            if (ct && ct->owner) {
                ck_error(c, n, "E-undetermined",
                         "the constructor `%s` has no unique "
                         "instantiation; an expected type determines it "
                         "(Clause 7.6.4)", ref->symbol->name);
            }
            return ck_ivar(c);
        }
        default:
            return ck_ivar(c); /* modules, externals, members: opacity */
        }
    }
    {
        const ek_type *t = name ? ck_scope_get(c, name) : NULL;

        return t ? t : ck_ivar(c);
    }
}

const ek_type *ck_block(ck_ctx *c, const ek_node *b, const ek_type *expected)
{
    const ek_type *t = ck_unit(c);
    size_t i, n = b->u.kids.count;

    ck_scope_push(c);
    for (i = 0; i < n; i++) {
        const ek_node *m = b->u.kids.data[i];

        switch (m->kind) {
        case EK_N_LET:
        case EK_N_REBIND:
        case EK_N_INVERT:
            t = ck_stmt(c, m, 0);
            break;
        default:
            t = ck_expr(c, m, i + 1 == n ? expected : NULL);
            break;
        }
    }
    ck_scope_pop(c);
    ck_row(c, b, t ? t : ck_unit(c), EK_RD_NONE);
    return t ? t : ck_unit(c);
}

const ek_type *ck_expr(ck_ctx *c, const ek_node *e, const ek_type *expected)
{
    const ek_type *t;

    if (c->oom || !e) {
        return expected;
    }
    switch (e->kind) {
    case EK_N_INT_LIT:
    case EK_N_FLOAT_LIT:
    case EK_N_CHAR_LIT:
        t = ck_place(c, e, expected, 0);
        break;
    case EK_N_STRING_LIT: {
        size_t i;

        for (i = 0; i < e->u.str.count; i++) {
            ck_expr(c, e->u.str.segs[i].expr, NULL);
        }
        t = ck_std_type(c, "String");
        break;
    }
    case EK_N_NAME:
        t = name_type(c, e, expected);
        break;
    case EK_N_FIELD:
        t = ck_field(c, e);
        break;
    case EK_N_CALL:
        return ck_call(c, e, expected);
    case EK_N_APPLY:
        return ck_apply(c, e, expected);
    case EK_N_PAREN:
        t = ck_expr(c, e->u.wrap.inner, expected);
        break;
    case EK_N_UNARY:
        t = ck_unary(c, e, expected);
        break;
    case EK_N_BINARY:
        t = ck_binary(c, e, expected);
        break;
    case EK_N_LAMBDA:
        return ck_lambda(c, e, expected);
    case EK_N_IF:
        return ck_if(c, e, expected);
    case EK_N_CASE:
        return ck_case(c, e, expected);
    case EK_N_BLOCK:
        return ck_block(c, e, expected);
    case EK_N_COMPOSITE:
        t = ck_composite(c, e, expected, 0);
        break;
    default:
        return expected;
    }
    ck_row(c, e, t, EK_RD_NONE);
    return t;
}
