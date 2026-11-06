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
   keeps the set idempotent.

   Registration follows the manifest: the shim builds one bundle per
   capability parameter, and that call pulls the runtime's own object
   files in through archive resolution, so the families the pulled
   members name are the capability's to define. The Out and Proc
   bundles stay runtime-typed and name no generated identity; each
   other capability's row is the set of identities its own runtime
   members name, and the Fs bundle's mode dispatch names the OpenMode
   identities besides. The table lists one row per manifest capability,
   so a future interface mechanism extends it in place. */

/* One family registration step: a constructor family, or a record
   representation when repr is set. */
typedef struct {
    const char *module;
    const char *owner;
    const char *name;
    int repr;
} fam_step;

/* One group of steps, and one capability's row over its own group
   list: the groups register in order, each family once. */
typedef struct {
    const fam_step *steps;
    size_t nsteps;
} fam_group;

typedef struct {
    const char *cap;
    const fam_group *const *groups;
    size_t ngroups;
} cap_row;

static const fam_step steps_status[] = {
    {"std", "Result", "Ok", 0},
    {"std", "Result", "Err", 0},
    {"std", "Option", "Some", 0},
    {"std", "Option", "None", 0},
};

static const fam_step steps_ioerr[] = {
    {"sys", "IoError", "NotFound", 0},
    {"sys", "IoError", "Permission", 0},
    {"sys", "IoError", "Exists", 0},
    {"sys", "IoError", "NotFile", 0},
    {"sys", "IoError", "Closed", 0},
    {"sys", "IoError", "InvalidData", 0},
    {"sys", "IoError", "Other", 0},
};

static const fam_step steps_finfo[] = {
    {"sys", NULL, "FileInfo", 1},
};

static const fam_step steps_time[] = {
    {"time", "Time", "Time", 0},
    {"time", "Mono", "Mono", 0},
    {"time", "Duration", "Duration", 0},
};

static const fam_step steps_span[] = {
    {"time", "Time", "Time", 0},
    {"time", "Duration", "Duration", 0},
};

static const fam_step steps_parse[] = {
    {"std", "ParseError", "Empty", 0},
    {"std", "ParseError", "Invalid", 0},
    {"std", "ParseError", "OutOfRange", 0},
};

static const fam_step steps_modes[] = {
    {"sys", "OpenMode", "Read", 0},
    {"sys", "OpenMode", "Write", 0},
    {"sys", "OpenMode", "Append", 0},
    {"sys", "OpenMode", "New", 0},
    {"sys", "OpenMode", "Update", 0},
};

/* The family groups. Each capability's row below takes exactly the
   groups its own runtime members name. */
static const fam_group group_status = {
    steps_status, sizeof steps_status / sizeof *steps_status
};
static const fam_group group_ioerr = {
    steps_ioerr, sizeof steps_ioerr / sizeof *steps_ioerr
};
static const fam_group group_finfo = {
    steps_finfo, sizeof steps_finfo / sizeof *steps_finfo
};
static const fam_group group_time = {
    steps_time, sizeof steps_time / sizeof *steps_time
};
static const fam_group group_span = {
    steps_span, sizeof steps_span / sizeof *steps_span
};
static const fam_group group_parse = {
    steps_parse, sizeof steps_parse / sizeof *steps_parse
};
static const fam_group group_modes = {
    steps_modes, sizeof steps_modes / sizeof *steps_modes
};

/* Each capability's list is what its own runtime members name, so a
   reordering of the groups cannot silently attach or detach a
   registration. The reader's Result and Option come from the In
   closures, the IoError from their failures; Env reads an Option;
   Clock reads the time pair; Args, Rand, Out, and Proc name no
   generated identity at all. */
static const fam_group *const groups_in[] = {
    &group_status, &group_ioerr,
};

static const fam_group *const groups_env[] = {
    &group_status,
};

static const fam_group *const groups_clock[] = {
    &group_time,
};

/* The Fs row: the status and reason set, the stat and time identities
   its directory walks build, and the OpenMode identities the mode
   dispatch names. */
static const fam_group *const groups_fs[] = {
    &group_status, &group_ioerr, &group_finfo, &group_time, &group_modes,
};

