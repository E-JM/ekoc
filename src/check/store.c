#include <stdlib.h>
#include <string.h>

#include "check/internal.h"

/* Type storage: hash-consed canonical types. Anonymous composites and
   nominal instantiations are interned, so type identity is pointer
   identity and recursive comparisons terminate (Clause 7.5.1). */

/* One mixed child of a content hash. */
static size_t fold_ptr(size_t h, const void *p)
{
    return h * 31 + ck_ptr_hash(p);
}

/* A content hash over the canonical children of a candidate. */
static size_t type_hash(const ek_type *t)
{
    size_t h = ck_ptr_hash((const void *)(size_t)t->kind), i;

    switch (t->kind) {
    case EK_T_NOMINAL:
        h = fold_ptr(h, t->u.nominal.sym);
        for (i = 0; i < t->u.nominal.argc; i++) {
            h = fold_ptr(h, t->u.nominal.args[i]);
        }
        return h;
    case EK_T_LIST:
        return fold_ptr(h, t->u.list.elem);
    case EK_T_DICT:
        return fold_ptr(fold_ptr(h, t->u.dict.key), t->u.dict.val);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            h = fold_ptr(h, t->u.tuple.items[i]);
        }
        return h;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            h = fold_ptr(fold_ptr(h, t->u.record.fields[i].name),
                         t->u.record.fields[i].type);
        }
        return h;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            h = fold_ptr(h, t->u.fn.params[i]);
        }
        return fold_ptr(fold_ptr(h, t->u.fn.result),
                        (const void *)(size_t)t->u.fn.arrow);
    default:
        return h;
    }
}

