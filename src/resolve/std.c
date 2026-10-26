#include <string.h>

#include "resolve/internal.h"

/* PLACEHOLDER STD SURFACE, standing in until the real interface is
   derived from lib/std.eko. It holds exactly the names the implicit
   import opens: the free names, the opened types, the capability module
   names, and the zero-payload constructors of Bool, Option, Result,
   Time, and Duration. Everything else a file uses from the implicit
   import resolves to EK_SYM_EXTERNAL without a diagnostic (the bridge
   described in resolve.h). */

static const char *free_names[] = {
    "show", "inspect", "panic", "assert", "assert_eq", "assert_ne", NULL
};

/* The free names the injected selective import opens. The list is a
   constant of the language definition. */
static const char *opened_names[] = {
    "assert", "assert_eq", "assert_ne", NULL
};

static const char *type_names[] = {
    "Bool", "Char", "String", "Bytes", "Int", "UInt", "Float",
    "Option", "Result", "Range", "Never", "Json", "Time", "Duration",
    "Out", "In", "Fs", "Env", "Args", "Clock", "Rand", "Proc", "Conc",
    "File", "OpenMode", "FileInfo", "IoError", "Scope", "Task", NULL
};

/* The placeholder constructors and the opened type that owns each:
   the owner spelling is the anchor a reference is matched against. */
static const struct {
    const char *name;
    const char *owner;
} ctor_names[] = {
    {"True", "Bool"}, {"False", "Bool"},
    {"Some", "Option"}, {"None", "Option"},
    {"Ok", "Result"}, {"Err", "Result"},
    {"Time", "Time"}, {"Duration", "Duration"},
    {NULL, NULL}
};

#define R_STD_COUNT (sizeof free_names / sizeof *free_names \
    + sizeof type_names / sizeof *type_names \
    + sizeof ctor_names / sizeof *ctor_names)

int r_std_free_name(const char *name)
{
    /* The sentinel NULL is not an entry. */
    size_t n = sizeof free_names / sizeof *free_names - 1;
    size_t i;

    for (i = 0; i < n; i++) {
        if (strcmp(free_names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* One placeholder row per name; stops silently on arena failure. */
static size_t fill(r_ctx *c, ek_surface *s, size_t n, const char **names,
                   ek_sym_kind kind)
{
    size_t i;

    for (i = 0; names[i]; i++) {
        ek_symbol *sym = r_sym(c, names[i], kind, "std", 0, 0);

        if (!sym) {
            return n;
        }
        s->syms[n++] = *sym;
    }
    return n;
}

/* A constructor row carries the opened type that owns it. */
static size_t fill_ctors(r_ctx *c, ek_surface *s, size_t n)
{
    size_t i;

    for (i = 0; ctor_names[i].name; i++) {
        ek_symbol *sym = r_sym(c, ctor_names[i].name, EK_SYM_CONSTRUCTOR,
                               "std", 0, 0);

        if (!sym) {
            return n;
        }
        sym->owner = ctor_names[i].owner;
        s->syms[n++] = *sym;
    }
    return n;
}

void r_std_init(r_ctx *c)
{
    ek_surface *s;
    size_t n;

    if (c->std || c->oom) {
        return;
    }
    s = ek_arena_alloc(c->arena, sizeof *s);
    if (!s) {
        c->oom = 1;
        return;
    }
    s->syms = ek_arena_alloc(c->arena, R_STD_COUNT * sizeof *s->syms);
    if (!s->syms) {
        c->oom = 1;
        return;
    }
    s->module = "std";
    s->file = "(std)";
    n = fill(c, s, 0, free_names, EK_SYM_FUNC);
    n = fill(c, s, n, type_names, EK_SYM_TYPE);
    n = fill_ctors(c, s, n);
    if (c->oom) {
        c->std = NULL;
        return;
    }
    s->count = n;
    c->std = s;
}

/* Binds the implicit import into a file: the shared surface stays the
   bottom layer for types and constructors, and the three injected free
   names enter the file's opened table. */
void r_std_open(r_ctx *c, r_file *f)
{
    size_t i;

    r_std_init(c);
    f->std = c->std;
    for (i = 0; opened_names[i]; i++) {
        const ek_symbol *sym = r_std_find(c->std, EK_SYM_FUNC,
                                          opened_names[i]);

        if (sym) {
            r_open_name(c, f, opened_names[i], sym, "std", NULL);
        }
    }
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

const ek_symbol *r_std_ctor(r_ctx *c, const char *name)
{
    r_std_init(c);
    return r_std_find(c->std, EK_SYM_CONSTRUCTOR, name);
}
