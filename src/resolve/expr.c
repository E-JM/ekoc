#include <string.h>

#include "resolve/internal.h"

/* Expressions: the expression-head lookup order of Clause 7.4.2
   (parameter, local binding, file top-level binding, imported module),
   the constructor reading of Clause 7.4.1, and the dispatch of
   Clause 6.5. Dotted heads go to chain.c. */

static const ek_symbol *find(const r_var *vars, size_t n, const char *name)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            return vars[i].sym;
        }
    }
    return NULL;
}

const ek_symbol *r_lookup_local(const r_file *f, const char *name)
{
    return r_scope_get(f->locals, name);
}

const ek_symbol *r_lookup_top(const r_file *f, const char *name)
{
    return find(f->tops, f->ntops, name);
}

const ek_symbol *r_lookup_mod(const r_file *f, const char *name)
{
    return find(f->mods, f->nmods, name);
}

size_t r_lookup_type(const r_file *f, const char *name,
                     const ek_symbol **first)
{
    size_t i, n = 0;

    *first = NULL;
    for (i = 0; i < f->ntypes; i++) {
        if (strcmp(f->types[i].name, name) == 0) {
            if (n == 0) {
                *first = f->types[i].sym;
            }
            n++;
        }
    }
    return n;
}

size_t r_lookup_ctor(const r_file *f, const char *name,
                     const ek_symbol **first)
{
    size_t i, n = 0;

    *first = NULL;
    for (i = 0; i < f->nctors; i++) {
        if (strcmp(f->ctors[i].name, name) == 0) {
            if (n == 0) {
                *first = f->ctors[i].sym;
            }
            n++;
        }
    }
    return n;
}

/* A placeholder std symbol: module std, no declaration behind it
   (resolve.h bridge ruling). */
static int is_std_placeholder(const ek_symbol *sym)
{
    return sym && sym->module && strcmp(sym->module, "std") == 0
        && sym->decl == NULL;
}

/* Constructor value or pattern position (Clauses 7.4.1, 7.4.2 row 3):
   two visible constructors with one name clash at the use site; an
   unknown one may be std's (the placeholder bridge). */
void r_cons_head(r_ctx *c, r_file *f, const ek_node *n, r_head *out)
{
    const char *name = n->u.path.count == 1 ? n->u.path.data[0] : "";
    const ek_symbol *first = NULL;
    size_t k = r_lookup_ctor(f, name, &first);

    memset(out, 0, sizeof *out);
    if (k > 1) {
        r_error(c, f->mod->file, n->line, n->col, "E-name-conflict",
                "`%s` names several visible constructors; qualify one "
                "(Clause 7.4.6)", name);
        out->kind = EK_SYM_EXTERNAL;
        r_add_ref(c, n, out->kind, EK_ST_CONS, NULL);
        return;
    }
    if (k == 1) {
        out->sym = first;
        out->kind = is_std_placeholder(first) ? EK_SYM_EXTERNAL
                                              : EK_SYM_CONSTRUCTOR;
        out->st = is_std_placeholder(first) ? EK_ST_EXTERNAL : EK_ST_CONS;
        r_add_ref(c, n, out->kind, out->st, out->sym);
        return;
    }
    out->sym = r_std_ctor(c, name);
    out->kind = EK_SYM_EXTERNAL;
    out->st = EK_ST_EXTERNAL;
    r_add_ref(c, n, out->kind, out->st, out->sym);
}

/* A module name is not a value (Clause 7.2.6). */
static void module_value(r_ctx *c, r_file *f, const ek_node *n)
{
    r_error(c, f->mod->file, n->line, n->col, "E-module-value",
            "module `%s` never enters value position (Clause 7.2.6)",
            n->u.path.data[0]);
}

