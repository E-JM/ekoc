#include <string.h>

#include "resolve/internal.h"
#include "resolve/cap_source.h"

/* The capability module, parsed from the embedded cap/cap.eko bytes
   like the std module. Its surface holds the file's own declarations
   (the handle and data types with their constructors, and the Out
   members) plus the two `_`-prefixed rows of the injected pair
   (Clause 10.8): plain functions with no declaration behind them,
   whose signatures check supplies. The nine capability type names stay
   std placeholder rows, and the capability module owns their namespace
   members (Clause 7.9.1). No import path names the module, so its
   surface reaches a file only as the fallback beneath std. */

#define CAP_FILE "cap.eko"

/* The injected pair of the Out capability: the unwrap and wrap of the
   session-write bundle. The spellings are compiler-owned, and no
   source line declares them. */
static const char *injected_names[] = {
    "_out_unwrap", "_out_wrap", NULL
};

/* Appends the injected pair to the module surface. The surface array
   was sized by the upper bound of the source alone, so the pair moves
   to a fresh one; each row is a private function with no declaration
   and no annotation, and check hangs the signatures on these
   symbols. */
static void inject(r_ctx *c, ek_surface *s)
{
    ek_symbol *grown;
    size_t i, cap = s->count + 2;

    grown = ek_arena_alloc(c->arena, cap * sizeof *grown);
    if (!grown) {
        c->oom = 1;
        return;
    }
    memcpy(grown, s->syms, s->count * sizeof *grown);
    s->syms = grown;
    for (i = 0; injected_names[i]; i++) {
        ek_symbol *sym = r_sym(c, injected_names[i], EK_SYM_FUNC, "cap",
                               0, 0);

        if (!sym) {
            return;
        }
        s->syms[s->count++] = *sym;
    }
}

/* Parses the embedded cap source into a module of the closure and
   hangs the injected pair off its surface. */
void r_cap_boot(r_ctx *c)
{
    r_mod *m;
    ek_node *ast;

    if (c->oom || c->cap) {
        return;
    }
    if (r_module_find(c, "cap")) {
        /* Only a root illegally named `cap` claims the stem first; it
           is already rejected, and the closure keeps one module per
           stem. */
        return;
    }
    char *source = ek_cap_source_join(c->arena);

    if (!source) {
        c->oom = 1;
        return;
    }
    ast = r_parse_source(c, source, ek_cap_source_len, CAP_FILE);
    if (!ast) {
        return;
    }
    m = r_module_add(c, "cap", CAP_FILE, ast);
    if (!m || !m->surf) {
        return;
    }
    inject(c, m->surf);
    if (c->oom) {
        return;
    }
    m->state = 2; /* parsed, and it imports nothing */
    c->cap = m->surf;
}

/* The capability surface is the fallback for the file's type and
   constructor lookups beneath the std surface: the handle and data
   types the cap source declares live there, while the nine capability
   names stay std rows. */
void r_cap_open(r_ctx *c, r_file *f)
{
    if (!c->cap) {
        return;
    }
    f->cap = c->cap;
}
