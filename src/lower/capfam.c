#include <string.h>

#include "lower/internal.h"

/* The generated families the runtime builds capability values over.
   The bundle closures construct std and capability values -- the
   Result of a read, the Option of a lookup, the IoError reasons, the
   OpenMode variants a path arrives in, the FileInfo of a stat, the
   time companion's readings -- and every such value carries the
   emitted unit's own typeinfo identity, so generated pattern matches
   answer them. Registration stamps each constructor's identity into
   the family registry without the construction helper (construct zero
   through lower_ctor_of_symbol): the runtime builds the values itself,
   the unit only defines the identity. A family a call site also
   constructs keeps its helper, and one registration per capability
   keeps the set idempotent. */

/* The constructor symbol of one module's namespace, or NULL. */
static const ek_symbol *ctor_sym(lower_ctx *c, const char *module,
                                 const char *owner, const char *name)
{
    size_t k, i;

    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_CONSTRUCTOR && m->owner
                && strcmp(m->owner, owner) == 0
                && strcmp(m->name, name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* Stamps one constructor family as runtime-shared. */
static void register_ctor(lower_ctx *c, const ek_node *at,
                          const char *module, const char *owner,
                          const char *name)
{
    const ek_symbol *cs = ctor_sym(c, module, owner, name);
    lower_gtype *g;

    if (cs == NULL) {
        lower_error(c, at, "the constructor `%s.%s` is not supported by "
                          "this lowerer", owner, name);
        return;
    }
    g = lower_ctor_of_symbol(c, at, cs, 0);
    if (g) {
        g->shared = 1;
    }
}

/* Stamps one record representation as runtime-shared: the runtime
   builds the records over the layout contract. */
static void register_repr(lower_ctx *c, const char *module,
                          const char *name)
{
    size_t k, i;

    for (k = 0; k < c->resolved.module_count; k++) {
        const ek_surface *s = &c->resolved.modules[k];

        if (strcmp(s->module, module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_TYPE && strcmp(m->name, name) == 0) {
                lower_gtype *g = lower_decl_repr(c, m);

                if (g) {
                    g->shared = 1;
                }
                return;
            }
        }
    }
}

void lower_cap_families(lower_ctx *c, const ek_node *at)
{
    static const char *io[] = {"NotFound",    "Permission", "Exists",
                               "NotFile",     "Closed",     "InvalidData",
                               "Other"};
    static const char *modes[] = {"Read", "Write", "Append", "New",
                                  "Update"};
    size_t i;

    register_ctor(c, at, "std", "Result", "Ok");
    register_ctor(c, at, "std", "Result", "Err");
    register_ctor(c, at, "std", "Option", "Some");
    register_ctor(c, at, "std", "Option", "None");
    for (i = 0; i < sizeof io / sizeof *io; i++) {
        register_ctor(c, at, "cap", "IoError", io[i]);
    }
    for (i = 0; i < sizeof modes / sizeof *modes; i++) {
        register_ctor(c, at, "cap", "OpenMode", modes[i]);
    }
    register_repr(c, "cap", "FileInfo");
    register_ctor(c, at, "time", "Time", "Time");
    register_ctor(c, at, "time", "Mono", "Mono");
}

/* The seven Json and three ParseError constructor families the
   runtime's document reader builds values over. A program that never
   routes the parse row references no identity of theirs, so the
   stamping fires exactly at the routing. */
void lower_json_families(lower_ctx *c, const ek_node *at)
{
    static const char *json[] = {"Null", "Bool", "Int",  "Float",
                                 "Str",  "Arr",  "Obj"};
    static const char *errs[] = {"Empty", "Invalid", "OutOfRange"};
    size_t i;

    for (i = 0; i < sizeof json / sizeof *json; i++) {
        register_ctor(c, at, "std", "Json", json[i]);
    }
    for (i = 0; i < sizeof errs / sizeof *errs; i++) {
        register_ctor(c, at, "std", "ParseError", errs[i]);
    }
}
