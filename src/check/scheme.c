#include <string.h>

#include "check/internal.h"

/* Generalization and instantiation (Clause 7.6). A scheme carries the
   rigid variables of an annotation; instantiation copies the type,
   replacing each with a fresh inference variable (Clause 7.6.4). */

/* Collects the distinct rigid variables of a type into out (capacity
   is the caller's). */
void ck_tvars_collect(const ek_type *t, const ek_type ***out, size_t *n)
{
    const ek_type **all = *out;
    size_t i;

    t = ck_prune(t);
    if (!t) {
        return;
    }
    if (t->kind == EK_T_TVAR) {
        for (i = 0; i < *n; i++) {
            if (all[i] == t) {
                return;
            }
        }
        all[(*n)++] = t;
        return;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            ck_tvars_collect(t->u.nominal.args[i], out, n);
        }
        return;
    case EK_T_LIST:
        ck_tvars_collect(t->u.list.elem, out, n);
        return;
    case EK_T_DICT:
        ck_tvars_collect(t->u.dict.key, out, n);
        ck_tvars_collect(t->u.dict.val, out, n);
        return;
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            ck_tvars_collect(t->u.tuple.items[i], out, n);
        }
        return;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            ck_tvars_collect(t->u.record.fields[i].type, out, n);
        }
        return;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            ck_tvars_collect(t->u.fn.params[i], out, n);
        }
        ck_tvars_collect(t->u.fn.result, out, n);
        return;
    default:
        return;
    }
}

ck_scheme ck_generalize(ck_ctx *c, const ek_type *t)
{
    ck_scheme s;
    const ek_type **all = NULL;
    size_t n = 0;

    memset(&s, 0, sizeof s);
    s.type = t;
    if (c->oom || !t) {
        return s;
    }
    all = ek_arena_alloc(c->arena, 64 * sizeof *all);
    if (!all) {
        c->oom = 1;
        return s;
    }
    ck_tvars_collect(t, &all, &n);
    s.vars = all;
    s.nvars = n > 64 ? 64 : n;
    return s;
}

/* Copies t, replacing the scheme's variables with fresh inference
   variables. One variable becomes ONE inference variable, memoized
   per instantiation, so a param and its result stay linked. */