/* The expression head of Clause 7.4.2. */
void r_name_head(r_ctx *c, r_file *f, const ek_node *n, int chained,
                 r_head *out)
{
    const char *name = n->u.path.count == 1 ? n->u.path.data[0] : "";
    const ek_symbol *sym;

    memset(out, 0, sizeof *out);
    if (strcmp(name, "_") == 0) {
        /* The unit value at value position, not a name (7.3.1). */
        return;
    }
    if (!r_is_snake(name)) {
        r_cons_head(c, f, n, out);
        return;
    }
    if ((sym = r_lookup_local(f, name)) != NULL) {
        out->sym = sym;
        out->kind = sym->kind;
        out->st = EK_ST_HEAD_LOCAL;
    } else if ((sym = r_lookup_top(f, name)) != NULL) {
        out->sym = sym;
        if (is_std_placeholder(sym)) {
            /* The free names of the implicit import resolve to the
               unresolved-external placeholder (Clause 7.2.5,
               resolve.h). */
            out->kind = EK_SYM_EXTERNAL;
            out->st = EK_ST_EXTERNAL;
        } else {
            out->kind = sym->kind;
            out->st = EK_ST_HEAD_TOP;
        }
    } else if ((sym = r_lookup_mod(f, name)) != NULL) {
        out->sym = sym;
        out->kind = EK_SYM_MODULE;
        out->st = EK_ST_HEAD_MODULE;
        if (!chained) {
            module_value(c, f, n);
        }
    } else {
        r_error(c, f->mod->file, n->line, n->col, "E-unbound-name",
                "`%s` is not bound in any enclosing scope (Clause 7.4.2)",
                name);
        out->kind = EK_SYM_EXTERNAL;
    }
    r_add_ref(c, n, out->kind, out->st, out->sym);
}

static void r_apply(r_ctx *c, r_file *f, const ek_node *e)
{
    const ek_node *head = e->u.pair.a;
    r_head h;

    r_expr(c, f, e->u.pair.b);
    if (head && head->kind == EK_N_NAME) {
        r_cons_head(c, f, head, &h);
    } else if (head && head->kind == EK_N_FIELD) {
        /* A qualified all-Pascal head; the chain classifier decides
           module and namespace readings (Clauses 7.4.4, 7.4.5). */
        r_chain(c, f, head, 0, &h);
    } else {
        memset(&h, 0, sizeof h);
        h.kind = EK_SYM_EXTERNAL;
        h.st = EK_ST_EXTERNAL;
    }
    r_add_ref(c, e, h.kind, h.st, h.sym);
}

/* Composite items: the value of every item resolves; a colon item's
   key is a record field or a dictionary key expression, and the
   expected type decides (Clause 7.5.3), so keys resolve in check. */
static void r_items(r_ctx *c, r_file *f, const ek_node *e)
{
    size_t i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM) {
            r_expr(c, f, item->u.pair.b);
        }
    }
}

/* Interpolation slots are full expressions (Clause 5.9.3). */
static void r_slots(r_ctx *c, r_file *f, const ek_node *e)
{
    size_t i;

    for (i = 0; i < e->u.str.count; i++) {
        r_expr(c, f, e->u.str.segs[i].expr);
    }
}

void r_expr(r_ctx *c, r_file *f, const ek_node *e)
{
    r_head h;

    if (c->oom || !e) {
        return;
    }
    switch (e->kind) {
    case EK_N_NAME:
        r_name_head(c, f, e, 0, &h);
        return;
    case EK_N_FIELD:
    case EK_N_CALL:
        r_chain(c, f, e, e->kind == EK_N_CALL, &h);
        return;
    case EK_N_APPLY:
        r_apply(c, f, e);
        return;
    case EK_N_LAMBDA:
        r_lambda(c, f, e);
        return;
    case EK_N_CASE:
        r_case(c, f, e);
        return;
    case EK_N_BLOCK:
        r_block(c, f, e, f->locals);
        return;
    case EK_N_IF:
        r_expr(c, f, e->u.ifexpr.cond);
        r_expr(c, f, e->u.ifexpr.then);
        r_expr(c, f, e->u.ifexpr.els);
        return;
    case EK_N_UNARY:
    case EK_N_PAREN:
        r_expr(c, f, e->u.wrap.inner);
        return;
    case EK_N_BINARY:
        r_expr(c, f, e->u.binary.lhs);
        r_expr(c, f, e->u.binary.rhs);
        return;
    case EK_N_COMPOSITE:
        r_items(c, f, e);
        return;
    case EK_N_STRING_LIT:
        r_slots(c, f, e);
        return;
    default:
        return;
    }
}
