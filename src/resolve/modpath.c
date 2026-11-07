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

/* A module segment continues the walk, as a module-path access from a
   receiver or a plain qualified access. */
int r_module_walk(r_ctx *c, r_file *f, r_head *cur, const ek_node *link,
                       const char *name, int last, int from_receiver,
                       r_mod **walk_mod, const ek_symbol **walk_type,
                       int *state)
{
    const ek_symbol *m;

    if (!*walk_mod || !(*walk_mod)->surf) {
        r_external_link(c, cur, link); /* the std placeholder bridge */
        *state = 0;
        *walk_mod = NULL;
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
        if ((*walk_mod)->surf == c->std
            && r_embedded_type_name(c, name)) {
            /* An embedded module's type has no `std.` spelling, on the
               chain path as on the type path (Clause 9.1.2). */
            r_error(c, f->mod->file, link->line, link->col,
                    "E-unbound-name",
                    "the type `%s` belongs to an embedded module; it "
                    "has no `std.` spelling and opens unqualified "
                    "(Clauses 9.1.1, 9.1.2)", name);
            r_external_link(c, cur, link);
            *state = 0;
            *walk_mod = NULL;
            return 1;
        }
        if ((*walk_mod)->surf == c->std || from_receiver) {
            /* The std placeholder keeps its bridge, and a receiver-form
               miss defers to check: a record field of the same spelling
               wins the link there, and only the receiver's type knows. */
            r_external_link(c, cur, link);
            *state = 0;
            *walk_mod = NULL;
            return 1;
        }
        r_error(c, f->mod->file, link->line, link->col, "E-unbound-member",
                "module `%s` declares no member `%s` (Clause 7.4.4)",
                (*walk_mod)->stem, name);
        r_external_link(c, cur, link);
        *state = 0;
        *walk_mod = NULL;
        return 1;
    }
    if (!m->public) {
        if (from_receiver) {
            /* The same deferral as a miss: the field that wins the
               module segment decides whether the name was ever
               read here. */
            r_external_link(c, cur, link);
            *state = 0;
            *walk_mod = NULL;
            return 1;
        }
        r_error(c, f->mod->file, link->line, link->col, "E-private-name",
                "member `%s` is private in module `%s` (Clause 7.3.1)",
                name, (*walk_mod)->stem);
    }
    if (m->kind == EK_SYM_TYPE) {
        /* A type spelling continues as its namespace; at a value
           position it denotes a constructor of the same spelling. */
        if (!last) {
            cur->sym = m;
            cur->kind = EK_SYM_TYPE;
            cur->st = from_receiver ? EK_ST_MODULE_PATH : EK_ST_QUALIFIED;
            r_add_ref(c, link, cur->kind, cur->st, cur->sym);
            *state = 2; /* Mod.Type.member: the namespace of the type */
            *walk_type = m;
            return 1;
        }
        m = r_module_ctor(*walk_mod, name);
        if (!m) {
            r_error(c, f->mod->file, link->line, link->col,
                    "E-unbound-name",
                    "`%s` names no visible constructor (Clause 7.4.2)",
                    name);
            r_external_link(c, cur, link);
            *state = 0;
            *walk_mod = NULL;
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
    } else {
        /* Derived services attach as defaults; check owns them.
           r_external_link records the reference itself. */
        r_external_link(c, cur, link);
    }
    *state = 0;
    return 1;
}

