#include <stdio.h>
#include <string.h>

#include "lex/lex.h"
#include "layout/layout.h"
#include "parse/parse.h"
#include "resolve/internal.h"
#include "resolve/time_source.h"
#include "support/buf.h"

/* Loading modules and walking the import closure. Paths resolve
   against the module path directories in order; the dependency graph
   is walked depth first, first import first, and must be acyclic. */

/* The file spelling of a compiler-embedded module: no release
   directory is consulted, and diagnostics name the module, not a
   host path. */
#define EMBEDDED_FILE "(std)"

/* The stems of the shipped companions: compiler-embedded release
   sources under the reserved std/ path (Clause 9.1.3). A stem here is
   reserved for the companion, so a user module MUST NOT claim it; a
   future companion adds its stem, its source table, and its boot. */
static const char *const companion_stems[] = {
    "time",
    NULL
};

int r_companion_stem(const char *stem)
{
    size_t i;

    for (i = 0; companion_stems[i]; i++) {
        if (strcmp(stem, companion_stems[i]) == 0) {
            return 1;
        }
    }
    return 0;
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

/* Reads the whole file into the arena; NULL when it cannot be read. */
static char *read_file(r_ctx *c, const char *path)
{
    FILE *fp = fopen(path, "rb");
    long size;
    char *data;

    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0
        || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    data = ek_arena_alloc(c->arena, (size_t)size + 1);
    if (!data) {
        c->oom = 1;
        fclose(fp);
        return NULL;
    }
    /* One byte over the claimed size: a real file comes back whole, a
       directory read errors (EISDIR), and a genuine empty file is
       neither (ferror stays clear at EOF). */
    if (fread(data, 1, (size_t)size + 1, fp) != (size_t)size
        || ferror(fp) != 0) {
        fclose(fp);
        return NULL;
    }
    data[size] = 0;
    fclose(fp);
    return data;
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

/* True for a module registered as a shipped companion: a real,
   compiler-embedded source, spelled with the embedded file name. */
static int shipped_companion(const r_mod *m)
{
    return m && !m->external && strcmp(m->file, EMBEDDED_FILE) == 0;
}

/* Boots the shipped companion of `stem`: parses the embedded source
   and registers the module like an imported file, so its surface and
   bodies follow the ordinary machinery. Called from the import walk
   and from the capability module's boot; idempotence is the caller's
   stem check. */
static void load_companion(r_ctx *c, const char *stem)
{
    char *source;
    ek_node *ast;
    r_mod *m;

    if (strcmp(stem, "time") != 0) {
        return; /* unreachable: companion_stems gates the call */
    }
    source = ek_time_source_join(c->arena);
    if (!source) {
        c->oom = 1;
        return;
    }
    ast = r_parse_source(c, source, ek_time_source_len, EMBEDDED_FILE);
    if (!ast) {
        return;
    }
    m = r_module_add(c, stem, EMBEDDED_FILE, ast);
    if (!m || !m->surf) {
        return;
    }
    r_closure_walk(c, m);
}

/* Two imported modules with the same stem collide over the closure.
   file may be NULL when the collision surfaces before the second file
   resolves; the diagnostic still carries the registered module. */
static void stem_claimed(r_ctx *c, const char *from, const ek_node *imp,
                         const char *stem, const char *file)
{
    r_mod *other = r_module_find(c, stem);

    if (other && (!file || strcmp(other->file, file) != 0)) {
        r_error(c, from, imp ? imp->line : 0, imp ? imp->col : 0,
                "E-name-conflict",
                "module stem `%s` is already imported from `%s` "
                "(Clauses 7.2.1, 7.4.6)", stem, other->file);
    }
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

/* Lex, layout, and parse one module file. */
static ek_node *parse_file(r_ctx *c, const char *path)
{
    char *src = read_file(c, path);

    if (!src) {
        return NULL;
    }
    return r_parse_source(c, src, strlen(src), path);
}

/* Resolves an import path to a file on the module path. */
static char *find_file(r_ctx *c, const ek_node *imp)
{
    ek_buf b;
    size_t d, i;

    for (d = 0; d < c->path_count; d++) {
        char *path;
        int ok;

        ek_buf_init(&b);
        ok = ek_buf_append_str(&b, c->path[d]);
        for (i = 0; ok && i < imp->u.imports.path.count; i++) {
            ok = ek_buf_append_char(&b, '/')
              && ek_buf_append_str(&b, imp->u.imports.path.data[i]);
        }
        ok = ok && ek_buf_append_str(&b, ".eko");
        if (!ok) {
            /* Without the full path the import cannot be resolved or
               named; the walk recovers as on arena exhaustion. */
            ek_buf_destroy(&b);
            c->oom = 1;
            return NULL;
        }
        path = ek_arena_alloc(c->arena, b.len + 1);
        if (!path) {
            c->oom = 1;
            ek_buf_destroy(&b);
            return NULL;
        }
        memcpy(path, b.data, b.len + 1);
        ek_buf_destroy(&b);
        if (read_file(c, path)) {
            return path;
        }
    }
    return NULL;
}

/* A stem is snake_case, never begins with `_`, and is never `std`; the
   reserved std/ paths return before this call. */
static int stem_legal(r_ctx *c, const char *file, const ek_node *imp,
                      const char *stem)
{
    if (!r_is_snake(stem)) {
        r_error(c, file, imp->line, imp->col, "E-name-case",
                "module name `%s` is not snake_case (Clauses 5.5.2, "
                "7.2.1)", stem);
        return 0;
    }
    if (stem[0] == '_') {
        r_error(c, file, imp->line, imp->col, "E-module-name",
                "module file `%s` begins with `_` (Clause 7.2.1)", stem);
        return 0;
    }
    if (strcmp(stem, "std") == 0) {
        r_error(c, file, imp->line, imp->col, "E-module-name",
                "a user module MUST NOT be named `std` (Clauses 7.2.2, "
                "9.1.2)");
        return 0;
    }
    return 1;
}

/* A stem already registered: a diamond share, a back edge of the walk,
   or a collision of two distinct modules with one stem. Two non-root
   files share a stem legally only when find_file resolves both to one
   file; the root's file is spelled by the host, so its back edge is
   judged by stem alone. */
static void already_loaded(r_ctx *c, r_mod *m, const char *from,
                           const ek_node *imp, const char *stem)
{
    char *path;

    if (m->external) {
        /* The stem belongs to a std companion; a user module with the
           same stem collides. */
        stem_claimed(c, from, imp, stem, "(user)");
        return;
    }
    if (m == c->mods) {
        r_error(c, from, imp->line, imp->col, "E-cycle-import",
                "the import of `%s` closes a dependency cycle "
                "(Clause 7.2.4)", stem);
        return;
    }
    path = find_file(c, imp);
    if (!path) {
        return; /* the file no longer resolves; the walk recovers */
    }
    if (strcmp(path, m->file) == 0) {
        if (m->state == 1) {
            r_error(c, from, imp->line, imp->col, "E-cycle-import",
                    "the import of `%s` closes a dependency cycle "
                    "(Clause 7.2.4)", stem);
        }
        return;
    }
    stem_claimed(c, from, imp, stem, path);
}

/* The injected list opens exactly these; a list confined to them asks
   for nothing new. */
static int std_list_opened(const ek_node *imp)
{
    size_t i;

    for (i = 0; i < imp->u.imports.names.count; i++) {
        if (!r_std_opened_name(imp->u.imports.names.data[i])) {
            return 0;
        }
    }
    return 1;
}

/* An explicit std import is redundant when it asks for nothing the
   injection does not already open: a bare line, or a list confined to
   the injected names. */
static void std_redundant(r_ctx *c, const char *file, const ek_node *imp)
{
    if (imp->u.imports.names.count == 0) {
        ek_diag_emit(c->diag, EK_SEV_WARNING, "W-redundant-import",
                     file, imp->line, imp->col,
                     "explicit `import std`; every file already "
                     "imports it (Clause 7.2.5)");
        return;
    }
    if (std_list_opened(imp)) {
        ek_diag_emit(c->diag, EK_SEV_WARNING, "W-redundant-import",
                     file, imp->line, imp->col,
                     "explicit `import std` names only free names the "
                     "implicit import opens (Clauses 7.2.5, 9.1.2)");
    }
}

static void load_import(r_ctx *c, r_mod *from, const ek_node *imp)
{
    const char *file = from->file;
    const char *stem;
    r_mod *m;
    char *path;

    if (c->oom || imp->u.imports.path.count == 0) {
        return;
    }
    stem = imp->u.imports.path.data[imp->u.imports.path.count - 1];
    if (strcmp(imp->u.imports.path.data[0], "std") == 0) {
        r_mod *held;

        if (imp->u.imports.path.count == 1) {
            /* The implicit import already put the std module in the
               closure; an explicit line only repeats it. */
            std_redundant(c, file, imp);
            return;
        }
        held = r_module_find(c, stem);
        if (shipped_companion(held)) {
            /* The shipped companion is already in the closure — the
               capability module imports it at boot — so this line is a
               legal diamond share, and the stem opens in the importing
               file's own scope pass. */
            return;
        }
        if (held == c->mods && r_companion_stem(stem)) {
            /* A root illegally named as a shipped companion claims the
               stem; the root is already rejected (E-module-name), and
               the companion stays out of the closure rather than
               compounding the rejection with a conflict at cap.eko
               and unbound names in the capability source. */
            return;
        }
        if (held && !held->external) {
            /* The std/ path is reserved; a user module already holding
               the companion's stem collides over the closure, whichever
               import came first. */
            r_error(c, file, imp->line, imp->col, "E-name-conflict",
                    "module stem `%s` is already imported from `%s` "
                    "(Clauses 7.2.2, 7.4.6)", stem, held->file);
            return;
        }
        if (held) {
            /* An unknown companion's stub holds the stem. */
            return;
        }
        /* A companion stem obeys the stem rules like any module
           name. */
        if (!stem_legal(c, file, imp, stem)) {
            return;
        }
        if (r_companion_stem(stem)) {
            load_companion(c, stem);
        } else {
            r_module_companion(c, stem);
        }
        return;
    }
    if (strcmp(imp->u.imports.path.data[0], "cap") == 0) {
        /* The capability module is compiler-embedded and has no import
           path; its names resolve without one (Clauses 9.1.1, 9.1.2). */
        r_error(c, file, imp->line, imp->col, "E-module-name",
                "no import path names the capability module (Clauses "
                "9.1.1, 9.1.2)");
        return;
    }
    if (!stem_legal(c, file, imp, stem)) {
        return;
    }
    m = r_module_find(c, stem);
    if (m) {
        already_loaded(c, m, file, imp, stem);
        return;
    }
    path = find_file(c, imp);
    if (!path) {
        r_error(c, file, imp->line, imp->col, "E-import-missing",
                "import `%s` resolves to no file on the module path "
                "(Clause 7.2.2)", stem);
        return;
    }
    m = register_mod(c, stem, path, parse_file(c, path), 0);
    r_closure_walk(c, m);
}

/* Depth-first walk over the imports of one module. */
void r_closure_walk(r_ctx *c, r_mod *m)
{
    const ek_node *mod;
    size_t i, idx;

    if (!m || c->oom || m->state != 0) {
        return;
    }
    /* The walk holds the module by index: a nested import grows the
       closure table, and a held pointer would dangle across the
       realloc. */
    idx = (size_t)(m - c->mods);
    m->state = 1;
    mod = m->ast;
    if (!mod || m->external) {
        c->mods[idx].state = 2;
        return;
    }
    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *imp = mod->u.kids.data[i];

        if (imp->kind != EK_N_IMPORT || imp->u.imports.path.count == 0) {
            continue;
        }
        load_import(c, &c->mods[idx], imp);
    }
    c->mods[idx].state = 2;
}

void r_closure(r_ctx *c, r_mod *root)
{
    r_closure_walk(c, root);
}
