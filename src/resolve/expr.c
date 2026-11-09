#include <string.h>

#include "resolve/internal.h"

/* Expressions: the expression-head lookup order (parameter, local
   binding, file top-level binding or opened free name, imported
   module, compiler service), the constructor reading, and the per-form
   dispatch. Dotted heads go to chain.c. */

/* The constructor value miss: a private std or embedded spelling
   diagnoses as private, and everything else is an unbound name. */
static void cons_miss(r_ctx *c, r_file *f, const ek_node *n,
                      const char *name, r_head *out)
{
    /* A private constructor is unreferenceable rather than
       unknown: the `_` spellings of std and the embedded modules
       diagnose, and the handle stays unforgeable (Clause 7.3.3). */
    const ek_symbol *priv = r_std_find(f->std, EK_SYM_CONSTRUCTOR, name);

    if (!priv) {
        priv = r_embedded_find(f, EK_SYM_CONSTRUCTOR, name);
    }
    if (priv && !priv->public) {
        r_error(c, f->mod->file, n->line, n->col, "E-private-name",
                "the constructor `%s` is private in module `%s` "
                "(Clause 7.3.3)", name, priv->module);
    }
    /* Unknown to the file, std, and the embedded modules: an
       unbound name, not the unresolved-external bridge
       (resolve.h). */
    r_error(c, f->mod->file, n->line, n->col, "E-unbound-name",
            "`%s` names no visible constructor (Clause 7.4.2)", name);
    out->kind = EK_SYM_EXTERNAL;
    out->st = EK_ST_EXTERNAL;
    r_add_ref(c, n, out->kind, out->st, NULL);
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
        cons_miss(c, f, n, name, out);
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

/* The snake name over the layers: a local, a top-level binding or
   opened free name, a module, the C namespace, a receiver service, or
   unbound. */
static void name_layers(r_ctx *c, r_file *f, const ek_node *n,
                        const char *name, int chained, r_head *out)
{
    const ek_symbol *sym;

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
    } else if (strcmp(name, "C") == 0) {
        /* The C namespace of the native interface: no module surface
           stands behind the name, so the qualified walk continues
           without one and the members keep the bridged reading
           (resolve.h). */
        out->kind = EK_SYM_MODULE;
        out->st = EK_ST_HEAD_MODULE;
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

/* The expression-head lookup. */
void r_name_head(r_ctx *c, r_file *f, const ek_node *n, int chained,
                 r_head *out)
{
    const char *name = n->u.path.count == 1 ? n->u.path.data[0] : "";

    memset(out, 0, sizeof *out);
    if (strcmp(name, "_") == 0) {
        /* The unit value at value position, not a name. */
        return;
    }
    if (!r_is_snake(name)) {
        r_cons_head(c, f, n, out);
        return;
    }
    name_layers(c, f, n, name, chained, out);
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
   expected type decides, so a bare name stays unread here -- check
   reads the fields. A key that is not a bare name can only be a
   dictionary key expression, and it resolves here. A bare name that a
   local or a top-level binding carries resolves as that value: a
   record field spelling survives untouched (the field path reads the
   name, never the ref), and a dictionary over a runtime key lowers. */
static void r_items(r_ctx *c, r_file *f, const ek_node *e)
{
    size_t i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind != EK_N_COMPOSITE_ITEM) {
            continue;
        }
        r_expr(c, f, item->u.pair.b);
        if (item->u.pair.a && (item->u.pair.a->kind != EK_N_NAME
                               || item->u.pair.a->u.path.count != 1)) {
            r_expr(c, f, item->u.pair.a);
        } else if (item->u.pair.a) {
            const char *name = item->u.pair.a->u.path.data[0];
            const ek_symbol *sym;

            if (strcmp(name, "_") != 0) {
                if ((sym = r_lookup_local(f, name)) != NULL) {
                    r_add_ref(c, item->u.pair.a, sym->kind,
                              EK_ST_HEAD_LOCAL, sym);
                } else if ((sym = r_lookup_top(f, name)) != NULL) {
                    r_add_ref(c, item->u.pair.a,
                              r_is_placeholder(sym) ? EK_SYM_EXTERNAL
                                                    : sym->kind,
                              EK_ST_EXTERNAL, sym);
                } else {
                    /* No binding carries the name: the record-field
                       reading or the checker's own report answers. The
                       ref keeps the table covering every key site. */
                    r_add_ref(c, item->u.pair.a, EK_SYM_EXTERNAL,
                              EK_ST_EXTERNAL, NULL);
                }
            }
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
