#include <stdio.h>
#include <string.h>

#include "resolve/internal.h"
#include "support/buf.h"

/* The import closure: one module's import lines, the files and shipped
   companions they load, and the depth-first walk over the closure.
   Paths resolve against the module path directories in order; the
   dependency graph must be acyclic. */

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

/* A stem the std/ line names that the closure already holds: the
   shipped companion shares as a diamond, a root claiming a reserved
   stem is already rejected, a user module holding it collides, and an
   unknown companion's stub keeps it. Returns 1 when the line is
   handled. */
static int std_held(r_ctx *c, const char *file, const ek_node *imp,
                    const char *stem, const r_mod *held)
{
    if (r_shipped_companion(held)) {
        /* The shipped companion is already in the closure — the
           capability module imports it at boot — so this line is a
           legal diamond share, and the stem opens in the importing
           file's own scope pass. */
        return 1;
    }
    if (held == c->mods && r_companion_stem(stem)) {
        /* A root illegally named as a shipped companion claims the
           stem; the root is already rejected (E-module-name), and
           the companion stays out of the closure rather than
           compounding the rejection with a conflict at cap.eko
           and unbound names in the capability source. */
        return 1;
    }
    if (held && !held->external) {
        /* The std/ path is reserved; a user module already holding
           the companion's stem collides over the closure, whichever
           import came first. */
        r_error(c, file, imp->line, imp->col, "E-name-conflict",
                "module stem `%s` is already imported from `%s` "
                "(Clauses 7.2.2, 7.4.6)", stem, held->file);
        return 1;
    }
    return held != NULL;
}

/* The std/ path of an import line: a bare line is redundant, a stem
   the closure holds is judged, and a new companion is booted or
   reserved. */
static void load_std(r_ctx *c, const char *file, const ek_node *imp,
                     const char *stem)
{
    if (imp->u.imports.path.count == 1) {
        /* The implicit import already put the std module in the
           closure; an explicit line only repeats it. */
        std_redundant(c, file, imp);
        return;
    }
    if (std_held(c, file, imp, stem, r_module_find(c, stem))) {
        return;
    }
    /* A companion stem obeys the stem rules like any module name. */
    if (!r_stem_legal(c, file, imp->line, imp->col, stem)) {
        return;
    }
    if (r_companion_stem(stem)) {
        r_load_companion(c, stem);
    } else {
        r_module_companion(c, stem);
    }
}

/* The embedded modules are compiler-embedded and have no import path;
   their names resolve without one (Clauses 9.1.1, 9.1.2). */
static void embedded_rejected(r_ctx *c, const char *file,
                              const ek_node *imp)
{
    r_error(c, file, imp->line, imp->col, "E-module-name",
            "no import path names an embedded module (Clauses "
            "9.1.1, 9.1.2)");
}

/* A user module: the stem rules, the closure guards, then the file the
   module path resolves the line to. */
static void load_user(r_ctx *c, const char *file, const ek_node *imp,
                      const char *stem)
{
    r_mod *m;
    char *path;

    if (!r_stem_legal(c, file, imp->line, imp->col, stem)) {
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
    m = r_module_add(c, stem, path, parse_file(c, path));
    r_closure_walk(c, m);
}

/* One import line of one module: the std/ path, an embedded module's
   name, or a user module found on the module path. */
static void load_import(r_ctx *c, r_mod *from, const ek_node *imp)
{
    const char *file = from->file;
    const char *stem;

    if (c->oom || imp->u.imports.path.count == 0) {
        return;
    }
    stem = imp->u.imports.path.data[imp->u.imports.path.count - 1];
    if (strcmp(imp->u.imports.path.data[0], "std") == 0) {
        load_std(c, file, imp, stem);
        return;
    }
    if (strcmp(imp->u.imports.path.data[0], "cap") == 0
        || strcmp(imp->u.imports.path.data[0], "sys") == 0) {
        embedded_rejected(c, file, imp);
        return;
    }
    load_user(c, file, imp, stem);
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
