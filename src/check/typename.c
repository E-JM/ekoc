#include <string.h>

#include "support/buf.h"

#include "check/internal.h"

/* The printable spelling of a type for diagnostics: the language's
   own type syntax, with a shared name qualified by the module that
   declares the type and the depth budget a cyclic spelling cannot
   exceed. */
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

/* The fields of a record, in their declaration order. */
static int fields_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth)
{
    const ek_tfield *fs = t->u.record.fields;
    const size_t *decl = t->u.record.decl;
    size_t i;
    int ok = 1;

    for (i = 0; ok && i < t->u.record.count; i++) {
        size_t k = decl ? decl[i] : i;

        if (i) {
            ok = ek_buf_append_str(b, ", ");
        }
        ok = ok && ek_buf_append_str(b, fs[k].name)
            && ek_buf_append_str(b, ": ")
            && name_append(c, b, fs[k].type, depth);
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

/* True when one surface declares a type of this name. */
static int surface_declares(const ek_surface *s, const char *name)
{
    size_t i;

    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_TYPE
            && strcmp(s->syms[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int shared_has(const ck_ctx *c, const char *name)
{
    size_t i;

    for (i = 0; i < c->nshared; i++) {
        if (strcmp(c->shared[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Appends one name to the shared list, growing it. */
static void shared_add(ck_ctx *c, const char *name)
{
    if (c->nshared == c->cshared) {
        size_t ncap = c->cshared ? c->cshared * 2 : 16;
        const char **grown = ek_arena_alloc(c->arena, ncap * sizeof *grown);

        if (!grown) {
            c->oom = 1;
            return;
        }
        if (c->shared) {
            memcpy(grown, c->shared, c->nshared * sizeof *grown);
        }
        c->shared = grown;
        c->cshared = ncap;
    }
    c->shared[c->nshared++] = name;
}

/* The type names more than one module of the closure declares: the
   printable form of such a name carries its module, so a message about
   two of them tells them apart. Built once; the surfaces are fixed. */
int ck_shared_name(ck_ctx *c, const char *name)
{
    size_t i, k, m;

    if (!c || !c->in || !name) {
        return 0;
    }
    if (!c->shared_ready) {
        c->shared_ready = 1;
        for (k = 0; k < c->in->module_count; k++) {
            const ek_surface *s = &c->in->modules[k];

            for (i = 0; i < s->count; i++) {
                const ek_symbol *sym = &s->syms[i];
                size_t owners = 0;

                if (sym->kind != EK_SYM_TYPE || shared_has(c, sym->name)) {
                    continue;
                }
                for (m = 0; m < c->in->module_count; m++) {
                    if (surface_declares(&c->in->modules[m], sym->name)) {
                        owners++;
                    }
                }
                if (owners > 1) {
                    shared_add(c, sym->name);
                }
            }
        }
    }
    return shared_has(c, name);
}

/* A nominal type: a name more than one module declares carries its
   module, then the space separated application arguments. */
static int nominal_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth)
{
    const ek_symbol *sym = t->u.nominal.sym;
    size_t i;
    int ok;

    if (sym && sym->module && ck_shared_name(c, sym->name)) {
        ok = ek_buf_append_str(b, sym->module)
            && ek_buf_append_char(b, '.')
            && ek_buf_append_str(b, sym->name);
    } else {
        ok = ek_buf_append_str(b, sym ? sym->name : "?");
    }
    for (i = 0; ok && i < t->u.nominal.argc; i++) {
        ok = ek_buf_append_char(b, ' ')
            && arg_append(c, b, t->u.nominal.args[i], depth - 1);
    }
    return ok;
}

/* An arrow type: the parenthesized parameters, the arrow's spelling,
   the result. */
static int fn_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth)
{
    return ek_buf_append_char(b, '(')
        && list_append(c, b, t->u.fn.params, t->u.fn.count, depth - 1)
        && ek_buf_append_char(b, ')')
        && ek_buf_append_char(b, ' ')
        && ek_buf_append_str(b, arrow_text(t->u.fn.arrow))
        && ek_buf_append_char(b, ' ')
        && name_append(c, b, t->u.fn.result, depth - 1);
}

static int name_append(ck_ctx *c, ek_buf *b, const ek_type *t, int depth)
{
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
        return nominal_append(c, b, t, depth);
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
            && fields_append(c, b, t, depth - 1)
            && ek_buf_append_char(b, ']');
    case EK_T_FN:
        return fn_append(c, b, t, depth);
    case EK_T_TVAR:
        return ek_buf_append_str(b, t->u.tvar.name ? t->u.tvar.name : "t");
    default:
        /* An undetermined variable is not the open bit: the bare `?`
           is that marker's spelling (Clause 7.9.8), so the inference
           artifact reads distinctly. */
        return ek_buf_append_str(b, "?t");
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
