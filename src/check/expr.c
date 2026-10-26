#include <string.h>

#include "check/internal.h"
#include "support/buf.h"

/* Expression typing: the lexical scopes, the surface schemes, the
   name readings, and the dispatch to the per-concern files (operators,
   literals, calls, patterns, control forms). */

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

/* The scheme of a surface symbol, built once from its annotation and
   instantiated per use. */
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

/* Constructor candidates: the reference carries every visible
   same-named constructor; the expected nominal type or the scrutinee
   type picks one. Ownership is the declaring module plus the owner's
   spelling, which a std placeholder ctor and its std type share. */
static int cand_owned(const ek_symbol *cand, const ek_symbol *type)
{
    return cand && type && cand->owner && cand->module && type->module
        && strcmp(cand->owner, type->name) == 0
        && strcmp(cand->module, type->module) == 0;
}

const ek_symbol *ck_ctor_pick(const ek_ref *ref, const ek_type *expected)
{
    const ek_type *exp = ck_prune(expected);
    size_t i;

    if (!ref || !ref->cands || !exp || exp->kind != EK_T_NOMINAL
        || !exp->u.nominal.sym) {
        return NULL;
    }
    for (i = 0; i < ref->ncands; i++) {
        if (cand_owned(ref->cands[i], exp->u.nominal.sym)) {
            return ref->cands[i];
        }
    }
    return NULL;
}

/* The std placeholders carry no declaration, so they are never
   generic; a declared owner with binders is. */
int ck_ctor_generic(ck_ctx *c, const ek_symbol *cand)
{
    const ck_ctor *ct = ck_ctor_of(c, cand);
    const ck_decl *d = ct && ct->owner ? ck_decl_of(c, ct->owner) : NULL;

    return d && d->nbinders > 0;
}

static const char *count_word(size_t n)
{
    switch (n) {
    case 2:
        return "two";
    case 3:
        return "three";
    default:
        return NULL;
    }
}

/* The owners of the candidates, joined for a diagnostic. */
static char *owners_text(ck_ctx *c, const ek_ref *ref)
{
    ek_buf b;
    char *out = NULL;
    int ok = 1;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; ok && i < ref->ncands; i++) {
        const ek_symbol *cand = ref->cands[i];

        if (i) {
            ok = ek_buf_append_str(&b, " or ");
        }
        if (ok) {
            ok = ek_buf_append_str(&b, cand->owner ? cand->owner : "?");
        }
    }
    if (ok && b.len) {
        out = ek_arena_alloc(c->arena, b.len + 1);
        if (out) {
            memcpy(out, b.data ? b.data : "", b.len + 1);
        }
    }
    if (!out) {
        c->oom = 1;
    }
    ek_buf_destroy(&b);
    return out;
}

/* Several candidates and no anchor: name them, since an annotation
   over any one of them resolves the reference. */
static void undetermined_many(ck_ctx *c, const ek_node *n, const ek_ref *ref)
{
    const char *owners = owners_text(c, ref);
    const char *name = ref->symbol ? ref->symbol->name : "?";
    const char *word = count_word(ref->ncands);

    if (!owners) {
        return;
    }
    if (word) {
        ck_error(c, n, "E-undetermined",
                 "%s constructors named `%s` are visible and no expected "
                 "type anchors the reference; write a `%s` annotation "
                 "(Clause 7.4.6)", word, name, owners);
        return;
    }
    ck_error(c, n, "E-undetermined",
             "%zu constructors named `%s` are visible and no expected "
             "type anchors the reference; write a `%s` annotation "
             "(Clause 7.4.6)", ref->ncands, name, owners);
}

