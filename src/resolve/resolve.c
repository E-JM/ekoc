#include <string.h>

#include "resolve/internal.h"

/* The entry point. Per file it assembles the name layers from the
   module surface, the opened imports, and the implicit std import;
   then it resolves the bodies, root first. */

static void push_var(r_ctx *c, r_var **vars, size_t *n, size_t *cap,
                     const char *name, const ek_symbol *sym)
{
    if (c->oom) {
        return;
    }
    if (*n == *cap) {
        *vars = r_grow(c, *vars, *n, cap, sizeof **vars);
        if (c->oom) {
            return;
        }
    }
    (*vars)[*n].name = name;
    (*vars)[*n].sym = sym;
    (*n)++;
}

static void open_surface(r_ctx *c, r_file *f, const ek_surface *s)
{
    size_t i;

    if (!s) {
        return;
    }
    for (i = 0; i < s->count; i++) {
        const ek_symbol *sym = &s->syms[i];

        switch (sym->kind) {
        case EK_SYM_FUNC:
        case EK_SYM_TEST:
        case EK_SYM_EXTERN:
            push_var(c, &f->tops, &f->ntops, &f->ctops, sym->name, sym);
            break;
        case EK_SYM_TYPE:
            push_var(c, &f->types, &f->ntypes, &f->ctypes, sym->name, sym);
            break;
        case EK_SYM_CONSTRUCTOR:
            push_var(c, &f->ctors, &f->nctors, &f->cctors, sym->name, sym);
            break;
        default:
            break;
        }
    }
}

/* An import opens the target module's public types and constructors
   into unqualified scope; functions stay module-qualified. Variant
   visibility follows the type. */
static void open_import(r_ctx *c, r_file *f, const r_mod *target)
{
    const ek_surface *s = target->surf;
    size_t i;

    if (!s) {
        return;
    }
    for (i = 0; i < s->count; i++) {
        const ek_symbol *sym = &s->syms[i];

        if (sym->kind == EK_SYM_TYPE && sym->public) {
            push_var(c, &f->types, &f->ntypes, &f->ctypes, sym->name, sym);
        } else if (sym->kind == EK_SYM_CONSTRUCTOR && sym->public) {
            push_var(c, &f->ctors, &f->nctors, &f->cctors, sym->name, sym);
        }
    }
}

static void add_module_name(r_ctx *c, r_file *f, const char *stem)
{
    ek_symbol *sym;

    if (r_lookup_mod(f, stem)) {
        return;
    }
    sym = r_sym(c, stem, EK_SYM_MODULE, stem, 0, 0);
    if (sym) {
        push_var(c, &f->mods, &f->nmods, &f->cmods, stem, sym);
    }
}

static void file_setup(r_ctx *c, r_file *f, r_mod *m)
{
    const ek_node *mod = m->ast;
    size_t i;

    memset(f, 0, sizeof *f);
    f->mod = m;
    add_module_name(c, f, m->stem);
    add_module_name(c, f, "std"); /* the implicit import */
    open_surface(c, f, m->surf);
    r_std_open(c, f);
    if (mod) {
        for (i = 0; i < mod->u.kids.count; i++) {
            const ek_node *imp = mod->u.kids.data[i];
            const char *stem;
            r_mod *target;

            if (imp->kind != EK_N_IMPORT
                || imp->u.imports.path.count == 0) {
                continue;
            }
            stem = imp->u.imports.path.data[imp->u.imports.path.count - 1];
            target = r_module_find(c, stem);
            if (!target) {
                continue; /* a failed import is already diagnosed */
            }
            if (!r_lookup_mod(f, stem)) {
                /* One open per stem: a repeated import line opens no
                   type or constructor twice. */
                add_module_name(c, f, stem);
                if (!target->external) {
                    open_import(c, f, target);
                }
            }
            r_opened_import(c, f, target, imp);
        }
    }
    r_opened_vs_tops(c, f);
}

/* The stem is a snake_case identifier; a user module is never named
   std. */
static void check_root_name(r_ctx *c, const char *file, const char *stem)
{
    if (!r_is_snake(stem)) {
        r_error(c, file, 1, 1, "E-name-case",
                "module name `%s` is not snake_case (Clauses 5.5.2, "
                "7.2.1)", stem);
    }
    if (stem[0] == '_') {
        r_error(c, file, 1, 1, "E-module-name",
                "module file `%s` begins with `_` (Clause 7.2.1)", stem);
    }
    if (strcmp(stem, "std") == 0) {
        r_error(c, file, 1, 1, "E-module-name",
                "a user module MUST NOT be named `std` (Clauses 7.2.2, "
                "9.1.2)");
    }
}

/* The root occupies slot 0 of the closure. */
static r_mod *register_root(r_ctx *c, const ek_node *module, const char *file,
                            char *stem)
{
    r_mod *root;

    if (c->nmods == c->cmods) {
        c->mods = r_grow(c, c->mods, c->nmods, &c->cmods, sizeof *c->mods);
        if (c->oom) {
            return NULL;
        }
    }
    root = &c->mods[c->nmods++];
    root->stem = stem;
    root->file = file;
    root->ast = module;
    root->surf = NULL;
    root->state = 0;
    root->external = 0;
    return root;
}

/* Every module in the closure resolves its bodies, root first. The
   refs of one module form a contiguous slice of the table. */
static void resolve_bodies(r_ctx *c)
{
    r_file f;
    size_t i, n;

    for (i = 0, n = c->nmods; i < n; i++) {
        r_mod *m = &c->mods[i];

        if (m->external || !m->ast) {
            continue;
        }
        file_setup(c, &f, m);
        m->ref_begin = c->nrefs;
        r_tops(c, &f, m->ast);
        m->ref_end = c->nrefs;
    }
}

/* The result surfaces in closure order, root first, and the refs
   sorted by node address for check's binary search. */
static void finish(r_ctx *c, ek_resolve_result *r)
{
    size_t i;

    r_warn_unused(c);
    r->modules = ek_arena_alloc(c->arena, (c->nmods ? c->nmods : 1)
                                * sizeof *r->modules);
    if (!r->modules) {
        return;
    }
    for (i = 0; i < c->nmods; i++) {
        if (!c->mods[i].external && c->mods[i].surf) {
            r->modules[r->module_count] = *c->mods[i].surf;
            r->modules[r->module_count].ast = c->mods[i].ast;
            r->module_count++;
        }
    }
    r_refs_sort(c->refs, c->nrefs);
    r->refs = c->refs;
    r->ref_count = c->nrefs;
}

ek_resolve_result ek_resolve(const ek_node *module, const char *file,
                             const char *const *path, size_t path_count,
                             ek_arena *arena, ek_diag *diag)
{
    r_ctx ctx;
    ek_resolve_result r;
    char *stem;

    memset(&ctx, 0, sizeof ctx);
    memset(&r, 0, sizeof r);
    if (!module || !file) {
        return r;
    }
    ctx.arena = arena;
    ctx.diag = diag;
    ctx.path = path;
    ctx.path_count = path_count;
    stem = r_path_stem(&ctx, file);
    if (!stem) {
        return r;
    }
    check_root_name(&ctx, file, stem);
    if (!register_root(&ctx, module, file, stem)) {
        return r;
    }
    r_std_boot(&ctx);
    r_surface_build(&ctx, ctx.mods);
    r_closure(&ctx, ctx.mods);
    resolve_bodies(&ctx);
    finish(&ctx, &r);
    return r;
}
