#include <stdarg.h>
#include <string.h>

#include "check/internal.h"
#include "support/buf.h"

/* Result plumbing: the closed snapshot of a type, the binder
   substitution, and printable names. */

/* Follows bound inference variables to the solution. */
const ek_type *ck_prune(const ek_type *t)
{
    while (t && t->kind == EK_T_IVAR && t->u.ivar.bound) {
        t = t->u.ivar.bound;
    }
    return t;
}

/* The substituted arguments of a nominal copy. */
static const ek_type *subst_nominal(ck_ctx *c, ek_type *copy,
                                    const ek_type *t)
{
    const ek_type **args = NULL;
    size_t i, n = t->u.nominal.argc;

    if (n) {
        args = ek_arena_alloc(c->arena, n * sizeof *args);
        if (!args) {
            c->oom = 1;
            return t;
        }
        for (i = 0; i < n; i++) {
            args[i] = ck_subst(c, t->u.nominal.args[i]);
        }
    }
    copy->u.nominal.args = args;
    return ck_intern(c, copy);
}

/* The substituted components of a tuple copy. */
static const ek_type *subst_tuple(ck_ctx *c, ek_type *copy,
                                  const ek_type *t)
{
    const ek_type **items = NULL;
    size_t i, n = t->u.tuple.count;

    if (n) {
        items = ek_arena_alloc(c->arena, n * sizeof *items);
        if (!items) {
            c->oom = 1;
            return t;
        }
        for (i = 0; i < n; i++) {
            items[i] = ck_subst(c, t->u.tuple.items[i]);
        }
    }
    copy->u.tuple.items = items;
    return ck_intern(c, copy);
}

/* The substituted fields of a record copy. */
static const ek_type *subst_record(ck_ctx *c, ek_type *copy,
                                   const ek_type *t)
{
    ek_tfield *fs = NULL;
    size_t i, n = t->u.record.count;

    if (n) {
        fs = ek_arena_alloc(c->arena, n * sizeof *fs);
        if (!fs) {
            c->oom = 1;
            return t;
        }
        for (i = 0; i < n; i++) {
            fs[i].name = t->u.record.fields[i].name;
            fs[i].type = ck_subst(c, t->u.record.fields[i].type);
        }
    }
    copy->u.record.fields = fs;
    return ck_intern(c, copy);
}

/* The substituted parameters and result of a function copy. */
static const ek_type *subst_fn(ck_ctx *c, ek_type *copy, const ek_type *t)
{
    const ek_type **ps = NULL;
    size_t i, n = t->u.fn.count;

    if (n) {
        ps = ek_arena_alloc(c->arena, n * sizeof *ps);
        if (!ps) {
            c->oom = 1;
            return t;
        }
        for (i = 0; i < n; i++) {
            ps[i] = ck_subst(c, t->u.fn.params[i]);
        }
    }
    copy->u.fn.params = ps;
    copy->u.fn.result = ck_subst(c, t->u.fn.result);
    return ck_intern(c, copy);
}

/* The closed snapshot the result table carries: bound ivars are
   substituted away, unbound ones remain. Memoized; the provisional
   self-entry terminates recursive types. */
const ek_type *ck_subst(ck_ctx *c, const ek_type *t)
{
    const ek_type *seen;
    ek_type *copy;

    if (!t) {
        return NULL;
    }
    if (t->kind == EK_T_IVAR) {
        return t->u.ivar.bound ? ck_subst(c, t->u.ivar.bound) : t;
    }
    seen = ck_map_get(&c->subst, t);
    if (seen) {
        return seen;
    }
    copy = ck_fresh(c, t->kind);
    if (!copy) {
        return t;
    }
    *copy = *t;
    ck_map_put(c, &c->subst, t, copy);
    switch (t->kind) {
    case EK_T_NOMINAL:
        return subst_nominal(c, copy, t);
    case EK_T_LIST:
        copy->u.list.elem = ck_subst(c, t->u.list.elem);
        return ck_intern(c, copy);
    case EK_T_DICT:
        copy->u.dict.key = ck_subst(c, t->u.dict.key);
        copy->u.dict.val = ck_subst(c, t->u.dict.val);
        return ck_intern(c, copy);
    case EK_T_TUPLE:
        return subst_tuple(c, copy, t);
    case EK_T_RECORD:
        return subst_record(c, copy, t);
    case EK_T_FN:
        return subst_fn(c, copy, t);
    default:
        return ck_intern(c, copy);
    }
}

/* Types that need parentheses as a nominal application argument. */
static int grouped_arg(const ek_type *t)
{
    t = ck_prune(t);
    return t && (t->kind == EK_T_LIST || t->kind == EK_T_DICT
                 || t->kind == EK_T_TUPLE || t->kind == EK_T_RECORD
                 || t->kind == EK_T_FN);
}

static int name_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth);

/* One nominal application argument: a composite or arrow type takes
   the parentheses the type syntax requires there. */
