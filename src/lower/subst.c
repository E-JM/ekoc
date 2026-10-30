#include <string.h>

#include "lower/internal.h"

/* The instantiation substitution: the rigid variables of one
   function's annotation mapped to the applied arguments of the call
   that keyed the emission, and the structural replacement every
   row-type read applies under it. Pairing walks the annotation's own
   row against the applied function type position by position, and the
   replacement matches variables by spelling, since the checker's
   elaborations leave one pointer per binder per pass while the
   spellings are unique within one annotation. */

/* Records one binder spelling with its applied argument, first
   occurrence winning. */
static void env_add(lower_ctx *c, const char *name, const ek_type *arg);

/* Pairs the rigid shape with the applied shape, recording every
   variable the applied side answers. Kinds must agree outside the
   variable positions. */
static int pair(lower_ctx *c, const ek_type *r, const ek_type *a);

static void env_add(lower_ctx *c, const char *name, const ek_type *arg)
{
    size_t i;

    for (i = 0; i < c->nenv; i++) {
        if (strcmp(c->env_names[i], name) == 0) {
            return;
        }
    }
    if (c->nenv == c->cenv_cap) {
        size_t ncap = c->cenv_cap ? c->cenv_cap * 2 : 4;
        const char **names = ek_arena_alloc(c->arena,
                                            ncap * sizeof *names);
        const ek_type **args = ek_arena_alloc(c->arena,
                                              ncap * sizeof *args);

        if (!names || !args) {
            c->oom = 1;
            return;
        }
        if (c->nenv) {
            memcpy(names, c->env_names, c->nenv * sizeof *names);
            memcpy(args, c->env_args, c->nenv * sizeof *args);
        }
        c->env_names = names;
        c->env_args = args;
        c->cenv_cap = ncap;
    }
    c->env_names[c->nenv] = name;
    c->env_args[c->nenv] = arg;
    c->nenv++;
}

/* Pairs two component slot arrays of one length. */
static int pair_slots(lower_ctx *c, const ek_type *const *r,
                      const ek_type *const *a, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (!pair(c, r[i], a[i])) {
            return 0;
        }
    }
    return 1;
}

/* Pairs two records: the fields answer by name, in either stored
   order -- the checker sorts record fields, the type-syntax walk
   keeps the written one. */
static int pair_fields(lower_ctx *c, const ek_type *r, const ek_type *a)
{
    size_t i, k;

    if (r->u.record.count != a->u.record.count) {
        return 0;
    }
    for (i = 0; i < r->u.record.count; i++) {
        for (k = 0; k < a->u.record.count; k++) {
            if (strcmp(r->u.record.fields[i].name,
                       a->u.record.fields[k].name) == 0) {
                if (!pair(c, r->u.record.fields[i].type,
                          a->u.record.fields[k].type)) {
                    return 0;
                }
                break;
            }
        }
        if (k == a->u.record.count) {
            return 0;
        }
    }
    return 1;
}

/* Pairs the rigid shape with the applied shape, recording every
   variable the applied side answers. Kinds must agree outside the
   variable positions. */
static int pair(lower_ctx *c, const ek_type *r, const ek_type *a)
{
    if (!r || !a) {
        return r == a;
    }
    if (r->kind == EK_T_TVAR) {
        env_add(c, r->u.tvar.name, a);
        return !c->oom;
    }
    if (r->kind != a->kind) {
        return 0;
    }
    switch (r->kind) {
    case EK_T_NOMINAL:
        if (r->u.nominal.argc != a->u.nominal.argc) {
            return 0;
        }
        return pair_slots(c, r->u.nominal.args, a->u.nominal.args,
                          r->u.nominal.argc);
    case EK_T_LIST:
        return pair(c, r->u.list.elem, a->u.list.elem);
    case EK_T_DICT:
        return pair(c, r->u.dict.key, a->u.dict.key)
            && pair(c, r->u.dict.val, a->u.dict.val);
    case EK_T_TUPLE:
        if (r->u.tuple.count != a->u.tuple.count) {
            return 0;
        }
        return pair_slots(c, r->u.tuple.items, a->u.tuple.items,
                          r->u.tuple.count);
    case EK_T_RECORD:
        return pair_fields(c, r, a);
    case EK_T_FN:
        if (r->u.fn.count != a->u.fn.count) {
            return 0;
        }
        return pair_slots(c, r->u.fn.params, a->u.fn.params,
                          r->u.fn.count)
            && pair(c, r->u.fn.result, a->u.fn.result);
    default:
        return 1;
    }
}

/* Builds the environment of one instantiation: the annotation's rigid
   function type paired against the applied one the call site carried.
   0 when the pairing cannot stand and the body emits unsubstituted. */
int lower_env_build(lower_ctx *c, const ek_symbol *sym, const ek_type *fn)
{
    const ek_ck_row *row;

    c->env_names = NULL;
    c->env_args = NULL;
    c->nenv = 0;
    c->cenv_cap = 0;
    if (!fn || fn->kind != EK_T_FN || !sym->type) {
        return 0;
    }
    row = lower_row(c, sym->type);
    if (!row || !row->type || row->type->kind != EK_T_FN) {
        return 0;
    }
    if (!pair(c, row->type, fn) || c->oom) {
        c->nenv = 0;
        return 0;
    }
    return c->nenv > 0;
}

/* The applied arguments of one instantiation, one per binder of the
   annotation in first-occurrence order -- the list the instantiation
   name spells. 0 when the pairing cannot stand; *out then stands. */
