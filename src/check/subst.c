#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "check/internal.h"

/* Result plumbing: the closed snapshot of a type, printable names,
   and the per-node row table with its diagnostics. */

/* Follows bound inference variables to the solution. */
const ek_type *ck_prune(const ek_type *t)
{
    while (t && t->kind == EK_T_IVAR && t->u.ivar.bound) {
        t = t->u.ivar.bound;
    }
    return t;
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
    case EK_T_NOMINAL: {
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
    case EK_T_LIST:
        copy->u.list.elem = ck_subst(c, t->u.list.elem);
        return ck_intern(c, copy);
    case EK_T_DICT:
        copy->u.dict.key = ck_subst(c, t->u.dict.key);
        copy->u.dict.val = ck_subst(c, t->u.dict.val);
        return ck_intern(c, copy);
    case EK_T_TUPLE: {
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
    case EK_T_RECORD: {
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
    case EK_T_FN: {
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
    default:
        return ck_intern(c, copy);
    }
}

/* A short printable name for diagnostics; never recurses into args. */
const char *ck_type_name(ck_ctx *c, const ek_type *t)
{
    char buf[64];
    char *out;
    size_t n;

    t = ck_prune(t);
    if (!t) {
        return "?";
    }
    switch (t->kind) {
    case EK_T_UNIT:
        return "_";
    case EK_T_NEVER:
        return "Never";
    case EK_T_NOMINAL:
        return t->u.nominal.sym ? t->u.nominal.sym->name : "?";
    case EK_T_LIST:
        return "[]list";
    case EK_T_DICT:
        return "[k]v";
    case EK_T_TUPLE:
        n = (size_t)snprintf(buf, sizeof buf, "[%zu-tuple]",
                             t->u.tuple.count);
        break;
    case EK_T_RECORD:
        n = (size_t)snprintf(buf, sizeof buf, "[%zu-field record]",
                             t->u.record.count);
        break;
    case EK_T_FN:
        return "function";
    case EK_T_TVAR:
        return t->u.tvar.name ? t->u.tvar.name : "t";
    default:
        return "?";
    }
    if (!n || n >= sizeof buf) {
        return "?";
    }
    out = ek_arena_alloc(c->arena, n + 1);
    if (!out) {
        c->oom = 1;
        return "?";
    }
    memcpy(out, buf, n + 1);
    return out;
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

static void row_error(ck_ctx *c, const ek_node *n, const char *code,
                      const char *fmt, va_list ap)
{
    char msg[400];

    vsnprintf(msg, sizeof msg, fmt, ap);
    ek_diag_emit(c->diag, EK_SEV_ERROR, code, ck_file(c),
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
    row_error(c, n, code, fmt, ap);
    va_end(ap);
}

const char *ck_file(const ck_ctx *c)
{
    return c->in->module_count ? c->root->file : "test.eko";
}