/* Pointer equality over two equal-length argument arrays. */
static int slots_eq(const ek_type *const *a, const ek_type *const *b,
                    size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/* Record equality: same fields at the same types (name-sorted at
   construction, 7.5.7). */
static int fields_eq(const ek_type *a, const ek_type *b)
{
    size_t i;

    if (a->u.record.count != b->u.record.count) {
        return 0;
    }
    for (i = 0; i < a->u.record.count; i++) {
        if (strcmp(a->u.record.fields[i].name, b->u.record.fields[i].name)
                != 0
            || a->u.record.fields[i].type != b->u.record.fields[i].type) {
            return 0;
        }
    }
    return 1;
}

/* Function equality: arrow, parameters, and result. */
static int fn_eq(const ek_type *a, const ek_type *b)
{
    return a->u.fn.count == b->u.fn.count
        && a->u.fn.arrow == b->u.fn.arrow
        && a->u.fn.result == b->u.fn.result
        && slots_eq(a->u.fn.params, b->u.fn.params, a->u.fn.count);
}

/* Two candidates are the same type when kind and canonical children
   coincide; record fields are name-sorted at construction (7.5.7). */
static int type_eq(const ek_type *a, const ek_type *b)
{
    if (a->kind != b->kind) {
        return 0;
    }
    switch (a->kind) {
    case EK_T_NOMINAL:
        return a->u.nominal.sym == b->u.nominal.sym
            && a->u.nominal.argc == b->u.nominal.argc
            && slots_eq(a->u.nominal.args, b->u.nominal.args,
                        a->u.nominal.argc);
    case EK_T_LIST:
        return a->u.list.elem == b->u.list.elem;
    case EK_T_DICT:
        return a->u.dict.key == b->u.dict.key
            && a->u.dict.val == b->u.dict.val;
    case EK_T_TUPLE:
        return a->u.tuple.count == b->u.tuple.count
            && slots_eq(a->u.tuple.items, b->u.tuple.items,
                        a->u.tuple.count);
    case EK_T_RECORD:
        return fields_eq(a, b);
    case EK_T_FN:
        return fn_eq(a, b);
    default:
        return 1;
    }
}

/* Rehash: every stored node is canonical, so re-inserting by identity
   preserves the table. */
static void intern_rehash(ck_ctx *c)
{
    size_t ncap = c->types.cap * 4;
    const void **nk = ek_arena_alloc(c->arena, ncap * sizeof *nk);
    const void **nv = ek_arena_alloc(c->arena, ncap * sizeof *nv);
    ck_map old = c->types;
    size_t j;

    if (!nk || !nv) {
        c->oom = 1;
        return;
    }
    memset(nk, 0, ncap * sizeof *nk);
    c->types.keys = nk;
    c->types.vals = nv;
    c->types.cap = ncap;
    c->types.count = 0;
    for (j = 0; j < old.cap; j++) {
        if (old.keys[j]) {
            ck_intern(c, old.vals[j]);
        }
    }
}

/* Canonicalizes a candidate: the table holds one node per identity.
   Inference and rigid variables keep their fresh identity (they are
   never interned). */
const ek_type *ck_intern(ck_ctx *c, const ek_type *t)
{
    size_t h, i;

    if (c->oom || !t) {
        return t;
    }
    if (t->kind == EK_T_IVAR || t->kind == EK_T_TVAR) {
        return t;
    }
    if (!c->types.cap) {
        c->types.cap = 64;
        c->types.keys = ek_arena_alloc(c->arena, 64 * sizeof *c->types.keys);
        c->types.vals = ek_arena_alloc(c->arena, 64 * sizeof *c->types.vals);
        if (!c->types.keys || !c->types.vals) {
            c->oom = 1;
            return t;
        }
        memset(c->types.keys, 0, 64 * sizeof *c->types.keys);
    }
    h = type_hash(t) & (c->types.cap - 1);
    for (i = h; c->types.keys[i]; i = (i + 1) & (c->types.cap - 1)) {
        if (type_eq(c->types.vals[i], t)) {
            return c->types.vals[i];
        }
    }
    c->types.keys[i] = t;
    c->types.vals[i] = t;
    c->types.count++;
    if (c->types.count * 3 >= c->types.cap * 2) {
        intern_rehash(c);
    }
    return t;
}

ek_type *ck_fresh(ck_ctx *c, ek_type_kind kind)
{
    ek_type *t = ek_arena_alloc(c->arena, sizeof *t);

    if (!t) {
        c->oom = 1;
        return NULL;
    }
    memset(t, 0, sizeof *t);
    t->kind = kind;
    return t;
}

const ek_type *ck_ivar(ck_ctx *c)
{
    ek_type *t = ck_fresh(c, EK_T_IVAR);

    if (!t) {
        return NULL;
    }
    t->u.ivar.id = c->next_ivar++;
    return t;
}

/* The kind-valued leaves: `_` and `Never`. */
static const ek_type *ck_leaf(ck_ctx *c, ek_type_kind kind)
{
    return ck_intern(c, ck_fresh(c, kind));
}

const ek_type *ck_unit(ck_ctx *c)
{
    return ck_leaf(c, EK_T_UNIT);
}

const ek_type *ck_never(ck_ctx *c)
{
    return ck_leaf(c, EK_T_NEVER);
}

const ek_type *ck_nominal(ck_ctx *c, const ek_symbol *sym, const ek_type **args,
                          size_t argc)
{
    ek_type *t = ck_fresh(c, EK_T_NOMINAL);

    if (!t) {
        return NULL;
    }
    t->u.nominal.sym = sym;
    t->u.nominal.args = args;
    t->u.nominal.argc = argc;
    return ck_intern(c, t);
}

const ek_type *ck_list(ck_ctx *c, const ek_type *elem)
{
    ek_type *t = ck_fresh(c, EK_T_LIST);

    if (!t) {
        return NULL;
    }
    t->u.list.elem = elem;
    return ck_intern(c, t);
}

const ek_type *ck_dict(ck_ctx *c, const ek_type *key, const ek_type *val)
{
    ek_type *t = ck_fresh(c, EK_T_DICT);

    if (!t) {
        return NULL;
    }
    t->u.dict.key = key;
    t->u.dict.val = val;
    return ck_intern(c, t);
}

const ek_type *ck_tuple(ck_ctx *c, const ek_type **items, size_t n)
{
    ek_type *t = ck_fresh(c, EK_T_TUPLE);

    if (!t) {
        return NULL;
    }
    t->u.tuple.items = items;
    t->u.tuple.count = n;
    return ck_intern(c, t);
}

static int field_order(const void *a, const void *b)
{
    const ek_tfield *f = a, *g = b;

    return strcmp(f->name, g->name);
}

const ek_type *ck_record(ck_ctx *c, ek_tfield *fields, size_t n)
{
    ek_type *t = ck_fresh(c, EK_T_RECORD);

    if (!t) {
        return NULL;
    }
    qsort(fields, n, sizeof *fields, field_order);
    t->u.record.fields = fields;
    t->u.record.count = n;
    return ck_intern(c, t);
}

const ek_type *ck_fn(ck_ctx *c, const ek_type **params, size_t n,
                     const ek_type *result, ek_arrow arrow)
{
    ek_type *t = ck_fresh(c, EK_T_FN);

    if (!t) {
        return NULL;
    }
    t->u.fn.params = params;
    t->u.fn.count = n;
    t->u.fn.result = result;
    t->u.fn.arrow = arrow;
    return ck_intern(c, t);
}
