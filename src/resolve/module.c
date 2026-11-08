#include <string.h>

#include "lex/lex.h"
#include "layout/layout.h"
#include "parse/parse.h"
#include "resolve/internal.h"
#include "resolve/regex_source.h"
#include "resolve/time_source.h"

/* The modules of the closure: stems, registration, and the lex, layout,
   and parse chain over one source. The shipped companions boot here;
   the import lines that load them are import.c's. */

/* The file spelling of a compiler-embedded module: no release
   directory is consulted, and diagnostics name the module, not a
   host path. */
#define EMBEDDED_FILE "(std)"

/* The shipped companions: compiler-embedded release sources under the
   reserved std/ path (Clause 9.1.3). A stem here is reserved for the
   companion, so a user module MUST NOT claim it. */
typedef struct {
    const char *stem;
    char *(*join)(ek_arena *);
    const size_t *len;
} companion;

static const companion companions[] = {
    { "regex", ek_regex_source_join, &ek_regex_source_len },
    { "time", ek_time_source_join, &ek_time_source_len },
    { NULL, NULL, NULL }
};

static const companion *find_companion(const char *stem)
{
    size_t i;

    for (i = 0; companions[i].stem; i++) {
        if (strcmp(stem, companions[i].stem) == 0) {
            return &companions[i];
        }
    }
    return NULL;
}

int r_companion_stem(const char *stem)
{
    return find_companion(stem) != NULL;
}

static char *r_stem_of(r_ctx *c, const char *file)
{
    const char *base = strrchr(file, '/');
    const char *dot;
    size_t len;
    char *out;

    base = base ? base + 1 : file;
    dot = strrchr(base, '.');
    len = dot ? (size_t)(dot - base) : strlen(base);
    out = ek_arena_alloc(c->arena, len + 1);
    if (!out) {
        c->oom = 1;
        return NULL;
    }
    memcpy(out, base, len);
    out[len] = 0;
    return out;
}

char *r_path_stem(r_ctx *c, const char *file)
{
    return r_stem_of(c, file);
}

r_mod *r_module_find(r_ctx *c, const char *stem)
{
    size_t i;

    for (i = 0; i < c->nmods; i++) {
        if (strcmp(c->mods[i].stem, stem) == 0) {
            return &c->mods[i];
        }
    }
    return NULL;
}

static r_mod *register_mod(r_ctx *c, const char *stem, const char *file,
                           const ek_node *ast, int external)
{
    r_mod *m;

    if (c->nmods == c->cmods) {
        c->mods = r_grow(c, c->mods, c->nmods, &c->cmods, sizeof *c->mods);
        if (c->oom) {
            return NULL;
        }
    }
    m = &c->mods[c->nmods++];
    m->stem = stem;
    m->file = file;
    m->ast = ast;
    m->surf = NULL;
    m->state = 0;
    m->external = external;
    if (!external) {
        r_surface_build(c, m);
    }
    return m;
}

/* Registers one parsed module in the closure and builds its surface. */
r_mod *r_module_add(r_ctx *c, const char *stem, const char *file,
                    const ek_node *ast)
{
    return register_mod(c, stem, file, ast, 0);
}

/* Registers a reserved std companion: a stem an import opens with no
   file behind it. */
r_mod *r_module_companion(r_ctx *c, const char *stem)
{
    return register_mod(c, stem, EMBEDDED_FILE, NULL, 1);
}

int r_shipped_companion(const r_mod *m)
{
    return m && !m->external && strcmp(m->file, EMBEDDED_FILE) == 0;
}

void r_load_companion(r_ctx *c, const char *stem)
{
    const companion *cp = find_companion(stem);
    char *source;
    ek_node *ast;
    r_mod *m;

    if (!cp) {
        return; /* unreachable: r_companion_stem gates the call */
    }
    source = cp->join(c->arena);
    if (!source) {
        c->oom = 1;
        return;
    }
    ast = r_parse_source(c, source, *cp->len, EMBEDDED_FILE);
    if (!ast) {
        return;
    }
    m = r_module_add(c, stem, EMBEDDED_FILE, ast);
    if (!m || !m->surf) {
        return;
    }
    r_closure_walk(c, m);
}

/* One diagnostic when a stage of the chain hands back no stream or no
   AST. The stage contracts (lex.h, layout.h, parse.h) make that an
   arena failure, so the walk recovers with the import failed. */
static void stage_dropped(r_ctx *c, const char *path, const char *stage)
{
    ek_diag_emit(c->diag, EK_SEV_ERROR, NULL, path, 0, 0,
                 "the %s stage produced no output for this module; the "
                 "import fails", stage);
    c->oom = 1;
}

/* Lex, layout, and parse one module source. */
ek_node *r_parse_source(r_ctx *c, const char *src, size_t len,
                        const char *path)
{
    ek_lex_result lex;
    ek_layout_result lay;
    ek_node *mod;

    lex = ek_lex(src, len, path, c->arena, c->diag);
    if (!lex.tokens || lex.count == 0) {
        stage_dropped(c, path, "lex");
        return NULL;
    }
    lay = ek_layout(lex.tokens, lex.count, path, c->arena, c->diag);
    if (!lay.tokens || lay.count == 0) {
        stage_dropped(c, path, "layout");
        return NULL;
    }
    mod = ek_parse(lay.tokens, lay.count, path, c->arena, c->diag).module;
    if (!mod) {
        stage_dropped(c, path, "parse");
    }
    return mod;
}

/* A stem spells the native identifier of every symbol its module emits,
   so its bytes stay in the portable set the clause names: the ASCII
   letters, digits, and underscore the identifier rule already admits. */
static int stem_ascii(const char *stem)
{
    const unsigned char *p;

    for (p = (const unsigned char *)stem; *p; p++) {
        if (*p > 0x7f) {
            return 0;
        }
    }
    return 1;
}

/* A stem is an ASCII identifier, snake_case, never begins with `_`, and
   is never `std`; the reserved names the callers judge beside it, so the
   std/ paths return before this call and the root check names the
   embedded modules. */
int r_stem_legal(r_ctx *c, const char *file, int line, int col,
                 const char *stem)
{
    if (!r_is_ident(stem)) {
        r_error(c, file, line, col, "E-name-case",
                "module name `%s` is not an identifier (Clauses 5.5.1, "
                "7.2.1)", stem);
        return 0;
    }
    if (!stem_ascii(stem)) {
        r_error(c, file, line, col, "E-name-case",
                "module name `%s` is not ASCII (Clause 7.2.1)", stem);
        return 0;
    }
    if (!r_is_snake(stem)) {
        r_error(c, file, line, col, "E-name-case",
                "module name `%s` is not snake_case (Clauses 5.5.2, "
                "7.2.1)", stem);
        return 0;
    }
    if (stem[0] == '_') {
        r_error(c, file, line, col, "E-module-name",
                "module file `%s` begins with `_` (Clause 7.2.1)", stem);
        return 0;
    }
    if (strcmp(stem, "std") == 0) {
        r_error(c, file, line, col, "E-module-name",
                "a user module MUST NOT be named `std` (Clauses 7.2.2, "
                "9.1.2)");
        return 0;
    }
    return 1;
}
