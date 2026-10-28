#include <string.h>

#include "check/internal.h"

/* Generalization and instantiation. A scheme carries the rigid
   variables of an annotation; instantiation copies the type, replacing
   each with a fresh inference variable. */

/* Records one rigid variable, deduplicated, growing the buffer as
   needed. */
static void collect_leaf(ck_ctx *c, const ek_type *t, const ek_type ***out,
                         size_t *n, size_t *cap)
{
    const ek_type **all = *out;
    size_t i;

    for (i = 0; i < *n; i++) {
        if (all[i] == t) {
            return;
        }
    }
    if (*n == *cap) {
        *out = ck_grow(c, *out, *n + 1, cap, sizeof **out);
        if (c->oom) {
            return;
        }
        all = *out;
    }
    all[(*n)++] = t;
}

/* Collects the distinct rigid variables of a type into the growable
   out buffer; n and cap are the buffer's fill and capacity. */
void ck_tvars_collect(ck_ctx *c, const ek_type *t, const ek_type ***out,
                      size_t *n, size_t *cap)
{
    size_t i;

    t = ck_prune(t);
    if (!t || c->oom) {
        return;
    }
    if (t->kind == EK_T_TVAR) {
        collect_leaf(c, t, out, n, cap);
        return;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            ck_tvars_collect(c, t->u.nominal.args[i], out, n, cap);
        }
        return;
    case EK_T_LIST:
        ck_tvars_collect(c, t->u.list.elem, out, n, cap);
        return;
    case EK_T_DICT:
        ck_tvars_collect(c, t->u.dict.key, out, n, cap);
        ck_tvars_collect(c, t->u.dict.val, out, n, cap);
        return;
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            ck_tvars_collect(c, t->u.tuple.items[i], out, n, cap);
        }
        return;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            ck_tvars_collect(c, t->u.record.fields[i].type, out, n, cap);
        }
        return;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            ck_tvars_collect(c, t->u.fn.params[i], out, n, cap);
        }
        ck_tvars_collect(c, t->u.fn.result, out, n, cap);
        return;
    default:
        return;
    }
}

ck_scheme ck_generalize(ck_ctx *c, const ek_type *t)
{
    ck_scheme s;
    size_t cap = 0;

    memset(&s, 0, sizeof s);
    s.type = t;
    if (c->oom || !t) {
        return s;
    }
    ck_tvars_collect(c, t, &s.vars, &s.nvars, &cap);
    if (c->oom) {
        s.vars = NULL;
        s.nvars = 0;
    }
    return s;
}

/* One variable becomes ONE inference variable, memoized per
   instantiation, so a param and its result stay linked. A bridge
   scheme's slots are opaque: the implementation the signature stands
   for is not written, and staying undetermined is their resting
   state. */
static const ek_type *inst_leaf(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, size_t nvars,
                                ck_map *memo)
{
    size_t i;

    for (i = 0; i < nvars; i++) {
        if (vars[i] == t) {
            const ek_type *got = ck_map_get(memo, t);

            if (got) {
                return got;
            }
            got = c->inst_bridge ? ck_opaque(c) : ck_ivar(c);
            ck_map_put(c, memo, t, got);
            if (c->inst_capture) {
                ck_map_put(c, c->inst_capture, t, got);
            }
            return got;
        }
    }
    return t;
}

/* Every type variable of a function annotation MUST occur in at
   least one parameter type. */
void ck_check_phantom(ck_ctx *c, const ek_node *ann, const ek_type *t)
{
    const ek_type **all = NULL;
    const ek_type **ps = NULL;
    size_t nall = 0, nps = 0, allcap = 0, pscap = 0, i, j;

    if (!ann || ann->kind != EK_N_T_FN || !t || t->kind != EK_T_FN) {
        return;
    }
    ck_tvars_collect(c, t, &all, &nall, &allcap);
    for (i = 0; i < t->u.fn.count; i++) {
        ck_tvars_collect(c, t->u.fn.params[i], &ps, &nps, &pscap);
    }
    if (c->oom) {
        return;
    }
    for (i = 0; i < nall; i++) {
        int in_param = 0;
        const ek_node *first;

        for (j = 0; j < nps; j++) {
            if (ps[j] == all[i]) {
                in_param = 1;
            }
        }
        if (in_param) {
            continue;
        }
        first = ck_map_get(&c->firsts, all[i]);
        ck_error(c, first ? first : ann, "E-phantom",
                 "the type variable `%s` occurs only in the result; it "
                 "must occur in a parameter type (Clause 7.6.5)",
                 all[i]->u.tvar.name);
    }
}

/* Copies t, replacing the scheme's variables with fresh inference
   variables. */
static const ek_type *inst(ck_ctx *c, const ek_type *t, const ek_type **vars,
                           size_t nvars, ck_map *memo);

/* The copied arguments of a nominal. */
static const ek_type **inst_args(ck_ctx *c, const ek_type *t,
                                 const ek_type **vars, size_t nvars,
                                 ck_map *memo)
{
    const ek_type **arr = NULL;
    size_t i, n = t->u.nominal.argc;

    if (n) {
        arr = ek_arena_alloc(c->arena, n * sizeof *arr);
        if (!arr) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            arr[i] = inst(c, t->u.nominal.args[i], vars, nvars, memo);
        }
    }
    return arr;
}

