#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check/internal.h"

/* The entry point. One ek_check call checks every module of the
   closure, root first: a compilation is the whole program, so an
   imported module's bodies are checked in the same pass, under their
   own file. The rows come
   back sorted by node address with inference variables substituted
   away (check.h). The growable buffers and pointer maps of the shared
   context live here too. */

/* FNV-1a over the pointer bytes; the map layer here and the intern
   table in store.c share it. */
size_t ck_ptr_hash(const void *p)
{
    const unsigned char *b = (const unsigned char *)&p;
    size_t h = 2166136261u, i;

    for (i = 0; i < sizeof p; i++) {
        h = (h ^ b[i]) * 16777619u;
    }
    return h;
}

void *ck_grow(ck_ctx *c, void *old, size_t n, size_t *cap, size_t elem)
{
    size_t ncap = *cap ? *cap * 2 : 8;
    void *grown;

    if (n < *cap) {
        return old;
    }
    while (ncap < n) {
        ncap *= 2;
    }
    grown = ek_arena_alloc(c->arena, ncap * elem);
    if (!grown) {
        c->oom = 1;
        return old;
    }
    if (old) {
        /* The old capacity, not the request: a caller may ask for one
           past the filled length. */
        memcpy(grown, old, *cap * elem);
    }
    *cap = ncap;
    return grown;
}

void ck_map_put(ck_ctx *c, ck_map *m, const void *key, const void *val)
{
    size_t i;

    if (c->oom || !key) {
        return;
    }
    if (m->count * 2 >= m->cap) {
        size_t ncap = m->cap ? m->cap * 2 : 32;
        const void **nk = ek_arena_alloc(c->arena, ncap * sizeof *nk);
        const void **nv = ek_arena_alloc(c->arena, ncap * sizeof *nv);

        if (!nk || !nv) {
            c->oom = 1;
            return;
        }
        memset(nk, 0, ncap * sizeof *nk);
        for (i = 0; i < m->cap; i++) {
            if (m->keys && m->keys[i]) {
                size_t j = ck_ptr_hash(m->keys[i]) & (ncap - 1);

                while (nk[j]) {
                    j = (j + 1) & (ncap - 1);
                }
                nk[j] = m->keys[i];
                nv[j] = m->vals[i];
            }
        }
        m->keys = nk;
        m->vals = nv;
        m->cap = ncap;
    }
    {
        size_t j = ck_ptr_hash(key) & (m->cap - 1);

        while (m->keys[j]) {
            if (m->keys[j] == key) {
                m->vals[j] = val;
                return;
            }
            j = (j + 1) & (m->cap - 1);
        }
        m->keys[j] = key;
        m->vals[j] = val;
        m->count++;
    }
}

const void *ck_map_get(const ck_map *m, const void *key)
{
    size_t j;

    if (!m->cap || !key) {
        return NULL;
    }
    j = ck_ptr_hash(key) & (m->cap - 1);
    while (m->keys[j]) {
        if (m->keys[j] == key) {
            return m->vals[j];
        }
        j = (j + 1) & (m->cap - 1);
    }
    return NULL;
}

static int row_cmp(const void *a, const void *b)
{
    const ek_ck_row *x = a, *y = b;

    if ((size_t)x->node < (size_t)y->node) {
        return -1;
    }
    return x->node == y->node ? 0 : 1;
}

/* One module's body: binding forms through ck_stmt, everything
   else as an expression; type declarations and imports were built by
   ck_decls. */
static void check_module(ck_ctx *c, const ek_node *mod)
{
    size_t i, n = mod->u.kids.count;

    for (i = 0; i < n && !c->oom; i++) {
        const ek_node *d = mod->u.kids.data[i];

        switch (d->kind) {
        case EK_N_LET:
        case EK_N_REBIND:
        case EK_N_INVERT:
        case EK_N_NAMESPACE:
        case EK_N_TEST_DECL:
        case EK_N_EXTERN_DECL:
            ck_stmt(c, d, 1);
            break;
        case EK_N_TYPE_DECL:
        case EK_N_CTYPE_DECL:
        case EK_N_IMPORT:
            break; /* built by ck_decls; imports carry no body */
        default:
            ck_expr(c, d, NULL);
            break;
        }
    }
}

ek_check_result ek_check(ek_resolve_result in, ek_arena *arena,
                         ek_diag *diag)
{
    ck_ctx c;
    ek_check_result r;
    size_t i;

    memset(&c, 0, sizeof c);
    memset(&r, 0, sizeof r);
    c.arena = arena;
    c.diag = diag;
    c.in = &in;
    if (!in.modules || !in.module_count) {
        return r;
    }
    c.root = &in.modules[0];
    c.cur = c.root;

    /* The binding rows and declared types of every module are in place
       before any body is checked: a body reaches across the closure,
       and the tables are keyed by the closure's symbols. The entry
       rules read the closure once. */
    for (i = 0; i < in.module_count && !c.oom; i++) {
        const ek_node *mod = in.modules[i].ast;

        if (!mod || mod->kind != EK_N_MODULE) {
            continue;
        }
        c.cur = &in.modules[i];
        ck_prebind(&c);
        ck_decls(&c);
    }
    ck_entry_rules(&c);
    c.locals = NULL;
    for (i = 0; i < in.module_count && !c.oom; i++) {
        const ek_node *mod = in.modules[i].ast;

        if (!mod || mod->kind != EK_N_MODULE) {
            continue;
        }
        c.cur = &in.modules[i];
        check_module(&c, mod);
        ck_externs(&c);
        c.locals = NULL;
    }
    if (c.nrows) {
        qsort(c.rows, c.nrows, sizeof *c.rows, row_cmp);
        for (i = 0; i < c.nrows; i++) {
            c.rows[i].type = ck_subst(&c, c.rows[i].type);
        }
        ck_fill_bits(&c);
    }
    r.rows = c.rows;
    r.count = c.nrows;
    return r;
}

void ck_row(ck_ctx *c, const ek_node *n, const ek_type *t, ek_reading r)
{
    if (c->oom || !n) {
        return;
    }
    if (c->nrows == c->crows) {
        c->rows = ck_grow(c, c->rows, c->nrows, &c->crows, sizeof *c->rows);
        if (c->oom) {
            return;
        }
    }
    c->rows[c->nrows].node = n;
    c->rows[c->nrows].type = t;
    c->rows[c->nrows].reading = r;
    c->rows[c->nrows].bit = EK_BIT_UNSET;
    c->nrows++;
}

static void row_diag(ck_ctx *c, ek_severity sev, const ek_node *n,
                     const char *code, const char *fmt, va_list ap)
{
    char msg[400];

    vsnprintf(msg, sizeof msg, fmt, ap);
    ek_diag_emit(c->diag, sev, code, ck_file(c),
                 n ? n->line : 1, n ? n->col : 1, "%s", msg);
}

void ck_error(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
              ...)
{
    va_list ap;

    if (c->oom) {
        return;
    }
    va_start(ap, fmt);
    row_diag(c, EK_SEV_ERROR, n, code, fmt, ap);
    va_end(ap);
}

void ck_warn(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
             ...)
{
    va_list ap;

    if (c->oom) {
        return;
    }
    va_start(ap, fmt);
    row_diag(c, EK_SEV_WARNING, n, code, fmt, ap);
    va_end(ap);
}

const char *ck_file(const ck_ctx *c)
{
    if (c->cur) {
        return c->cur->file;
    }
    return c->in->module_count ? c->root->file : "test.eko";
}
