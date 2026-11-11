#include <string.h>

#include "resolve/internal.h"

/* The module-path walk's legs: the surface lookups of a module
   segment, the bridge a miss keeps, and the continuations the chain
   walk enters -- a module segment descends its surface, and a type
   namespace descends the owner module's (Clauses 7.4.4, 7.4.5). */

const ek_symbol *r_module_member(const r_mod *m, const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        /* tests and namespace members are not module surface names */
        if (s->kind != EK_SYM_TEST && s->kind != EK_SYM_NAMESPACE
            && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* A module's constructor of one spelling; a type MAY share the
   spelling, so the kind is decided here, not by surface order. */
const ek_symbol *r_module_ctor(const r_mod *m, const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        if (s->kind == EK_SYM_CONSTRUCTOR && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* A module's constructor of one spelling owned by one type: the
   type-namespace member a `Type.Ctor` or `Mod.Type.Ctor` spelling
   names (Clause 7.4.4). */
const ek_symbol *r_module_owned_ctor(const r_mod *m, const char *type,
                                     const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        if (s->kind == EK_SYM_CONSTRUCTOR && s->owner
            && strcmp(s->owner, type) == 0 && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* A module's type of one spelling; a constructor may share it, and a
   namespace link continues at the type, never at the constructor. */
static const ek_symbol *module_type(const r_mod *m, const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        if (s->kind == EK_SYM_TYPE && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

static const ek_symbol *namespace_member(const r_mod *m, const char *type,
                                         const char *name)
{
    size_t i;

    if (!m || !m->surf) {
        return NULL;
    }
    for (i = 0; i < m->surf->count; i++) {
        const ek_symbol *s = &m->surf->syms[i];

        if (s->kind == EK_SYM_NAMESPACE && s->owner
            && strcmp(s->owner, type) == 0 && strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

void r_external_link(r_ctx *c, r_head *cur, const ek_node *link)
{
    cur->sym = NULL;
    cur->kind = EK_SYM_EXTERNAL;
    cur->st = EK_ST_EXTERNAL;
    r_add_ref(c, link, cur->kind, cur->st, NULL);
}

/* The bridge and stop: the link records an unresolved external and the
   walk is over. */
static void bridge_stop(r_ctx *c, r_head *cur, const ek_node *link,
                        r_mod **walk_mod, int *state)
{
    r_external_link(c, cur, link);
    *state = 0;
    *walk_mod = NULL;
}

/* The segment names no member: an embedded module's type has no `std.`
   spelling, the std placeholder and the receiver form keep the bridge,
   and a plain miss is unbound. */
static void module_miss(r_ctx *c, r_file *f, r_head *cur,
                        const ek_node *link, const char *name,
                        int from_receiver, r_mod **walk_mod, int *state)
{
    if ((*walk_mod)->surf == c->std && r_embedded_type_name(c, name)) {
        /* An embedded module's type has no `std.` spelling, on the
           chain path as on the type path (Clause 9.1.2). */
        r_error(c, f->mod->file, link->line, link->col, "E-unbound-name",
                "the type `%s` belongs to an embedded module; it has no "
                "`std.` spelling and opens unqualified (Clauses 9.1.1, "
                "9.1.2)", name);
        bridge_stop(c, cur, link, walk_mod, state);
        return;
    }
    if (from_receiver) {
        /* A receiver-form miss defers to check: a record field of the
           same spelling wins the link there, and only the receiver's
           type knows. */
        bridge_stop(c, cur, link, walk_mod, state);
        return;
    }
    if ((*walk_mod)->surf == c->std) {
        /* The std placeholder keeps its bridge at the head position;
           the naming of its misses lands with the bridge tightening. */
        bridge_stop(c, cur, link, walk_mod, state);
        return;
    }
    r_error(c, f->mod->file, link->line, link->col, "E-unbound-member",
            "module `%s` declares no member `%s` (Clause 7.4.4)",
            (*walk_mod)->stem, name);
    bridge_stop(c, cur, link, walk_mod, state);
}

/* The member the segment names: a private spelling defers to the
   receiver reading or diagnoses, a type spelling continues as its
   namespace mid-chain and denotes its constructor at the last link. */
static int module_found(r_ctx *c, r_file *f, r_head *cur,
                        const ek_node *link, const char *name,
                        const ek_symbol *m, int last, int from_receiver,
                        r_mod **walk_mod, const ek_symbol **walk_type,
                        int *state)
{
    if (!m->public && from_receiver) {
        /* The same deferral as a miss: the field that wins the
           module segment decides whether the name was ever
           read here. */
        bridge_stop(c, cur, link, walk_mod, state);
        return 1;
    }
    if (!m->public) {
        r_error(c, f->mod->file, link->line, link->col, "E-private-name",
                "member `%s` is private in module `%s` (Clause 7.3.1)",
                name, (*walk_mod)->stem);
    }
    if (m->kind == EK_SYM_TYPE && !last) {
        /* A type spelling continues as its namespace; at a value
           position it denotes a constructor of the same spelling. */
        cur->sym = m;
        cur->kind = EK_SYM_TYPE;
        cur->st = from_receiver ? EK_ST_MODULE_PATH : EK_ST_QUALIFIED;
        r_add_ref(c, link, cur->kind, cur->st, cur->sym);
        *state = 2; /* Mod.Type.member: the namespace of the type */
        *walk_type = m;
        return 1;
    }
    if (m->kind == EK_SYM_TYPE) {
        m = r_module_ctor(*walk_mod, name);
        if (!m) {
            r_error(c, f->mod->file, link->line, link->col,
                    "E-unbound-name",
                    "`%s` names no visible constructor (Clause 7.4.2)",
                    name);
            bridge_stop(c, cur, link, walk_mod, state);
            return 1;
        }
    }
    cur->sym = m;
    cur->kind = m->kind;
    cur->st = from_receiver ? EK_ST_MODULE_PATH : EK_ST_QUALIFIED;
    r_add_ref(c, link, cur->kind, cur->st, cur->sym);
    *state = 0;
    *walk_mod = NULL;
    return 1;
}

/* A module segment continues the walk, as a module-path access from a
   receiver or a plain qualified access. */
int r_module_walk(r_ctx *c, r_file *f, r_head *cur, const ek_node *link,
                  const char *name, int last, int from_receiver,
                  r_mod **walk_mod, const ek_symbol **walk_type, int *state)
{
    const ek_symbol *m;

    if (!*walk_mod || !(*walk_mod)->surf) {
        bridge_stop(c, cur, link, walk_mod, state); /* the std placeholder bridge */
        return 1;
    }
    m = r_module_member(*walk_mod, name);
    if (!last) {
        /* A namespace link continues at the type a shared spelling
           may hide behind a constructor row. */
        const ek_symbol *t = module_type(*walk_mod, name);

        if (t) {
            m = t;
        }
    }
    if (!m) {
        module_miss(c, f, cur, link, name, from_receiver, walk_mod, state);
        return 1;
    }
    return module_found(c, f, cur, link, name, m, last, from_receiver,
                        walk_mod, walk_type, state);
}

int r_namespace_walk(r_ctx *c, r_file *f, r_head *cur,
                          const ek_node *link, const char *name,
                          const ek_symbol *type, r_mod *owner,
                          int from_receiver, int *state)
{
    const ek_symbol *nm = namespace_member(owner, type->name, name);

    if (nm) {
        if (!nm->public) {
            r_error(c, f->mod->file, link->line, link->col,
                    "E-private-name",
                    "namespace member `%s.%s` is private (Clause 7.3.1)",
                    type->name, name);
        }
        cur->sym = nm;
        cur->kind = EK_SYM_NAMESPACE;
        cur->st = from_receiver ? EK_ST_MODULE_PATH : EK_ST_QUALIFIED;
        r_add_ref(c, link, cur->kind, cur->st, cur->sym);
    } else if (!from_receiver
               && (nm = r_module_owned_ctor(owner, type->name, name)) != NULL) {
        /* The type's own constructor: `Type.Ctor` and `Mod.Type.Ctor`
           anchor it with the type as provenance (Clause 7.4.4). */
        if (!nm->public) {
            r_error(c, f->mod->file, link->line, link->col,
                    "E-private-name",
                    "constructor `%s.%s` is private (Clause 7.3.1)",
                    type->name, name);
        }
        cur->sym = nm;
        cur->kind = EK_SYM_CONSTRUCTOR;
        cur->st = EK_ST_QUALIFIED;
        r_add_ref(c, link, cur->kind, cur->st, cur->sym);
    } else {
        /* Derived services attach as defaults, and the compiler-owned
           namespaces keep their rows in check; the walk bridges and a
           miss is named there (Clauses 7.4.3, 7.4.4).
           r_external_link records the reference itself. */
        r_external_link(c, cur, link);
    }
    *state = 0;
    return 1;
}