/* The copied components of a tuple. */
static const ek_type **inst_items(ck_ctx *c, const ek_type *t,
                                  const ek_type **vars, size_t nvars,
                                  ck_map *memo)
{
    const ek_type **arr = NULL;
    size_t i, n = t->u.tuple.count;

    if (n) {
        arr = ek_arena_alloc(c->arena, n * sizeof *arr);
        if (!arr) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            arr[i] = inst(c, t->u.tuple.items[i], vars, nvars, memo);
        }
    }
    return arr;
}

/* The copied fields of a record. */
static ek_tfield *inst_fields(ck_ctx *c, const ek_type *t,
                              const ek_type **vars, size_t nvars,
                              ck_map *memo)
{
    ek_tfield *fs = NULL;
    size_t i, n = t->u.record.count;

    if (n) {
        fs = ek_arena_alloc(c->arena, n * sizeof *fs);
        if (!fs) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            fs[i].name = t->u.record.fields[i].name;
            fs[i].type = inst(c, t->u.record.fields[i].type, vars, nvars,
                              memo);
        }
    }
    return fs;
}

/* The copied parameters of a function. */
static const ek_type **inst_params(ck_ctx *c, const ek_type *t,
                                   const ek_type **vars, size_t nvars,
                                   ck_map *memo)
{
    const ek_type **arr = NULL;
    size_t i, n = t->u.fn.count;

    if (n) {
        arr = ek_arena_alloc(c->arena, n * sizeof *arr);
        if (!arr) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            arr[i] = inst(c, t->u.fn.params[i], vars, nvars, memo);
        }
    }
    return arr;
}

/* The built kinds of an instantiation; an allocation failure leaves
   the original type in place. */
static const ek_type *inst_nominal(ck_ctx *c, const ek_type *t,
                                   const ek_type **vars, size_t nvars,
                                   ck_map *memo)
{
    const ek_type **arr = inst_args(c, t, vars, nvars, memo);

    if (!arr && t->u.nominal.argc) {
        return t;
    }
    return ck_nominal(c, t->u.nominal.sym, arr, t->u.nominal.argc);
}

static const ek_type *inst_tuple(ck_ctx *c, const ek_type *t,
                                 const ek_type **vars, size_t nvars,
                                 ck_map *memo)
{
    const ek_type **items = inst_items(c, t, vars, nvars, memo);

    if (!items && t->u.tuple.count) {
        return t;
    }
    return ck_tuple(c, items, t->u.tuple.count);
}

static const ek_type *inst_record(ck_ctx *c, const ek_type *t,
                                  const ek_type **vars, size_t nvars,
                                  ck_map *memo)
{
    ek_tfield *fs = inst_fields(c, t, vars, nvars, memo);

    if (!fs && t->u.record.count) {
        return t;
    }
    return ck_record(c, fs, t->u.record.count);
}

static const ek_type *inst_fn(ck_ctx *c, const ek_type *t,
                              const ek_type **vars, size_t nvars,
                              ck_map *memo)
{
    const ek_type **ps = inst_params(c, t, vars, nvars, memo);

    if (!ps && t->u.fn.count) {
        return t;
    }
    return ck_fn(c, ps, t->u.fn.count,
                 inst(c, t->u.fn.result, vars, nvars, memo), t->u.fn.arrow);
}

/* A leaf the scheme does not generalize over: copied as it stands. */
static const ek_type *inst_fresh(ck_ctx *c, const ek_type *t)
{
    ek_type *fresh = ck_fresh(c, t->kind);

    if (!fresh) {
        return t;
    }
    *fresh = *t;
    return fresh;
}

/* Copies t, replacing the scheme's variables with fresh inference
   variables. */
static const ek_type *inst(ck_ctx *c, const ek_type *t, const ek_type **vars,
                           size_t nvars, ck_map *memo)
{
    t = ck_prune(t);
    if (!t || c->oom) {
        return t;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        return inst_nominal(c, t, vars, nvars, memo);
    case EK_T_LIST:
        return ck_list(c, inst(c, t->u.list.elem, vars, nvars, memo));
    case EK_T_DICT:
        return ck_dict(c, inst(c, t->u.dict.key, vars, nvars, memo),
                       inst(c, t->u.dict.val, vars, nvars, memo));
    case EK_T_TUPLE:
        return inst_tuple(c, t, vars, nvars, memo);
    case EK_T_RECORD:
        return inst_record(c, t, vars, nvars, memo);
    case EK_T_FN:
        return inst_fn(c, t, vars, nvars, memo);
    case EK_T_TVAR:
        return inst_leaf(c, t, vars, nvars, memo);
    default:
        return inst_fresh(c, t);
    }
}

const ek_type *ck_instantiate(ck_ctx *c, const ck_scheme *s)
{
    ck_map memo;

    if (!s || !s->type) {
        return NULL;
    }
    if (!s->nvars) {
        return s->type;
    }
    memset(&memo, 0, sizeof memo);
    c->inst_bridge = s->bridge;
    return inst(c, s->type, s->vars, s->nvars, &memo);
}

