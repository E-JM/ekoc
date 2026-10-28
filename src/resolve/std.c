#include <string.h>

#include "resolve/internal.h"
#include "resolve/std_source.h"

/* The std module, parsed from the embedded lib/std.eko bytes. Its
   surface holds the file's own declarations first (the types with
   their constructors, and the opened assert family), then the
   placeholder rows for the names the compiler still supplies: the
   scalar, container, capability, and time type names. A real-file row
   wins over a placeholder row of the same kind and name; a name no row
   declares stays the external bridge (resolve.h). */

#define STD_FILE "std.eko"

/* The free names the injected selective import opens. The list is a
   constant of the language definition. */
static const char *opened_names[] = {
    "assert", "assert_eq", "assert_ne", NULL
};

static const char *type_names[] = {
    "Char", "String", "Bytes", "Int", "UInt", "Float",
    "Set", "List", "Dict", "Path",
    "Out", "In", "Fs", "Env", "Args", "Clock", "Rand", "Proc", "Conc",
    NULL
};

#define TYPE_ROWS (sizeof type_names / sizeof *type_names - 1)

/* The nine capability type names: std carries no capability content,
   so the names open unqualified and have no std-qualified spelling. */
int r_std_cap_name(const char *name)
{
    static const char caps[] = "OutInFsEnvArgsClockRandProcConc";
    static const unsigned char off[] = {0, 3, 5, 7, 10, 14, 19, 23, 27};
    size_t i;

    for (i = 0; i < sizeof off; i++) {
        const char *cand = caps + off[i];
        size_t len = (i + 1 < sizeof off ? off[i + 1] : 31) - off[i];

        if (strlen(name) == len && strncmp(name, cand, len) == 0) {
            return 1;
        }
    }
    return 0;
}

/* True for a type name the std surface supplies as a placeholder row
   (the scalar, container, and time vocabulary), except the capability
   names: their namespaces belong to the capability module. The std
   source may declare members of these namespaces, so the ownership
   check accepts the module that owns the placeholder row. */
int r_std_type_name(const char *name)
{
    size_t i;

    if (r_std_cap_name(name)) {
        return 0;
    }
    for (i = 0; i < TYPE_ROWS; i++) {
        if (strcmp(type_names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

int r_std_opened_name(const char *name)
{
    /* The sentinel NULL is not an entry. */
    size_t n = sizeof opened_names / sizeof *opened_names - 1;
    size_t i;

    for (i = 0; i < n; i++) {
        if (strcmp(opened_names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* A placeholder std row: the module is std and no declaration stands
   behind it. Its reading stays bridged. */
int r_is_placeholder(const ek_symbol *sym)
{
    return sym && sym->module && strcmp(sym->module, "std") == 0
        && sym->decl == NULL;
}

/* The std surface symbol of one kind and name, or NULL. */
const ek_symbol *r_std_find(const ek_surface *s, ek_sym_kind kind,
                            const char *name)
{
    size_t i;

    if (!s) {
        return NULL;
    }
    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == kind && strcmp(s->syms[i].name, name) == 0) {
            return &s->syms[i];
        }
    }
    return NULL;
}

/* Appends one placeholder row unless the surface already declares the
   name in that kind. */
static void add_row(r_ctx *c, ek_surface *s, const char *name,
                    ek_sym_kind kind, const char *owner)
{
    ek_symbol *sym;

    if (r_std_find(s, kind, name)) {
        return;
    }
    sym = r_sym(c, name, kind, "std", 0, 0);
    if (!sym) {
        return;
    }
    if (owner) {
        sym->owner = owner;
    }
    s->syms[s->count++] = *sym;
}

/* The placeholder rows beneath the file surface: the type names the
   compiler still supplies. */
static void extend(r_ctx *c, ek_surface *s)
{
    ek_symbol *grown;
    size_t i, cap = s->count + TYPE_ROWS;

    grown = ek_arena_alloc(c->arena, cap * sizeof *grown);
    if (!grown) {
        c->oom = 1;
        return;
    }
    memcpy(grown, s->syms, s->count * sizeof *grown);
    s->syms = grown;
    for (i = 0; type_names[i]; i++) {
        add_row(c, s, type_names[i], EK_SYM_TYPE, NULL);
    }
}

/* The injected list is a constant of the language definition, so every
   name it carries is a free function of the parsed file. A missing one
   is a defect of the compiler's own surface, never a bridge. */
static void require_opened(r_ctx *c)
{
    size_t i;

    for (i = 0; opened_names[i]; i++) {
        const ek_symbol *sym = r_std_find(c->std, EK_SYM_FUNC,
                                          opened_names[i]);

        if (!sym || !sym->decl) {
            r_error(c, STD_FILE, 1, 1, "E-std-surface",
                    "the embedded std source declares no free function "
                    "`%s`, so the implicit import cannot open it",
                    opened_names[i]);
        }
    }
}

/* Parses the embedded std source into a module of the closure. */
void r_std_boot(r_ctx *c)
{
    r_mod *m;
    ek_node *ast;

    if (c->oom || c->std) {
        return;
    }
    if (r_module_find(c, "std")) {
        /* Only a root illegally named `std` claims the stem first; it is
           already rejected, and the closure keeps one module per stem. */
        return;
    }
    char *source = ek_std_source_join(c->arena);

    if (!source) {
        c->oom = 1;
        return;
    }
    ast = r_parse_source(c, source, ek_std_source_len, STD_FILE);
    if (!ast) {
        return;
    }
    m = r_module_add(c, "std", STD_FILE, ast);
    if (!m || !m->surf) {
        return;
    }
    extend(c, m->surf);
    if (c->oom) {
        return;
    }
    m->state = 2; /* parsed, and it imports nothing */
    c->std = m->surf;
    require_opened(c);
}

/* The std module declares the names its own injection would open; a
   module cannot import itself, and its own binding already carries the
   name. */
static int own_binding(const r_file *f, const ek_symbol *sym)
{
    size_t i;

    for (i = 0; i < f->ntops; i++) {
        if (f->tops[i].sym == sym) {
            return 1;
        }
    }
    return 0;
}

/* Binds the implicit import into a file: the shared surface stays the
   bottom layer for types and constructors, and the three injected free
   names enter the file's opened table. */
void r_std_open(r_ctx *c, r_file *f)
{
    size_t i;

    if (!c->std) {
        return;
    }
    f->std = c->std;
    for (i = 0; opened_names[i]; i++) {
        const ek_symbol *sym = r_std_find(c->std, EK_SYM_FUNC,
                                          opened_names[i]);

        if (sym && !own_binding(f, sym)) {
            r_open_name(c, f, opened_names[i], sym, "std", NULL);
        }
    }
}