static int arg_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth)
{
    int ok;

    if (!grouped_arg(t)) {
        return name_append(c, b, t, depth);
    }
    ok = ek_buf_append_char(b, '(');
    ok = ok && name_append(c, b, t, depth);
    return ok && ek_buf_append_char(b, ')');
}

/* The argument list of a function type or the components of a tuple. */
static int list_append(ck_ctx *c, ek_buf *b, const ek_type *const *items,
                       size_t n, int depth)
{
    size_t i;
    int ok = 1;

    for (i = 0; ok && i < n; i++) {
        if (i) {
            ok = ek_buf_append_str(b, ", ");
        }
        if (ok) {
            ok = name_append(c, b, items[i], depth);
        }
    }
    return ok;
}

/* The fields of a record, in their canonical order. */
static int fields_append(ck_ctx *c, ek_buf *b, const ek_tfield *fs, size_t n,
                         int depth)
{
    size_t i;
    int ok = 1;

    for (i = 0; ok && i < n; i++) {
        if (i) {
            ok = ek_buf_append_str(b, ", ");
        }
        ok = ok && ek_buf_append_str(b, fs[i].name)
            && ek_buf_append_str(b, ": ")
            && name_append(c, b, fs[i].type, depth);
    }
    return ok;
}

static const char *arrow_text(ek_arrow a)
{
    if (a == EK_ARROW_WORLD) {
        return "!->";
    }
    return a == EK_ARROW_OPEN ? "?->" : "->";
}

static int name_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth)
{
    size_t i;
    int ok;

    if (!t || depth <= 0) {
        return 1;
    }
    t = ck_prune(t);
    if (!t) {
        return 1;
    }
    switch (t->kind) {
    case EK_T_UNIT:
        return ek_buf_append_str(b, "_");
    case EK_T_NEVER:
        return ek_buf_append_str(b, "Never");
    case EK_T_NOMINAL:
        ok = ek_buf_append_str(b, t->u.nominal.sym
                                      ? t->u.nominal.sym->name : "?");
        for (i = 0; ok && i < t->u.nominal.argc; i++) {
            ok = ek_buf_append_char(b, ' ')
                && arg_append(c, b, t->u.nominal.args[i], depth - 1);
        }
        return ok;
    case EK_T_LIST:
        return ek_buf_append_str(b, "[]")
            && name_append(c, b, t->u.list.elem, depth - 1);
    case EK_T_DICT:
        return ek_buf_append_char(b, '[')
            && name_append(c, b, t->u.dict.key, depth - 1)
            && ek_buf_append_char(b, ']')
            && name_append(c, b, t->u.dict.val, depth - 1);
    case EK_T_TUPLE:
        return ek_buf_append_char(b, '[')
            && list_append(c, b, t->u.tuple.items, t->u.tuple.count,
                           depth - 1)
            && ek_buf_append_char(b, ']');
    case EK_T_RECORD:
        return ek_buf_append_char(b, '[')
            && fields_append(c, b, t->u.record.fields, t->u.record.count,
                             depth - 1)
            && ek_buf_append_char(b, ']');
    case EK_T_FN:
        ok = ek_buf_append_char(b, '(')
            && list_append(c, b, t->u.fn.params, t->u.fn.count, depth - 1)
            && ek_buf_append_char(b, ')')
            && ek_buf_append_char(b, ' ')
            && ek_buf_append_str(b, arrow_text(t->u.fn.arrow))
            && ek_buf_append_char(b, ' ')
            && name_append(c, b, t->u.fn.result, depth - 1);
        return ok;
    case EK_T_TVAR:
        return ek_buf_append_str(b, t->u.tvar.name ? t->u.tvar.name : "t");
    default:
        return ek_buf_append_char(b, '?');
    }
}

/* A printable name for diagnostics in the language's type syntax. */
const char *ck_type_name(ck_ctx *c, const ek_type *t)
{
    ek_buf b;
    char *out;

    ek_buf_init(&b);
    if (!name_append(c, &b, t, 64) || !b.data) {
        ek_buf_destroy(&b);
        return "?";
    }
    out = ek_arena_alloc(c->arena, b.len + 1);
    if (!out) {
        ek_buf_destroy(&b);
        c->oom = 1;
        return "?";
    }
    memcpy(out, b.data, b.len + 1);
    ek_buf_destroy(&b);
    return out;
}

static const ek_type *repl_walk(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, const ek_type **repls,
                                size_t n);

/* The replaced arguments of a nominal. */
static const ek_type **repl_args(ck_ctx *c, const ek_type *t,
                                 const ek_type **vars, const ek_type **repls,
                                 size_t n)
{
    const ek_type **arr = NULL;
    size_t k, m = t->u.nominal.argc;

    if (m) {
        arr = ek_arena_alloc(c->arena, m * sizeof *arr);
        if (!arr) {
            c->oom = 1;
            return NULL;
        }
        for (k = 0; k < m; k++) {
            arr[k] = repl_walk(c, t->u.nominal.args[k], vars, repls, n);
        }
    }
    return arr;
}

