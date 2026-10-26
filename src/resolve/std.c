#include <string.h>

#include "resolve/internal.h"

/* PLACEHOLDER STD SURFACE, standing in until the real interface is
   derived from lib/std.eko. It holds exactly the names Clause 7.2.5
   opens: the free names, the opened types, the capability module names
   of Clause 9.1.2, and the zero-payload constructors of Bool, Option,
   Result, Time, and Duration. Everything else a file uses from the
   implicit import resolves to EK_SYM_EXTERNAL without a diagnostic
   (the bridge ruling in resolve.h). */

static const char *free_names[] = {
    "show", "inspect", "panic", "assert", "assert_eq", "assert_ne", NULL
};

static const char *type_names[] = {
    "Bool", "Char", "String", "Bytes", "Int", "UInt", "Float",
    "Option", "Result", "Range", "Never", "Json", "Time", "Duration",
    "Out", "In", "Fs", "Env", "Args", "Clock", "Rand", "Proc", "Conc",
    "File", "OpenMode", "FileInfo", "IoError", "Scope", "Task", NULL
};

static const char *ctor_names[] = {
    "True", "False", "Some", "None", "Ok", "Err", "Time", "Duration",
    NULL
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
    n = fill(c, s, n, ctor_names, EK_SYM_CONSTRUCTOR);
    if (c->oom) {
        c->std = NULL;
        return;
    }
    s->count = n;
    c->std = s;
}

/* Opens the implicit import into a file's layers: the free names share
   the top-level layer, the types the identity layer, the constructors
   the constructor space (Clause 7.2.5). */
void r_std_open(r_ctx *c, r_file *f)
{
    ek_surface *s;
    size_t i;

    r_std_init(c);
    s = c->std;
    if (!s) {
        return;
    }
    for (i = 0; i < s->count; i++) {
        const ek_symbol *sym = &s->syms[i];
        r_var *v;

        if (sym->kind == EK_SYM_FUNC) {
            if (f->ntops == f->ctops) {
                f->tops = r_grow(c, f->tops, f->ntops, &f->ctops,
                                 sizeof *f->tops);
                if (c->oom) {
                    return;
                }
            }
            v = &f->tops[f->ntops++];
        } else if (sym->kind == EK_SYM_TYPE) {
            if (f->ntypes == f->ctypes) {
                f->types = r_grow(c, f->types, f->ntypes, &f->ctypes,
                                  sizeof *f->types);
                if (c->oom) {
                    return;
                }
            }
            v = &f->types[f->ntypes++];
        } else {
            if (f->nctors == f->cctors) {
                f->ctors = r_grow(c, f->ctors, f->nctors, &f->cctors,
                                  sizeof *f->ctors);
                if (c->oom) {
                    return;
                }
            }
            v = &f->ctors[f->nctors++];
        }
        v->name = sym->name;
        v->sym = sym;
    }
}

const ek_symbol *r_std_type(r_ctx *c, const char *name)
{
    ek_surface *s;
    size_t i;

    r_std_init(c);
    s = c->std;
    if (!s) {
        return NULL;
    }
    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_TYPE
            && strcmp(s->syms[i].name, name) == 0) {
            return &s->syms[i];
        }
    }
    return NULL;
}

const ek_symbol *r_std_ctor(r_ctx *c, const char *name)
{
    ek_surface *s;
    size_t i;

    r_std_init(c);
    s = c->std;
    if (!s) {
        return NULL;
    }
    for (i = 0; i < s->count; i++) {
        if (s->syms[i].kind == EK_SYM_CONSTRUCTOR
            && strcmp(s->syms[i].name, name) == 0) {
            return &s->syms[i];
        }
    }
    return NULL;
}