size_t lower_inst_args(lower_ctx *c, const ek_symbol *sym,
                       const ek_type *fn, const ek_type ***out)
{
    const char **saved_names = c->env_names;
    const ek_type **saved_args = c->env_args;
    size_t saved_n = c->nenv, saved_cap = c->cenv_cap;
    size_t n;

    if (!lower_env_build(c, sym, fn)) {
        c->env_names = saved_names;
        c->env_args = saved_args;
        c->nenv = saved_n;
        c->cenv_cap = saved_cap;
        return 0;
    }
    n = c->nenv;
    *out = c->env_args;
    c->env_names = saved_names;
    c->env_args = saved_args;
    c->nenv = saved_n;
    c->cenv_cap = saved_cap;
    return n;
}

/* The rebuilt arguments of a nominal. */
static const ek_type **subst_args(lower_ctx *c, const ek_type *t,
                                  int *changed)
{
    const ek_type **args = NULL;
    size_t i, n = t->u.nominal.argc;

    if (n) {
        args = ek_arena_alloc(c->arena, n * sizeof *args);
        if (!args) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            args[i] = lower_type_in(c, t->u.nominal.args[i]);
            if (args[i] != t->u.nominal.args[i]) {
                *changed = 1;
            }
        }
    }
    return args;
}

/* The rebuilt fields of a record. */
static ek_tfield *subst_fields(lower_ctx *c, const ek_type *t,
                               int *changed)
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
            fs[i].type = lower_type_in(c, t->u.record.fields[i].type);
            if (fs[i].type != t->u.record.fields[i].type) {
                *changed = 1;
            }
        }
    }
    return fs;
}

/* The rebuilt component slots: a function's parameters or a tuple's
   items. */
static const ek_type **subst_slots(lower_ctx *c,
                                   const ek_type *const *slots, size_t n,
                                   int *changed)
{
    const ek_type **ps = NULL;
    size_t i;

    if (n) {
        ps = ek_arena_alloc(c->arena, n * sizeof *ps);
        if (!ps) {
            c->oom = 1;
            return NULL;
        }
        for (i = 0; i < n; i++) {
            ps[i] = lower_type_in(c, slots[i]);
            if (ps[i] != slots[i]) {
                *changed = 1;
            }
        }
    }
    return ps;
}

/* The copy of one compound with its substituted slots; the kind's own
   header -- the nominal's symbol, the arrow, the counts -- carries
   over. An unchanged compound stands as it is. */
static const ek_type *subst_copy(lower_ctx *c, const ek_type *t)
{
    int changed = 0;
    ek_type *out = ek_arena_alloc(c->arena, sizeof *out);

    if (!out) {
        c->oom = 1;
        return t;
    }
    *out = *t;
    switch (t->kind) {
    case EK_T_NOMINAL:
        out->u.nominal.args = subst_args(c, t, &changed);
        break;
    case EK_T_LIST:
        out->u.list.elem = lower_type_in(c, t->u.list.elem);
        changed = out->u.list.elem != t->u.list.elem;
        break;
    case EK_T_DICT:
        out->u.dict.key = lower_type_in(c, t->u.dict.key);
        out->u.dict.val = lower_type_in(c, t->u.dict.val);
        changed = out->u.dict.key != t->u.dict.key
            || out->u.dict.val != t->u.dict.val;
        break;
    case EK_T_TUPLE:
        out->u.tuple.items = subst_slots(c, t->u.tuple.items,
                                         t->u.tuple.count, &changed);
        break;
    case EK_T_RECORD:
        out->u.record.fields = subst_fields(c, t, &changed);
        break;
    case EK_T_FN:
        out->u.fn.params = subst_slots(c, t->u.fn.params, t->u.fn.count,
                                       &changed);
        out->u.fn.result = lower_type_in(c, t->u.fn.result);
        changed = changed || out->u.fn.result != t->u.fn.result;
        break;
    default:
        return t;
    }
    if (c->oom || !changed) {
        return t;
    }
    return out;
}

/* The substitution of one type: binder spellings replace with their
   applied arguments, compounds rebuild only where a child moved, and
   everything else stands. */
const ek_type *lower_type_in(lower_ctx *c, const ek_type *t)
{
    size_t i;

    if (!t || c->nenv == 0) {
        return t;
    }
    if (t->kind == EK_T_TVAR) {
        for (i = 0; i < c->nenv; i++) {
            if (strcmp(c->env_names[i], t->u.tvar.name) == 0) {
                return c->env_args[i];
            }
        }
        return t;
    }
    if (t->kind == EK_T_IVAR) {
        return t;
    }
    return subst_copy(c, t);
}

/* The type a node's check row carries under the instantiation being
   emitted: the row's own snapshot with the binders replaced. */
const ek_type *lower_row_type(lower_ctx *c, const ek_node *n)
{
    const ek_ck_row *row = lower_row(c, n);

    return row ? lower_type_in(c, row->type) : NULL;
}

/* The zero-argument nominal of a std placeholder type, the result
   type of the display and comparison services. */
const ek_type *lower_std_nominal(lower_ctx *c, const char *name)
{
    const ek_symbol *sym = lower_type_symbol(c, "std", name);
    ek_type *t;

    if (!sym) {
        return NULL;
    }
    t = ek_arena_alloc(c->arena, sizeof *t);
    if (!t) {
        c->oom = 1;
        return NULL;
    }
    t->kind = EK_T_NOMINAL;
    t->u.nominal.sym = sym;
    t->u.nominal.args = NULL;
    t->u.nominal.argc = 0;
    return t;
}
