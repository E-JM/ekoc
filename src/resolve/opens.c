#include <string.h>

#include "resolve/internal.h"

/* The per-file opened free-name table. An import's selective list names
   free functions of the target module; those names join the file's
   top-level layer. Collisions are judged at the declaration point: a
   name opened twice, or opened over a file top-level binding, is a
   conflict. Locals shadow the layer legally. */

/* The entry of name in the opened table, or NULL. */
static const r_open *open_find(const r_file *f, const char *name)
{
    size_t i;

    for (i = 0; i < f->nopens; i++) {
        if (strcmp(f->opens[i].name, name) == 0) {
            return &f->opens[i];
        }
    }
    return NULL;
}

/* Opens one free name. The prior entry decides the message: the same
   import's list, the injected std list, or another import line. */
void r_open_name(r_ctx *c, r_file *f, const char *name,
                 const ek_symbol *sym, const char *module,
                 const ek_node *at)
{
    const r_open *hit;
    r_open *slot;

    if (c->oom) {
        return;
    }
    hit = open_find(f, name);
    if (hit) {
        if (at && hit->at == at) {
            r_error(c, f->mod->file, at->line, at->col, "E-name-conflict",
                    "`%s` is listed twice in this import (Clause 7.4.6)",
                    name);
        } else if (!hit->at) {
            r_error(c, f->mod->file, at->line, at->col, "E-name-conflict",
                    "`%s` is already opened by the implicit std import "
                    "(Clauses 7.2.3, 7.4.6)", name);
        } else {
            r_error(c, f->mod->file, at->line, at->col, "E-name-conflict",
                    "`%s` is already opened by another import "
                    "(Clauses 7.2.3, 7.4.6)", name);
        }
        return;
    }
    if (f->nopens == f->copens) {
        f->opens = r_grow(c, f->opens, f->nopens, &f->copens,
                          sizeof *f->opens);
        if (c->oom) {
            return;
        }
    }
    slot = &f->opens[f->nopens++];
    slot->name = name;
    slot->sym = sym;
    slot->module = module;
    slot->at = at;
}

/* One listed name of a real module: a public free function of the
   target surface, or a diagnostic. */
static void open_member(r_ctx *c, r_file *f, const r_mod *target,
                        const ek_node *imp, const char *name)
{
    const ek_symbol *sym = r_module_member(target, name);

    if (!sym || sym->kind != EK_SYM_FUNC) {
        r_error(c, f->mod->file, imp->line, imp->col, "E-unbound-member",
                "module `%s` exposes no free function `%s` "
                "(Clauses 7.2.3, 7.4.4)", target->stem, name);
        return;
    }
    if (!sym->public) {
        r_error(c, f->mod->file, imp->line, imp->col, "E-private-name",
                "member `%s` is private in module `%s` (Clause 7.3.1)",
                name, target->stem);
        return;
    }
    r_open_name(c, f, name, sym, target->stem, imp);
}

/* The std placeholder knows exactly the injected free names; the
   injected import already opened them, so a repeated one adds
   nothing. */
static void open_std(r_ctx *c, r_file *f, const ek_node *imp,
                     const char *name)
{
    if (!r_std_find(c->std, EK_SYM_FUNC, name)) {
        r_error(c, f->mod->file, imp->line, imp->col, "E-unbound-member",
                "std exposes no free function `%s` (Clauses 7.2.3, "
                "9.1.2)", name);
    }
}

/* Validates and opens every name of one import's list. */
void r_opened_import(r_ctx *c, r_file *f, const r_mod *target,
                     const ek_node *imp)
{
    size_t i;

    for (i = 0; i < imp->u.imports.names.count && !c->oom; i++) {
        const char *name = imp->u.imports.names.data[i];

        if (target->external) {
            open_std(c, f, imp, name);
        } else {
            open_member(c, f, target, imp, name);
        }
    }
}

/* Opens over a file top-level binding are conflicts at the binding's
   declaration; the binding itself was collected before the imports
   were fed, so the whole table is compared at once. */
void r_opened_vs_tops(r_ctx *c, r_file *f)
{
    size_t i, k;

    for (i = 0; i < f->nopens && !c->oom; i++) {
        const r_open *o = &f->opens[i];

        for (k = 0; k < f->ntops; k++) {
            const r_var *v = &f->tops[k];

            if (strcmp(v->name, o->name) != 0) {
                continue;
            }
            if (!o->at) {
                r_error(c, f->mod->file, v->sym->line, v->sym->col,
                        "E-name-conflict",
                        "top-level binding `%s` collides with a free name "
                        "the implicit std import opens (Clauses 7.2.3, "
                        "7.4.6)", o->name);
            } else {
                r_error(c, f->mod->file, v->sym->line, v->sym->col,
                        "E-name-conflict",
                        "top-level binding `%s` collides with a free name "
                        "the import of `%s` opens (Clauses 7.2.3, 7.4.6)",
                        o->name, o->module);
            }
        }
    }
}
