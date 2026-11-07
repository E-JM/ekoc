#include <string.h>

#include "resolve/internal.h"

/* The name layers of one file, in lookup order: the lexical scopes,
   the top-level bindings beside the opened free names, the module
   layer, and the type and constructor layers with the std and embedded
   fallbacks beneath them. */

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

/* A free name from std answers only when an import opened it, so a
   file binding of the same spelling owns the name. */
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
        /* The embedded modules are the fallbacks beneath std: the
           handle and data types of the capability surfaces live in the
           system vocabulary, the bundle records in the capability
           module. */
        const ek_symbol *sym = r_embedded_find(f, EK_SYM_TYPE, name);

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
    const ek_symbol *emb_ct = r_embedded_find(f, EK_SYM_CONSTRUCTOR, name);
    const ek_symbol **out;
    int std_fb, emb_fb;
    size_t i, n = 0;

    *cands = NULL;
    /* A private std or embedded constructor never reaches an
       unqualified reference: visibility follows the type, and only the
       declaring module sees it. */
    std_fb = std_ct && std_ct->public
             && !ctor_listed(f->ctors, f->nctors, std_ct);
    emb_fb = emb_ct && emb_ct->public
             && !ctor_listed(f->ctors, f->nctors, emb_ct);
    for (i = 0; i < f->nctors; i++) {
        if (strcmp(f->ctors[i].name, name) == 0) {
            n++;
        }
    }
    if (std_fb) {
        n++;
    }
    if (emb_fb) {
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
    if (emb_fb) {
        out[n++] = emb_ct;
    }
    *cands = out;
    return n;
}