static const ek_type *inst(ck_ctx *c, const ek_type *t, const ek_type **vars,
                           size_t nvars, ck_map *memo)
{
    const ek_type **arr = NULL;
    ek_type *fresh;
    size_t i, n;

    t = ck_prune(t);
    if (!t || c->oom) {
        return t;
    }
    for (i = 0; i < nvars; i++) {
        if (vars[i] == t) {
            const ek_type *got = ck_map_get(memo, t);

            if (got) {
                return got;
            }
            got = ck_ivar(c);
            ck_map_put(c, memo, t, got);
            return got;
        }
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        n = t->u.nominal.argc;
        if (n) {
            arr = ek_arena_alloc(c->arena, n * sizeof *arr);
            if (!arr) {
                c->oom = 1;
                return t;
            }
            for (i = 0; i < n; i++) {
                arr[i] = inst(c, t->u.nominal.args[i], vars, nvars, memo);
            }
        }
        return ck_nominal(c, t->u.nominal.sym, arr, n);
    case EK_T_LIST:
        return ck_list(c, inst(c, t->u.list.elem, vars, nvars, memo));
    case EK_T_DICT:
        return ck_dict(c, inst(c, t->u.dict.key, vars, nvars, memo),
                       inst(c, t->u.dict.val, vars, nvars, memo));
    case EK_T_TUPLE:
        n = t->u.tuple.count;
        if (n) {
            arr = ek_arena_alloc(c->arena, n * sizeof *arr);
            if (!arr) {
                c->oom = 1;
                return t;
            }
            for (i = 0; i < n; i++) {
                arr[i] = inst(c, t->u.tuple.items[i], vars, nvars, memo);
            }
        }
        return ck_tuple(c, arr, n);
    case EK_T_RECORD: {
        ek_tfield *fs = NULL;

        n = t->u.record.count;
        if (n) {
            fs = ek_arena_alloc(c->arena, n * sizeof *fs);
            if (!fs) {
                c->oom = 1;
                return t;
            }
            for (i = 0; i < n; i++) {
                fs[i].name = t->u.record.fields[i].name;
                fs[i].type = inst(c, t->u.record.fields[i].type, vars, nvars,
                              memo);
            }
        }
        return ck_record(c, fs, n);
    }
    case EK_T_FN:
        n = t->u.fn.count;
        if (n) {
            arr = ek_arena_alloc(c->arena, n * sizeof *arr);
            if (!arr) {
                c->oom = 1;
                return t;
            }
            for (i = 0; i < n; i++) {
                arr[i] = inst(c, t->u.fn.params[i], vars, nvars, memo);
            }
        }
        return ck_fn(c, arr, n, inst(c, t->u.fn.result, vars, nvars, memo),
                     t->u.fn.arrow);
    default:
        fresh = ck_fresh(c, t->kind);
        if (!fresh) {
            return t;
        }
        *fresh = *t;
        return fresh;
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
    return inst(c, s->type, s->vars, s->nvars, &memo);
}

/* Replaces each vars[i] with repls[i]: the payload of a constructor
   applied to the scrutinee's instantiation (Clause 7.6.4). */
static const ek_type *repl_walk(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, const ek_type **repls,
                                size_t n)
{
    const ek_type **arr = NULL;
    size_t i, k, m;

    t = ck_prune(t);
    if (!t || c->oom) {
        return t;
    }
    for (i = 0; i < n; i++) {
        if (vars[i] == t) {
            return repls[i];
        }
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        m = t->u.nominal.argc;
        if (m) {
            arr = ek_arena_alloc(c->arena, m * sizeof *arr);
            if (!arr) {
                c->oom = 1;
                return t;
            }
            for (k = 0; k < m; k++) {
                arr[k] = repl_walk(c, t->u.nominal.args[k], vars, repls, n);
            }
        }
        return ck_nominal(c, t->u.nominal.sym, arr, m);
    case EK_T_LIST:
        return ck_list(c, repl_walk(c, t->u.list.elem, vars, repls, n));
    case EK_T_DICT:
        return ck_dict(c, repl_walk(c, t->u.dict.key, vars, repls, n),
                       repl_walk(c, t->u.dict.val, vars, repls, n));
    case EK_T_TUPLE:
        m = t->u.tuple.count;
        if (m) {
            arr = ek_arena_alloc(c->arena, m * sizeof *arr);
            if (!arr) {
                c->oom = 1;
                return t;
            }
            for (k = 0; k < m; k++) {
                arr[k] = repl_walk(c, t->u.tuple.items[k], vars, repls, n);
            }
        }
        return ck_tuple(c, arr, m);
    case EK_T_RECORD: {
        ek_tfield *fs = NULL;

        m = t->u.record.count;
        if (m) {
            fs = ek_arena_alloc(c->arena, m * sizeof *fs);
            if (!fs) {
                c->oom = 1;
                return t;
            }
            for (k = 0; k < m; k++) {
                fs[k].name = t->u.record.fields[k].name;
                fs[k].type = repl_walk(c, t->u.record.fields[k].type, vars,
                                       repls, n);
            }
        }
        return ck_record(c, fs, m);
    }
    case EK_T_FN:
        m = t->u.fn.count;
        if (m) {
            arr = ek_arena_alloc(c->arena, m * sizeof *arr);
            if (!arr) {
                c->oom = 1;
                return t;
            }
            for (k = 0; k < m; k++) {
                arr[k] = repl_walk(c, t->u.fn.params[k], vars, repls, n);
            }
        }
        return ck_fn(c, arr, m, repl_walk(c, t->u.fn.result, vars, repls, n),
                     t->u.fn.arrow);
    default: {
        ek_type *fresh = ck_fresh(c, t->kind);

        if (!fresh) {
            return t;
        }
        *fresh = *t;
        return fresh;
    }
    }
}

const ek_type *ck_apply_binders(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, const ek_type **repls,
                                size_t n)
{
    if (!t || !n || !vars || !repls) {
        return t;
    }
    return repl_walk(c, t, vars, repls, n);
}
