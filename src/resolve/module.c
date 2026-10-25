#include <stdio.h>
#include <string.h>

#include "lex/lex.h"
#include "layout/layout.h"
#include "parse/parse.h"
#include "resolve/internal.h"
#include "support/buf.h"

/* Loading modules and walking the import closure (Clause 7.2). Paths
   resolve against the module path directories in order; the dependency
   graph is walked depth first, first import first, and must be acyclic
   (Clause 7.2.4). */

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
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
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

/* Two imported modules with the same stem collide over the closure
   (Clauses 7.2.1, 7.4.6). */
static void stem_claimed(r_ctx *c, const char *from, const ek_node *imp,
                         const char *stem, const char *file)
{
    r_mod *other = r_module_find(c, stem);

    if (other && strcmp(other->file, file) != 0) {
        r_error(c, from, imp ? imp->line : 0, imp ? imp->col : 0,
                "E-name-conflict",
                "module stem `%s` is already imported from `%s` "
                "(Clauses 7.2.1, 7.4.6)", stem, other->file);
    }
}

/* Lex, layout, and parse one module file (stages before this one). */
static ek_node *parse_file(r_ctx *c, const char *path)
{
    char *src = read_file(c, path);
    ek_lex_result lex;
    ek_layout_result lay;

    if (!src) {
        return NULL;
    }
    lex = ek_lex(src, strlen(src), path, c->arena, c->diag);
    lay = ek_layout(lex.tokens, lex.count, path, c->arena, c->diag);
    return ek_parse(lay.tokens, lay.count, path, c->arena, c->diag).module;
}

/* Resolves an import path to a file on the module path (7.2.2). */
static char *find_file(r_ctx *c, const ek_node *imp)
{
    ek_buf b;
    size_t d, i;

    for (d = 0; d < c->path_count; d++) {
        char *path;

        ek_buf_init(&b);
        ek_buf_append_str(&b, c->path[d]);
        for (i = 0; i < imp->u.path.count; i++) {
            ek_buf_append_char(&b, '/');
            ek_buf_append_str(&b, imp->u.path.data[i]);
        }
        ek_buf_append_str(&b, ".eko");
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

/* A module stem is snake_case and never begins with `_`
   (Clauses 5.5.2, 7.2.1); std paths were handled earlier. */
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
    return 1;
}

/* A stem already registered: a diamond share, or a collision of two
   distinct modules with one stem (Clauses 7.2.1, 7.4.6). */
static void already_loaded(r_ctx *c, r_mod *m, const char *from,
                           const ek_node *imp, const char *stem)
{
    char *path;

    if (m->external) {
        /* The stem belongs to a std companion; a user module with the
           same stem collides (Clauses 7.2.2, 7.4.6). */
        stem_claimed(c, from, imp, stem, "(user)");
        return;
    }
    path = find_file(c, imp);
    if (!path || strcmp(path, m->file) != 0) {
        stem_claimed(c, from, imp, stem, path);
    }
}

static void load_import(r_ctx *c, r_mod *from, const ek_node *imp)
{
    const char *file = from->file;
    const char *stem;
    r_mod *m;
    char *path;

    if (c->oom || imp->u.path.count == 0) {
        return;
    }
    stem = imp->u.path.data[imp->u.path.count - 1];
    if (strcmp(imp->u.path.data[0], "std") == 0) {
        if (imp->u.path.count == 1) {
            ek_diag_emit(c->diag, EK_SEV_WARNING, "W-redundant-import",
                         file, imp->line, imp->col,
                         "explicit `import std`; every file already "
                         "imports it (Clause 7.2.5)");
        }
        if (!r_module_find(c, stem)) {
            register_mod(c, stem, "(std)", NULL, 1);
        }
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
    size_t i;

    if (!m || c->oom || m->state != 0) {
        return;
    }
    m->state = 1;
    if (!m->ast || m->external) {
        m->state = 2;
        return;
    }
    mod = m->ast;
    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *imp = mod->u.kids.data[i];
        const char *stem;
        r_mod *target;

        if (imp->kind != EK_N_IMPORT || imp->u.path.count == 0) {
            continue;
        }
        stem = imp->u.path.data[imp->u.path.count - 1];
        target = r_module_find(c, stem);
        if (target && target->state == 1) {
            r_error(c, m->file, imp->line, imp->col, "E-cycle-import",
                    "the import of `%s` closes a dependency cycle "
                    "(Clause 7.2.4)", stem);
            continue;
        }
        load_import(c, m, imp);
    }
    m->state = 2;
}

void r_closure(r_ctx *c, r_mod *root)
{
    r_closure_walk(c, root);
}