ck_ctorsel ck_ctor_select(ck_ctx *c, const ek_node *n, const ek_ref *ref,
                          const ek_type *expected)
{
    ck_ctorsel out;
    const ek_type *exp = ck_prune(expected);
    const ek_symbol *pick;

    memset(&out, 0, sizeof out);
    if (exp && exp->kind == EK_T_NOMINAL && exp->u.nominal.sym) {
        pick = ck_ctor_pick(ref, exp);
        if (pick) {
            out.sym = pick;
            out.anchored = 1;
            return out;
        }
        ck_error(c, n, ck_conform_code(CK_NOMINAL),
                 "the constructor `%s` does not match the expected type "
                 "%s (Clause 7.5.2)",
                 ref->symbol ? ref->symbol->name : "?",
                 ck_type_name(c, exp));
        return out;
    }
    if (ref->ncands == 1 && !ck_ctor_generic(c, ref->cands[0])) {
        out.sym = ref->cands[0];
        return out;
    }
    if (ref->ncands == 1) {
        ck_error(c, n, "E-undetermined",
                 "the constructor `%s` has no unique instantiation; an "
                 "expected type determines it (Clause 7.6.4)",
                 ref->symbol ? ref->symbol->name : "?");
    } else if (ref->ncands > 1) {
        undetermined_many(c, n, ref);
    }
    return out;
}

/* A constructor in value position: the expected nominal type selects
   among the candidates, a sole non-generic constructor self-anchors,
   and a std placeholder stays the opaque bridge. */
static const ek_type *ctor_type(ck_ctx *c, const ek_node *n,
                                const ek_ref *ref, const ek_type *expected)
{
    ck_ctorsel sel;
    const ck_ctor *ct;
    const ek_type *exp = ck_prune(expected);

    if (!ref->ncands) {
        /* A qualified constructor names one declaration. */
        ct = ref->symbol ? ck_ctor_of(c, ref->symbol) : NULL;
        if (ct && ct->owner) {
            const ck_decl *d = ck_decl_of(c, ct->owner);

            if (!d || d->nbinders == 0) {
                return ck_nominal(c, ct->owner, NULL, 0);
            }
            if (exp && exp->kind == EK_T_NOMINAL
                && exp->u.nominal.sym == ct->owner) {
                return exp; /* the expected type anchors it */
            }
            ck_error(c, n, "E-undetermined",
                     "the constructor `%s` has no unique "
                     "instantiation; an expected type determines it "
                     "(Clause 7.6.4)", ref->symbol->name);
        }
        return ck_ivar(c);
    }
    sel = ck_ctor_select(c, n, ref, expected);
    if (!sel.sym) {
        return ck_ivar(c);
    }
    if (sel.sym->decl == NULL) {
        /* A std placeholder: opaque, conforming into an anchor. */
        const ek_type *iv = ck_ivar(c);

        if (expected) {
            ck_conforms(c, iv, expected, 1);
        }
        return iv;
    }
    if (sel.anchored) {
        return exp;
    }
    ct = ck_ctor_of(c, sel.sym);
    if (!ct || !ct->owner) {
        return ck_ivar(c);
    }
    return ck_nominal(c, ct->owner, NULL, 0);
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
            if (ref->ncands) {
                return ctor_type(c, n, ref, expected);
            }
            return ck_ivar(c);
        case EK_SYM_CONSTRUCTOR:
            return ctor_type(c, n, ref, expected);
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

/* The literal cases: placed at the expected type, an interpolated
   string's segments are expressions. */
static const ek_type *literal_expr(ck_ctx *c, const ek_node *e,
                                   const ek_type *expected)
{
    switch (e->kind) {
    case EK_N_INT_LIT:
    case EK_N_FLOAT_LIT:
    case EK_N_CHAR_LIT:
        return ck_place(c, e, expected, 0);
    case EK_N_STRING_LIT: {
        size_t i;

        for (i = 0; i < e->u.str.count; i++) {
            ck_expr(c, e->u.str.segs[i].expr, NULL);
        }
        return ck_std_type(c, "String");
    }
    default:
        return NULL; /* not a literal */
    }
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
    case EK_N_STRING_LIT:
        t = literal_expr(c, e, expected);
        break;
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