static const cap_row cap_table[] = {
    {"Out",   NULL, 0},
    {"Conc",  groups_env, sizeof groups_env / sizeof *groups_env},
    {"In",    groups_in, sizeof groups_in / sizeof *groups_in},
    {"Fs",    groups_fs, sizeof groups_fs / sizeof *groups_fs},
    {"Env",   groups_env, sizeof groups_env / sizeof *groups_env},
    {"Args",  NULL, 0},
    {"Clock", groups_clock, sizeof groups_clock / sizeof *groups_clock},
    {"Rand",  NULL, 0},
    {"Proc",  NULL, 0},
};

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

/* Registers one step: the constructor family, or the record
   representation. */
static void register_step(lower_ctx *c, const ek_node *at,
                          const fam_step *s)
{
    if (s->repr) {
        register_repr(c, s->module, s->name);
    } else {
        register_ctor(c, at, s->module, s->owner, s->name);
    }
}

static void register_groups(lower_ctx *c, const ek_node *at,
                            const fam_group *const *groups, size_t ngroups)
{
    size_t g, i;

    for (g = 0; g < ngroups; g++) {
        for (i = 0; i < groups[g]->nsteps; i++) {
            register_step(c, at, &groups[g]->steps[i]);
        }
    }
}

/* The families of one capability of the manifest: the set the
   capability's bundle pull names. A capability outside the table has
   no bundle behind it and is a diagnostic. */
void lower_cap_families_of(lower_ctx *c, const ek_node *at, const char *cap)
{
    size_t i;

    for (i = 0; i < sizeof cap_table / sizeof *cap_table; i++) {
        if (strcmp(cap_table[i].cap, cap) == 0) {
            register_groups(c, at, cap_table[i].groups,
                            cap_table[i].ngroups);
            return;
        }
    }
    lower_error(c, at, "the capability `%s` is not supported by this "
                       "lowerer", cap);
}

/* The full set, every capability row at once: for the roots whose
   family needs no manifest bounds -- the test binaries accept the
   size, and their injected constructors are harder to bound. */
void lower_cap_families(lower_ctx *c, const ek_node *at)
{
    size_t i;

    for (i = 0; i < sizeof cap_table / sizeof *cap_table; i++) {
        register_groups(c, at, cap_table[i].groups, cap_table[i].ngroups);
    }
}

/* The seven Json variant shapes the decode side reads: the typedefs
   and typeinfo identities join the unit as one set, without the
   construction helpers the decode never calls. */
void lower_json_shapes(lower_ctx *c, const ek_node *at)
{
    static const fam_step steps_json[] = {
        {"std", "Json", "Null", 0},
        {"std", "Json", "Bool", 0},
        {"std", "Json", "Int", 0},
        {"std", "Json", "Float", 0},
        {"std", "Json", "Str", 0},
        {"std", "Json", "Arr", 0},
        {"std", "Json", "Obj", 0},
    };
    static const fam_group group_json = {
        steps_json, sizeof steps_json / sizeof *steps_json
    };
    static const fam_group *const groups[] = {&group_json};

    register_groups(c, at, groups, sizeof groups / sizeof *groups);
}

/* The seven Json and three ParseError constructor families the
   runtime's document reader builds values over. The routing pulls the
   reader's own object file and, with it, the status builders its
   Result constructors stand on, so the stamping covers both sets and
   fires exactly at the routing. */
void lower_json_families(lower_ctx *c, const ek_node *at)
{
    static const fam_group *const groups_build[] = {&group_status};
    static const fam_group *const groups_reader[] = {&group_parse};

    register_groups(c, at, groups_build,
                    sizeof groups_build / sizeof *groups_build);
    lower_json_shapes(c, at);
    register_groups(c, at, groups_reader,
                    sizeof groups_reader / sizeof *groups_reader);
}

/* The time companion's parse identities: the reading and span
   constructors the runtime's parsers build values over, with the
   status and parse-error sets their Results stand on. The parsers
   share the document reader's error builders, so pulling them pulls
   the reader's object file, whose Json identities come with it. */
void lower_time_families(lower_ctx *c, const ek_node *at)
{
    static const fam_group *const groups_span[] = {&group_span};

    register_groups(c, at, groups_span,
                    sizeof groups_span / sizeof *groups_span);
    lower_json_families(c, at);
}
