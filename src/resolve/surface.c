#include <string.h>

#include "resolve/internal.h"
#include "support/buf.h"

/* The top-level surface of one module. Types are collected first so
   namespace ownership does not depend on source order. Duplicate names
   are conflicts; a namespace binding is keyed by its full head, so
   `Int.add` and `Float.add` coexist. */

typedef struct {
    const char *key;
    int line, col;
} r_key;

typedef struct {
    r_ctx *c;
    r_mod *m;
    ek_symbol *syms;
    size_t n;
    r_key *tkeys; size_t nt, ctk;
    r_key *lkeys; size_t nl, clk;
    r_key *nkeys; size_t nn, cnk;
} r_surf;

/* An upper bound: every top-level item yields at most one symbol per
   variant plus one. */
static size_t bound(const ek_node *mod)
{
    size_t total = 2, i;

    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *d = mod->u.kids.data[i];

        total += 2;
        if (d->kind == EK_N_TYPE_DECL) {
            total += d->u.type_decl.variants.count;
        }
    }
    return total;
}

static void key_push(r_surf *s, r_key **keys, size_t *n, size_t *cap,
                     const char *key, int line, int col)
{
    if (s->c->oom) {
        return;
    }
    if (*n == *cap) {
        *keys = r_grow(s->c, *keys, *n, cap, sizeof **keys);
        if (s->c->oom) {
            return;
        }
    }
    (*keys)[*n].key = key;
    (*keys)[*n].line = line;
    (*keys)[*n].col = col;
    (*n)++;
}

