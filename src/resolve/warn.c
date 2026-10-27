#include <string.h>

#include "resolve/internal.h"

/* The unused-binding warning over the finished closure. The refs are
   the evidence: a symbol is used when a reference denotes it anywhere
   in the closure, except a reference sitting on its own declaration.
   Tests are roots, so their references count; parameters and names
   beginning with `_` do not participate. */

typedef struct {
    const ek_symbol **slots;
    size_t cap;
} r_used;

static size_t used_hash(const ek_symbol *sym)
{
    const unsigned char *bytes = (const unsigned char *)&sym;
    size_t h = 2166136261u, i;

    for (i = 0; i < sizeof sym; i++) {
        h = (h ^ bytes[i]) * 16777619u;
    }
    return h;
}

static void used_init(r_ctx *c, r_used *u)
{
    size_t cap = 16;

    memset(u, 0, sizeof *u);
    while (cap < c->nrefs * 2) {
        cap *= 2;
    }
    u->slots = ek_arena_alloc(c->arena, cap * sizeof *u->slots);
    if (!u->slots) {
        c->oom = 1;
        return;
    }
    memset(u->slots, 0, cap * sizeof *u->slots);
    u->cap = cap;
}

static void used_put(r_ctx *c, r_used *u, const ek_symbol *sym)
{
    size_t i;

    if (c->oom || !u->cap) {
        return;
    }
    i = used_hash(sym) & (u->cap - 1);
    while (u->slots[i]) {
        if (u->slots[i] == sym) {
            return;
        }
        i = (i + 1) & (u->cap - 1);
    }
    u->slots[i] = sym;
}

static int used_has(const r_used *u, const ek_symbol *sym)
{
    size_t i;

    if (!u->cap) {
        return 0;
    }
    i = used_hash(sym) & (u->cap - 1);
    while (u->slots[i]) {
        if (u->slots[i] == sym) {
            return 1;
        }
        i = (i + 1) & (u->cap - 1);
    }
    return 0;
}

static void mark_refs(r_ctx *c, r_used *u)
{
    size_t i;

    for (i = 0; i < c->nrefs; i++) {
        const ek_ref *r = &c->refs[i];

        if (!r->symbol || r->node == r->symbol->decl) {
            continue;
        }
        used_put(c, u, r->symbol);
    }
}

static void warn_binding(r_ctx *c, const char *file, const ek_symbol *sym)
{
    ek_diag_emit(c->diag, EK_SEV_WARNING, "W-unused", file, sym->line,
                 sym->col, "the binding `%s` is never used", sym->name);
}

static const char *module_file(const r_ctx *c, const char *stem)
{
    size_t i;

    for (i = 0; i < c->nmods; i++) {
        if (c->mods[i].stem && strcmp(c->mods[i].stem, stem) == 0) {
            return c->mods[i].file;
        }
    }
    return c->nmods ? c->mods[0].file : NULL;
}

/* The entry point is a root: the host calls it, so no reference has to
   name it. The injected std module is compiler-owned and never
   warns. */
static int binding_participates(const ek_symbol *sym)
{
    return sym->kind == EK_SYM_FUNC && sym->name[0] != '_'
        && strcmp(sym->name, "main") != 0;
}

static void warn_top_bindings(r_ctx *c, const r_used *u)
{
    size_t i, k;

    for (i = 0; i < c->nmods; i++) {
        const r_mod *m = &c->mods[i];

        if (!m->surf || m->surf == c->std || !m->ast) {
            continue;
        }
        for (k = 0; k < m->surf->count; k++) {
            const ek_symbol *sym = &m->surf->syms[k];

            if (!binding_participates(sym) || used_has(u, sym)) {
                continue;
            }
            warn_binding(c, m->file, sym);
        }
    }
}

/* The alternatives of one or-pattern are one binding: the group is
   used when any alternative's symbol is. A local whose name begins with
   `_` is the discard spelling and does not participate. */
static int local_used(const r_ctx *c, const r_used *u, size_t at)
{
    const r_local *l = &c->locals[at];
    size_t i;

    for (i = at; i < c->nlocals; i++) {
        if (c->locals[i].group == l->group
            && strcmp(c->locals[i].sym->name, l->sym->name) == 0
            && used_has(u, c->locals[i].sym)) {
            return 1;
        }
    }
    return 0;
}

static int local_repeats(const r_ctx *c, size_t at)
{
    const r_local *l = &c->locals[at];
    size_t i;

    for (i = 0; i < at; i++) {
        if (c->locals[i].group == l->group
            && strcmp(c->locals[i].sym->name, l->sym->name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void warn_locals(r_ctx *c, const r_used *u)
{
    size_t i;

    for (i = 0; i < c->nlocals; i++) {
        const r_local *l = &c->locals[i];

        if (l->sym->name[0] == '_' || local_repeats(c, i)
            || local_used(c, u, i)) {
            continue;
        }
        if (l->sym->module && strcmp(l->sym->module, "std") == 0) {
            continue;
        }
        warn_binding(c, module_file(c, l->sym->module), l->sym);
    }
}

void r_warn_unused(r_ctx *c)
{
    r_used u;

    if (c->oom) {
        return;
    }
    used_init(c, &u);
    if (c->oom) {
        return;
    }
    mark_refs(c, &u);
    warn_top_bindings(c, &u);
    warn_locals(c, &u);
}
