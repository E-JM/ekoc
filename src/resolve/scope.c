#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "resolve/internal.h"

void *r_grow(r_ctx *c, void *old, size_t n, size_t *cap, size_t elem)
{
    size_t ncap = *cap ? *cap * 2 : 8;
    void *p;

    if (!c->oom && n < *cap) {
        return old;
    }
    p = ek_arena_alloc(c->arena, ncap * elem);
    if (!p) {
        c->oom = 1;
        return old;
    }
    if (old && n) {
        memcpy(p, old, n * elem);
    }
    *cap = ncap;
    return p;
}

/* Clause 5.5.2: PascalCase carries identity; the `_` privacy prefix is
   not part of the case class. */
int r_is_pascal(const char *s)
{
    while (*s == '_') {
        s++;
    }
    return s[0] >= 'A' && s[0] <= 'Z' && !strchr(s, '_');
}

int r_is_snake(const char *s)
{
    return !r_is_pascal(s);
}

ek_symbol *r_sym(r_ctx *c, const char *name, ek_sym_kind kind,
                 const char *module, int line, int col)
{
    ek_symbol *s = ek_arena_alloc(c->arena, sizeof *s);

    if (!s) {
        c->oom = 1;
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->name = name;
    s->kind = kind;
    s->public = r_is_snake(name) ? name[0] != '_' : 1;
    s->module = module;
    s->line = line;
    s->col = col;
    return s;
}

scope *r_scope_new(r_ctx *c, scope *parent)
{
    scope *s = ek_arena_alloc(c->arena, sizeof *s);

    if (!s) {
        c->oom = 1;
        return parent;
    }
    s->parent = parent;
    s->vars = NULL;
    s->count = 0;
    s->cap = 0;
    return s;
}

void r_scope_add(r_ctx *c, scope *s, const char *name, const ek_symbol *sym)
{
    if (c->oom) {
        return;
    }
    if (s->count == s->cap) {
        s->vars = r_grow(c, s->vars, s->count, &s->cap, sizeof *s->vars);
        if (c->oom) {
            return;
        }
    }
    s->vars[s->count].name = name;
    s->vars[s->count].sym = sym;
    s->count++;
}

const ek_symbol *r_scope_get(const scope *s, const char *name)
{
    size_t i;

    for (; s; s = s->parent) {
        for (i = 0; i < s->count; i++) {
            if (strcmp(s->vars[i].name, name) == 0) {
                return s->vars[i].sym;
            }
        }
    }
    return NULL;
}

void r_add_ref(r_ctx *c, const ek_node *n, ek_sym_kind kind,
               ek_station st, const ek_symbol *sym)
{
    if (c->oom || !n) {
        return;
    }
    if (c->nrefs == c->crefs) {
        c->refs = r_grow(c, c->refs, c->nrefs, &c->crefs, sizeof *c->refs);
        if (c->oom) {
            return;
        }
    }
    c->refs[c->nrefs].node = n;
    c->refs[c->nrefs].kind = kind;
    c->refs[c->nrefs].station = st;
    c->refs[c->nrefs].symbol = sym;
    c->nrefs++;
}

void r_error(r_ctx *c, const char *file, int line, int col,
             const char *code, const char *fmt, ...)
{
    va_list ap;

    if (c->oom) {
        return;
    }
    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, code, file, line, col, fmt, ap);
    va_end(ap);
}

static int ref_cmp(const void *a, const void *b)
{
    const ek_ref *x = a;
    const ek_ref *y = b;

    if (x->node < y->node) {
        return -1;
    }
    return x->node > y->node;
}

void r_refs_sort(ek_ref *refs, size_t count)
{
    if (count > 1) {
        qsort(refs, count, sizeof *refs, ref_cmp);
    }
}
