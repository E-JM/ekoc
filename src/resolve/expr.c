#include <string.h>

#include "resolve/internal.h"

/* Expressions: the expression-head lookup order (parameter, local
   binding, file top-level binding or opened free name, imported
   module, compiler service), the constructor reading, and the per-form
   dispatch. Dotted heads go to chain.c. */

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

/* The layer lookups. A free name from std answers only when an import
   opened it, so a file binding of the same spelling owns the name. */
const ek_symbol *r_lookup_local(const r_file *f, const char *name)
{
    return r_scope_get(f->locals, name);
}

const ek_symbol *r_lookup_top(const r_file *f, const char *name)
{
    const ek_symbol *sym = find(f->tops, f->ntops, name);
    size_t i;

    if (sym) {
        return sym;
    }
    for (i = 0; i < f->nopens; i++) {
        if (strcmp(f->opens[i].name, name) == 0) {
            return f->opens[i].sym;
        }
    }
    return NULL;
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
    if (n == 0) {
        const ek_symbol *sym = r_std_find(f->std, EK_SYM_TYPE, name);

        if (sym) {
            *first = sym;
            n = 1;
        }
    }
    if (n == 0) {
        /* The capability module is the fallback beneath std: the
           handle and data types of the capability surfaces live there
           (Clause 9.1.1). */
        const ek_symbol *sym = r_std_find(f->cap, EK_SYM_TYPE, name);

        if (sym) {
            *first = sym;
            n = 1;
        }
    }
    return n;
}

/* Whether the visible constructors already hold this symbol: the std
   module's own rows arrive both through its surface and through the
   fallback below it, and one constructor is one candidate. */
static int ctor_listed(const r_var *vars, size_t n, const ek_symbol *sym)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (vars[i].sym == sym) {
            return 1;
        }
    }
    return 0;
}

size_t r_lookup_ctor(r_ctx *c, const r_file *f, const char *name,
                     const ek_symbol *const **cands)
{
    const ek_symbol *std_ct = r_std_find(f->std, EK_SYM_CONSTRUCTOR, name);
    const ek_symbol *cap_ct = r_std_find(f->cap, EK_SYM_CONSTRUCTOR, name);
    const ek_symbol **out;
    int std_fb, cap_fb;
    size_t i, n = 0;

    *cands = NULL;
    /* A private std or cap constructor never reaches an unqualified
       reference: visibility follows the type, and only the declaring
           module sees it. */
    std_fb = std_ct && std_ct->public
             && !ctor_listed(f->ctors, f->nctors, std_ct);
    cap_fb = cap_ct && cap_ct->public
             && !ctor_listed(f->ctors, f->nctors, cap_ct);
    for (i = 0; i < f->nctors; i++) {
        if (strcmp(f->ctors[i].name, name) == 0) {
            n++;
        }
    }
    if (std_fb) {
        n++;
    }
    if (cap_fb) {
        n++;
    }
    if (n == 0) {
        return 0;
    }
    out = ek_arena_alloc(c->arena, n * sizeof *out);
    if (!out) {
        c->oom = 1;
        return 0;
    }
    n = 0;
    for (i = 0; i < f->nctors; i++) {
        if (strcmp(f->ctors[i].name, name) == 0) {
            out[n++] = f->ctors[i].sym;
        }
    }
    if (std_fb) {
        out[n++] = std_ct;
    }
    if (cap_fb) {
        out[n++] = cap_ct;
    }
    *cands = out;
    return n;
}

/* Constructor value position: the visible same-named constructors are
   handed to check as a candidate set; the first one is the recorded
   symbol. */
void r_cons_head(r_ctx *c, r_file *f, const ek_node *n, r_head *out)
{
    const char *name = n->u.path.count == 1 ? n->u.path.data[0] : "";
    const ek_symbol *const *cands = NULL;
    size_t k = r_lookup_ctor(c, f, name, &cands);

    memset(out, 0, sizeof *out);
    if (strcmp(name, "Never") == 0) {
        /* The language's bottom type names no value and has no
           constructor; the miss is not the bridge. */
        r_error(c, f->mod->file, n->line, n->col, "E-unbound-name",
                "`Never` is a type of the language; it names no value "
                "(Clause 7.5.4)");
        out->kind = EK_SYM_EXTERNAL;
        out->st = EK_ST_EXTERNAL;
        r_add_ref(c, n, out->kind, out->st, NULL);
        return;
    }
    if (k == 0) {
        /* A private constructor is unreferenceable rather than
           unknown: std's and cap's `_` spellings diagnose, and the
           handle stays unforgeable (Clause 7.3.3). */
        const ek_symbol *priv = r_std_find(f->std, EK_SYM_CONSTRUCTOR,
                                           name);

        if (!priv) {
            priv = r_std_find(f->cap, EK_SYM_CONSTRUCTOR, name);
        }
        if (priv && !priv->public) {
            r_error(c, f->mod->file, n->line, n->col, "E-private-name",
                    "the constructor `%s` is private in module `%s` "
                    "(Clause 7.3.3)", name, priv->module);
        }
        /* Unknown to the file and to std: the unresolved-external
           bridge (resolve.h). */
        out->kind = EK_SYM_EXTERNAL;
        out->st = EK_ST_EXTERNAL;
        r_add_ref(c, n, out->kind, out->st, NULL);
        return;
    }
    out->sym = cands[0];
    if (r_is_placeholder(cands[0])) {
        out->kind = EK_SYM_EXTERNAL;
        out->st = EK_ST_EXTERNAL;
    } else {
        out->kind = EK_SYM_CONSTRUCTOR;
        out->st = EK_ST_CONS;
    }
    r_add_ctor_ref(c, n, out->kind, out->st, out->sym, cands, k);
}

/* A module name is not a value. */
static void module_value(r_ctx *c, r_file *f, const ek_node *n)
{
    r_error(c, f->mod->file, n->line, n->col, "E-module-value",
            "module `%s` never enters value position (Clause 7.2.6)",
            n->u.path.data[0]);
}

/* A compiler-generated service on every type namespace. The free-call
   form of the name denotes the receiver call with its first argument
   as the receiver, so the chain machinery reads it. */
static int service_name(const char *name)
{
    return strcmp(name, "show") == 0 || strcmp(name, "inspect") == 0
        || strcmp(name, "panic") == 0;
}

/* The expression-head lookup. */
void r_name_head(r_ctx *c, r_file *f, const ek_node *n, int chained,
                 r_head *out)
{
    const char *name = n->u.path.count == 1 ? n->u.path.data[0] : "";
    const ek_symbol *sym;

    memset(out, 0, sizeof *out);
    if (strcmp(name, "_") == 0) {
        /* The unit value at value position, not a name. */
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
        if (r_is_placeholder(sym)) {
            /* A row without a declaration behind it keeps the bridged
               reading (resolve.h). */
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
    } else if (service_name(name)) {
        /* No layer binds it: the free form of the receiver service.
           check supplies the receiver and its reading. */
        out->sym = NULL;
        out->kind = EK_SYM_MEMBER;
        out->st = EK_ST_RECEIVER;
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
           module and namespace readings. */
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
   expected type decides, so keys resolve in check. */
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

/* Interpolation slots are full expressions. */
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