/* The replaced components of a tuple. */
static const ek_type **repl_items(ck_ctx *c, const ek_type *t,
                                  const ek_type **vars,
                                  const ek_type **repls, size_t n)
{
    const ek_type **arr = NULL;
    size_t k, m = t->u.tuple.count;

    if (m) {
        arr = ek_arena_alloc(c->arena, m * sizeof *arr);
        if (!arr) {
            c->oom = 1;
            return NULL;
        }
        for (k = 0; k < m; k++) {
            arr[k] = repl_walk(c, t->u.tuple.items[k], vars, repls, n);
        }
    }
    return arr;
}

/* The replaced fields of a record. */
static ek_tfield *repl_fields(ck_ctx *c, const ek_type *t,
                              const ek_type **vars, const ek_type **repls,
                              size_t n)
{
    ek_tfield *fs = NULL;
    size_t k, m = t->u.record.count;

    if (m) {
        fs = ek_arena_alloc(c->arena, m * sizeof *fs);
        if (!fs) {
            c->oom = 1;
            return NULL;
        }
        for (k = 0; k < m; k++) {
            fs[k].name = t->u.record.fields[k].name;
            fs[k].type = repl_walk(c, t->u.record.fields[k].type, vars,
                                   repls, n);
        }
    }
    return fs;
}

/* The replaced parameters of a function. */
static const ek_type **repl_params(ck_ctx *c, const ek_type *t,
                                   const ek_type **vars,
                                   const ek_type **repls, size_t n)
{
    const ek_type **arr = NULL;
    size_t k, m = t->u.fn.count;

    if (m) {
        arr = ek_arena_alloc(c->arena, m * sizeof *arr);
        if (!arr) {
            c->oom = 1;
            return NULL;
        }
        for (k = 0; k < m; k++) {
            arr[k] = repl_walk(c, t->u.fn.params[k], vars, repls, n);
        }
    }
    return arr;
}

static const ek_type *repl_walk(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, const ek_type **repls,
                                size_t n);

/* The built kinds of a substitution; an allocation failure leaves the
   original type in place. */
static const ek_type *repl_nominal(ck_ctx *c, const ek_type *t,
                                   const ek_type **vars,
                                   const ek_type **repls, size_t n)
{
    const ek_type **arr = repl_args(c, t, vars, repls, n);

    if (!arr && t->u.nominal.argc) {
        return t;
    }
    return ck_nominal(c, t->u.nominal.sym, arr, t->u.nominal.argc);
}

static const ek_type *repl_tuple(ck_ctx *c, const ek_type *t,
                                 const ek_type **vars, const ek_type **repls,
                                 size_t n)
{
    const ek_type **items = repl_items(c, t, vars, repls, n);

    if (!items && t->u.tuple.count) {
        return t;
    }
    return ck_tuple(c, items, t->u.tuple.count);
}

static const ek_type *repl_record(ck_ctx *c, const ek_type *t,
                                  const ek_type **vars,
                                  const ek_type **repls, size_t n)
{
    ek_tfield *fs = repl_fields(c, t, vars, repls, n);

    if (!fs && t->u.record.count) {
        return t;
    }
    return ck_record(c, fs, t->u.record.count);
}

static const ek_type *repl_fn(ck_ctx *c, const ek_type *t,
                              const ek_type **vars, const ek_type **repls,
                              size_t n)
{
    const ek_type **ps = repl_params(c, t, vars, repls, n);

    if (!ps && t->u.fn.count) {
        return t;
    }
    return ck_fn(c, ps, t->u.fn.count,
                 repl_walk(c, t->u.fn.result, vars, repls, n),
                 t->u.fn.arrow);
}

/* A leaf outside the substitution: copied as it stands. */
static const ek_type *repl_fresh(ck_ctx *c, const ek_type *t)
{
    ek_type *fresh = ck_fresh(c, t->kind);

    if (!fresh) {
        return t;
    }
    *fresh = *t;
    return fresh;
}

static const ek_type *repl_walk(ck_ctx *c, const ek_type *t,
                                const ek_type **vars, const ek_type **repls,
                                size_t n)
{
    size_t i;

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
        return repl_nominal(c, t, vars, repls, n);
    case EK_T_LIST:
        return ck_list(c, repl_walk(c, t->u.list.elem, vars, repls, n));
    case EK_T_DICT:
        return ck_dict(c, repl_walk(c, t->u.dict.key, vars, repls, n),
                       repl_walk(c, t->u.dict.val, vars, repls, n));
    case EK_T_TUPLE:
        return repl_tuple(c, t, vars, repls, n);
    case EK_T_RECORD:
        return repl_record(c, t, vars, repls, n);
    case EK_T_FN:
        return repl_fn(c, t, vars, repls, n);
    default:
        return repl_fresh(c, t);
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
