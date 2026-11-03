#include <string.h>

#include "resolve/internal.h"
#include "resolve/cap_source.h"
#include "resolve/sys_source.h"

/* The compiler-embedded modules beneath std, parsed from their embedded
   bytes like the std module and booted in lookup order: the system
   vocabulary module (the handle and data types of the capability
   surfaces, with their constructors) and the capability module (the
   capability members and the bundle records, plus the `_`-prefixed rows
   of the injected pairs: plain functions with no declaration behind
   them, whose signatures check supplies). The nine capability type
   names stay std placeholder rows, and the capability module owns their
   namespace members. No import path names either module, so each
   surface reaches a file only as a fallback beneath std -- the embedded
   files included, which is how the capability source names the
   vocabulary without an import line. */

/* The injected pairs, one per capability: the unwrap and wrap of each
   session bundle. The spellings are compiler-owned, and no source line
   declares them. */
static const char *injected_names[] = {
    "_out_unwrap", "_out_wrap",
    "_in_unwrap", "_in_wrap",
    "_fs_unwrap", "_fs_wrap",
    "_env_unwrap", "_env_wrap",
    "_args_unwrap", "_args_wrap",
    "_clock_unwrap", "_clock_wrap",
    "_rand_unwrap", "_rand_wrap",
    NULL
};

#define INJECTED_COUNT (sizeof injected_names / sizeof *injected_names - 1)

/* One embedded module: its stem, the file name diagnostics report, the
   join of its embedded bytes and their length, and whether the injected
   pairs hang off its surface. The array order is the lookup order. */
typedef struct {
    const char *stem;
    const char *file;
    char *(*join)(ek_arena *);
    const size_t *len;
    int inject;
} embedded_module;

static const embedded_module embedded[R_EMBEDDED] = {
    { "sys", "sys.eko", ek_sys_source_join, &ek_sys_source_len, 0 },
    { "cap", "cap.eko", ek_cap_source_join, &ek_cap_source_len, 1 },
};

/* Appends the injected pairs to the module surface. The surface array
   was sized by the upper bound of the source alone, so the pairs move
   to a fresh one; each row is a private function with no declaration
   and no annotation, and check hangs the signatures on these
   symbols. */
static void inject(r_ctx *c, ek_surface *s, const char *stem)
{
    ek_symbol *grown;
    size_t i, cap = s->count + INJECTED_COUNT;

    grown = ek_arena_alloc(c->arena, cap * sizeof *grown);
    if (!grown) {
        c->oom = 1;
        return;
    }
    memcpy(grown, s->syms, s->count * sizeof *grown);
    s->syms = grown;
    for (i = 0; i < INJECTED_COUNT; i++) {
        ek_symbol *sym = r_sym(c, injected_names[i], EK_SYM_FUNC, stem,
                               0, 0);

        if (!sym) {
            return;
        }
        s->syms[s->count++] = *sym;
    }
}

/* Parses one embedded source into a module of the closure and records
   its surface at its lookup position. */
static void boot_one(r_ctx *c, size_t i)
{
    const embedded_module *em = &embedded[i];
    r_mod *m;
    ek_node *ast;
    char *source;

    if (c->oom || c->embedded[i]) {
        return;
    }
    if (!c->std || (i > 0 && !c->embedded[i - 1])) {
        /* Every embedded source reads the std layer and the embedded
           surfaces before it; when an illegally named root kept one of
           them from booting, the closure is already rejected and the
           module stays out rather than resolving against a broken
           surface. */
        return;
    }
    if (r_module_find(c, em->stem)) {
        /* Only a root illegally named by the stem claims it first; it
           is already rejected, and the closure keeps one module per
           stem. */
        return;
    }
    source = em->join(c->arena);
    if (!source) {
        c->oom = 1;
        return;
    }
    ast = r_parse_source(c, source, *em->len, em->file);
    if (!ast) {
        return;
    }
    m = r_module_add(c, em->stem, em->file, ast);
    if (!m || !m->surf) {
        return;
    }
    if (em->inject) {
        inject(c, m->surf, em->stem);
        if (c->oom) {
            return;
        }
    }
    /* An embedded source may carry an ordinary import of a shipped
       companion (std/time); the shared walk registers the companion in
       the closure before any file's bodies resolve. */
    r_closure_walk(c, m);
    c->embedded[i] = m->surf;
}

void r_embedded_boot(r_ctx *c)
{
    size_t i;

    for (i = 0; i < R_EMBEDDED; i++) {
        boot_one(c, i);
    }
}

/* The embedded surfaces are the fallbacks for the file's type and
   constructor lookups beneath the std surface, in boot order. */
void r_embedded_open(r_ctx *c, r_file *f)
{
    size_t i;

    for (i = 0; i < R_EMBEDDED; i++) {
        f->embedded[i] = c->embedded[i];
    }
}

const ek_symbol *r_embedded_find(const r_file *f, ek_sym_kind kind,
                                 const char *name)
{
    size_t i;

    for (i = 0; i < R_EMBEDDED; i++) {
        const ek_symbol *sym;

        if (!f->embedded[i]) {
            continue;
        }
        sym = r_std_find(f->embedded[i], kind, name);
        if (sym) {
            return sym;
        }
    }
    return NULL;
}

/* True when a booted embedded module declares the type name: such a
   name opens unqualified in every file and carries no `std.` spelling
   (Clause 9.1.2). */
int r_embedded_type_name(r_ctx *c, const char *name)
{
    size_t i, k;

    for (i = 0; i < R_EMBEDDED; i++) {
        if (!c->embedded[i]) {
            continue;
        }
        for (k = 0; k < c->embedded[i]->count; k++) {
            const ek_symbol *sym = &c->embedded[i]->syms[k];

            if (sym->kind == EK_SYM_TYPE && strcmp(sym->name, name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}
