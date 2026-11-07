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

static const ek_type *undetermined_in(ck_ctx *c, const ek_type *t);

/* The first plain inference variable among n types. */
static const ek_type *undetermined_among(ck_ctx *c,
                                         const ek_type *const *ts, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        const ek_type *v = undetermined_in(c, ts[i]);

        if (v) {
            return v;
        }
    }
    return NULL;
}

/* The first plain inference variable of a type. A rigid variable and
   a bridge-marked one are legal residents; a plain one that survived
   the module's checking was never determined. */
static const ek_type *undetermined_in(ck_ctx *c, const ek_type *t)
{
    size_t i;

    t = ck_prune(t);
    if (!t) {
        return NULL;
    }
    if (t->kind == EK_T_IVAR) {
        return ck_ivar_opaque(c, t) ? NULL : t;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        return undetermined_among(c, t->u.nominal.args, t->u.nominal.argc);
    case EK_T_LIST:
        return undetermined_in(c, t->u.list.elem);
    case EK_T_DICT: {
        const ek_type *v = undetermined_in(c, t->u.dict.key);

        return v ? v : undetermined_in(c, t->u.dict.val);
    }
    case EK_T_TUPLE:
        return undetermined_among(c, t->u.tuple.items, t->u.tuple.count);
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            const ek_type *v =
                undetermined_in(c, t->u.record.fields[i].type);

            if (v) {
                return v;
            }
        }
        return NULL;
    case EK_T_FN: {
        const ek_type *v =
            undetermined_among(c, t->u.fn.params, t->u.fn.count);

        return v ? v : undetermined_in(c, t->u.fn.result);
    }
    default:
        return NULL;
    }
}

/* One diagnostic per surviving plain inference variable, at the
   first row of the module's slice that shows it. */
static void ck_undetermined(ck_ctx *c, size_t from)
{
    ck_map seen;
    size_t i;

    memset(&seen, 0, sizeof seen);
    for (i = from; i < c->nrows && !c->oom; i++) {
        const ek_type *v = undetermined_in(c, c->rows[i].type);

        if (v && !ck_map_get(&seen, v)) {
            ck_map_put(c, &seen, v, (const void *)1);
            ck_error(c, c->rows[i].node, "E-undetermined",
                     "the type %s keeps a variable the arguments and "
                     "the expected type never determine (Clause 7.6.4)",
                     ck_type_name(c, c->rows[i].type));
        }
    }
}

/* The composite literals placed under an undetermined expectation:
   substitution had its chance. One whose family stayed undecided has
   no lowering, so the checker rejects it here, in its own file; one
   whose family was decided after the opaque placement reads the items
   reads them again under that family now, with the row table
   answering each expression's type. */
static void sweep_literals(ck_ctx *c)
{
    size_t i;

    for (i = 0; i < c->npend; i++) {
        const ek_node *n = c->pend_lits[i].node;
        const ek_ck_row *row = ck_row_of(c, n);
        const ek_type *t = row ? ck_prune(row->type) : NULL;

        if (!t || t->kind == EK_T_IVAR || t->kind == EK_T_TVAR) {
            ek_diag_emit(c->diag, EK_SEV_ERROR, "E-literal-expected",
                         c->pend_lits[i].file, n->line, n->col, "%s",
                         "a composite literal never stands alone; it "
                         "requires an expected type or an annotation "
                         "(Clause 7.5.3)");
            continue;
        }
        c->cur = c->pend_lits[i].mod;
        c->sweep_recheck = 1;
        ck_composite(c, n, t, 0);
        c->sweep_recheck = 0;
    }
}

/* The binding rows and declared types of every module are in place
   before any body is checked: a body reaches across the closure, and
   the tables are keyed by the closure's symbols. The entry rules read
   the closure once. */
static void prebind_modules(ck_ctx *c, const ek_resolve_result *in)
{
    size_t i;

    for (i = 0; i < in->module_count && !c->oom; i++) {
        const ek_node *mod = in->modules[i].ast;

        if (!mod || mod->kind != EK_N_MODULE) {
            continue;
        }
        c->cur = &in->modules[i];
        ck_prebind(c);
        ck_decls(c);
        if (strcmp(in->modules[i].module, "cap") == 0) {
            /* The injected pair's symbols carry no declaration, so
               their schemes hang before any body reads them (Clause
               10.8). */
            ck_cap_schemes(c);
        }
    }
    ck_entry_rules(c);
}

/* Every body of the closure, root first: each module's pass checks
   its bodies and externs, then rejects the variables its checking
   left undetermined. */
static void check_bodies(ck_ctx *c, const ek_resolve_result *in)
{
    size_t i;

    for (i = 0; i < in->module_count && !c->oom; i++) {
        const ek_node *mod = in->modules[i].ast;
        size_t from = c->nrows;

        if (!mod || mod->kind != EK_N_MODULE) {
            continue;
        }
        c->cur = &in->modules[i];
        check_module(c, mod);
        ck_externs(c);
        ck_undetermined(c, from);
        c->locals = NULL;
    }
}

ek_check_result ek_check(ek_resolve_result in, ek_arena *arena,
                         ek_diag *diag)
{
    ck_ctx c;
    ek_check_result r;

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
    prebind_modules(&c, &in);
    c.locals = NULL;
    check_bodies(&c, &in);
    /* The instantiation graph is complete once every body is checked;
       the instance-set simulation reports a non-terminating
       expansion (Clause 8.7.3). */
    ck_inst_depth(&c);
    ck_reqs_check(&c);
    if (c.nrows) {
        finish_rows(&c);
    }
    sweep_literals(&c);
    r.rows = c.rows;
    r.count = c.nrows;
    return r;
}