/* Returns the index of an earlier equal key, or -1. */
static int key_seen(r_key *keys, size_t n, const char *key)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (strcmp(keys[i].key, key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void emit(r_surf *s, ek_symbol *sym)
{
    if (s->c->oom || !sym) {
        return;
    }
    s->syms[s->n++] = *sym;
}

/* A constructor of the name already emitted for the type declaration
   whose first symbol sits at first. */
static int variant_seen(const r_surf *s, size_t first, const char *name)
{
    size_t i;

    for (i = first; i < s->n; i++) {
        if (s->syms[i].kind == EK_SYM_CONSTRUCTOR
            && strcmp(s->syms[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* A declared type or ctype: one symbol of the identity layer. */
static void collect_type(r_surf *s, const ek_node *d)
{
    const char *name = d->kind == EK_N_TYPE_DECL
                           ? d->u.type_decl.name : d->u.decl.name;
    ek_symbol *sym = r_sym(s->c, name, EK_SYM_TYPE,
                           s->m->stem, d->line, d->col);
    size_t i, first = s->n;
    int seen;

    if (!sym) {
        return;
    }
    sym->decl = d;
    sym->public = name[0] != '_';
    seen = key_seen(s->tkeys, s->nt, name);
    if (seen >= 0) {
        r_error(s->c, s->m->file, d->line, d->col, "E-name-conflict",
                "`%s` is declared twice in this module (Clause 7.4.6)",
                name);
    } else {
        key_push(s, &s->tkeys, &s->nt, &s->ctk, name, d->line, d->col);
    }
    emit(s, sym);
    if (d->kind != EK_N_TYPE_DECL) {
        return;
    }
    for (i = 0; i < d->u.type_decl.variants.count; i++) {
        const ek_node *v = d->u.type_decl.variants.data[i];
        ek_symbol *cs;

        if (variant_seen(s, first, v->u.variant.name)) {
            r_error(s->c, s->m->file, v->line, v->col, "E-name-conflict",
                    "the constructor `%s` is declared twice in the type "
                    "`%s` (Clause 7.4.6)", v->u.variant.name, name);
            continue;
        }
        cs = r_sym(s->c, v->u.variant.name, EK_SYM_CONSTRUCTOR,
                   s->m->stem, v->line, v->col);

        if (!cs) {
            return;
        }
        cs->decl = d;
        cs->owner = d->u.type_decl.name;
        cs->public = sym->public;
        emit(s, cs);
    }
}

/* The full head of a namespace binding, segments joined with `.`:
   `Type.name`, or the qualified spelling `a.Type.name`. NULL when the
   key cannot be built; the context records the failure. */
static char *ns_key(r_surf *s, const ek_node *d)
{
    ek_buf b;
    char *out = NULL;
    int ok = 1;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; ok && i < d->u.namespace.owner.count; i++) {
        if (b.len) {
            ok = ek_buf_append_char(&b, '.');
        }
        ok = ok && ek_buf_append_str(&b, d->u.namespace.owner.data[i]);
    }
    if (ok && b.len) {
        ok = ek_buf_append_char(&b, '.');
    }
    ok = ok && ek_buf_append_str(&b, d->u.namespace.name);
    if (ok) {
        out = ek_arena_alloc(s->c->arena, b.len + 1);
        if (out) {
            memcpy(out, b.data ? b.data : "", b.len + 1);
        }
    }
    if (!out) {
        s->c->oom = 1;
    }
    ek_buf_destroy(&b);
    return out;
}

/* A namespace member is legal only in the module that declares the
   type; the qualified spelling does not bypass it. */
static int type_owned(r_surf *s, const char *type)
{
    size_t i;

    for (i = 0; i < s->n; i++) {
        if (s->syms[i].kind == EK_SYM_TYPE
            && strcmp(s->syms[i].name, type) == 0) {
            return 1;
        }
    }
    return 0;
}

static void leaf_push(r_surf *s, ek_symbol *sym)
{
    if (!sym) {
        return;
    }
    if (key_seen(s->lkeys, s->nl, sym->name) >= 0) {
        r_error(s->c, s->m->file, sym->line, sym->col, "E-name-conflict",
                "`%s` is declared twice in this module (Clause 7.4.6)",
                sym->name);
        return;
    }
    key_push(s, &s->lkeys, &s->nl, &s->clk, sym->name, sym->line, sym->col);
}

/* A namespace binding: full-head key, ownership, member case. */
static ek_symbol *collect_namespace(r_surf *s, const ek_node *item)
{
    const char *type = item->u.namespace.owner.count
        ? item->u.namespace.owner.data[item->u.namespace.owner.count - 1]
        : item->u.namespace.name;
    const char *name = item->u.namespace.name;
    ek_symbol *sym;
    char *key;

    if (!r_is_snake(name)) {
        r_error(s->c, s->m->file, item->line, item->col, "E-name-case",
                "namespace member `%s` is not snake_case "
                "(Clauses 5.5.2, 7.4.1)", name);
    }
    if (!type_owned(s, type)) {
        r_error(s->c, s->m->file, item->line, item->col,
                "E-namespace-owner",
                "type namespace `%s` is owned by another module "
                "(Clause 7.4.5)", type);
    }
    sym = r_sym(s->c, name, EK_SYM_NAMESPACE, s->m->stem, item->line,
                item->col);
    if (sym) {
        sym->decl = item;
        sym->owner = type;
        sym->type = item->u.namespace.type;
        sym->public = name[0] != '_';
    }
    key = ns_key(s, item);
    if (key && key_seen(s->nkeys, s->nn, key) >= 0) {
        r_error(s->c, s->m->file, item->line, item->col,
                "E-name-conflict",
                "namespace member `%s` is declared twice in this "
                "module (Clause 7.4.6)", key);
        sym = NULL;
    }
    if (key) {
        key_push(s, &s->nkeys, &s->nn, &s->cnk, key, item->line, item->col);
    }
    return sym;
}

/* A plain value binding; a PascalCase spelling violates the binding
   space. */
static ek_symbol *collect_let(r_surf *s, const ek_node *item)
{
    const ek_node *pat = item->u.bind.pattern;
    const char *name = NULL;
    ek_symbol *sym;

    if (pat && pat->kind == EK_N_P_VAR && pat->u.path.count == 1) {
        name = pat->u.path.data[0];
    } else if (pat && pat->kind == EK_N_P_CONS
               && pat->u.cons.name.count == 1) {
        r_error(s->c, s->m->file, pat->line, pat->col, "E-name-case",
                "value binding `%s` is not snake_case "
                "(Clauses 5.5.2, 7.4.1)", pat->u.cons.name.data[0]);
        name = pat->u.cons.name.data[0];
    }
    if (!name) {
        return NULL;
    }
    sym = r_sym(s->c, name, EK_SYM_FUNC, s->m->stem, item->line, item->col);
    if (sym) {
        sym->decl = item;
        sym->type = item->u.bind.type;
    }
    return sym;
}

static void collect_member(r_surf *s, const ek_node *item)
{
    ek_symbol *sym = NULL;

    if (item->kind == EK_N_LET) {
        sym = collect_let(s, item);
    } else if (item->kind == EK_N_NAMESPACE) {
        sym = collect_namespace(s, item);
    } else if (item->kind == EK_N_TEST_DECL) {
        sym = r_sym(s->c, item->u.decl.name, EK_SYM_TEST, s->m->stem,
                    item->line, item->col);
        if (sym) {
            sym->decl = item;
            sym->public = 0; /* tests never enter the interface */
        }
    } else if (item->kind == EK_N_EXTERN_DECL) {
        sym = r_sym(s->c, item->u.extern_decl.name, EK_SYM_EXTERN,
                    s->m->stem, item->line, item->col);
        if (sym) {
            sym->decl = item;
            sym->type = item->u.extern_decl.type;
            sym->extern_symbol = item->u.extern_decl.symbol;
            sym->pure = item->u.extern_decl.pure;
        }
    }
    /* Namespace members are keyed by their full head and never occupy
       the leaf layer. */
    if (!sym || sym->kind != EK_SYM_NAMESPACE) {
        leaf_push(s, sym);
    }
    emit(s, sym);
}

/* N12: an import and a file top-level binding share a name; the
   implicit std import counts as the import. */
static void binding_vs_import(r_surf *s, const char *stem)
{
    size_t k;

    for (k = 0; k < s->nl; k++) {
        if (strcmp(s->lkeys[k].key, stem) == 0) {
            r_error(s->c, s->m->file, s->lkeys[k].line, s->lkeys[k].col,
                    "E-name-conflict",
                    "top-level binding `%s` collides with the import of "
                    "module `%s` (Clause 7.4.6)", stem, stem);
        }
    }
}

static void import_conflicts(r_surf *s)
{
    const ek_node *mod = s->m->ast;
    size_t i;

    binding_vs_import(s, "std");
    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *imp = mod->u.kids.data[i];

        if (imp->kind != EK_N_IMPORT || imp->u.imports.path.count == 0) {
            continue;
        }
        binding_vs_import(s, imp->u.imports.path.data[imp->u.imports.path.count - 1]);
    }
}

void r_surface_build(r_ctx *c, r_mod *m)
{
    r_surf s;
    const ek_node *mod;
    size_t i;

    if (c->oom || !m->ast) {
        return;
    }
    memset(&s, 0, sizeof s);
    s.c = c;
    s.m = m;
    s.syms = ek_arena_alloc(c->arena, bound(m->ast) * sizeof *s.syms);
    if (!s.syms) {
        c->oom = 1;
        return;
    }
    m->surf = ek_arena_alloc(c->arena, sizeof *m->surf);
    if (!m->surf) {
        c->oom = 1;
        return;
    }
    m->surf->module = m->stem;
    m->surf->file = m->file;
    mod = m->ast;
    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *d = mod->u.kids.data[i];

        if (d->kind == EK_N_TYPE_DECL || d->kind == EK_N_CTYPE_DECL) {
            collect_type(&s, d);
        }
    }
    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *d = mod->u.kids.data[i];

        if (d->kind != EK_N_IMPORT && d->kind != EK_N_TYPE_DECL) {
            collect_member(&s, d);
        }
    }
    import_conflicts(&s);
    m->surf->syms = s.syms;
    m->surf->count = s.n;
}
