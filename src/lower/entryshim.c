#include <string.h>

#include "lower/internal.h"

/* The entry shim's bookkeeping: every `main` parameter is a capability
   the shim supplies by position, and each test binding registers for
   the dispatching shim. A parameter outside the table keeps the
   no-shim balance, and a test root registers every capability family
   its derived constructors can build over. */

/* The capability name of one parameter's written type: the final name
   of the type node, parens unwrapped. */
static const char *param_cap_name(const ek_node *p)
{
    const ek_node *t;

    if (!p || p->kind != EK_N_T_PARAM) {
        return NULL;
    }
    t = p->u.t_field.type;
    while (t && t->kind == EK_N_T_PAREN) {
        t = t->u.wrap.inner;
    }
    if (!t || t->kind != EK_N_T_NAME || t->u.path.count == 0) {
        return NULL;
    }
    return t->u.path.data[t->u.path.count - 1];
}

/* The bundle constructor the entry shim builds for one capability of
   the manifest, or NULL when the lowerer cannot supply it. */
static const char *shim_ctor_of(const char *cap)
{
    static const struct {
        const char *cap;
        const char *ctor;
    } ctors[] = {
        {"Out",   "ek_cap_out_stdout"},
        {"In",    "ek_cap_in_stdin"},
        {"Fs",    "ek_cap_fs_cwd"},
        {"Env",   "ek_cap_env_os"},
        {"Args",  "ek_cap_args"},
        {"Clock", "ek_cap_clock_wall"},
        {"Rand",  "ek_cap_rand_os"},
        {"Proc",  "ek_cap_proc"},
        {"Conc",  "ek_cap_conc_os"},
    };
    size_t i;

    for (i = 0; i < sizeof ctors / sizeof *ctors; i++) {
        if (strcmp(ctors[i].cap, cap) == 0) {
            return ctors[i].ctor;
        }
    }
    return NULL;
}

/* The authorization manifest: each capability registers the shared
   families its bundle pull names. 0 when a parameter is outside the
   table and the shim cannot stand. */
int entry_manifest(lower_ctx *c, const ek_node *decl, const ek_node *ann)
{
    const ek_nlist *params = &ann->u.t_fn.params;
    size_t i;

    for (i = 0; i < params->count; i++) {
        const char *cap = param_cap_name(params->data[i]);
        const char *ctor = cap ? shim_ctor_of(cap) : NULL;

        if (!ctor) {
            lower_error(c, decl, "a `main` parameter of `%s` is not "
                                 "supported by this lowerer",
                        cap ? cap : "?");
            c->nshim = 0;
            return 0;
        }
        c->shim[c->nshim++] = ctor;
        lower_cap_families_of(c, decl, cap);
    }
    return 1;
}

/* Records one test binding for the dispatching shim, unless its name
   was recorded already. */
void record_test(lower_ctx *c, const ek_symbol *sym, const char *name)
{
    lower_test *slots;
    size_t i;

    for (i = 0; i < c->ntests; i++) {
        if (strcmp(c->tests[i].name, sym->name) == 0) {
            return;
        }
    }
    slots = lower_grow(c, c->tests, &c->ctests, c->ntests + 1,
                       sizeof *slots);
    if (slots) {
        slots[c->ntests].name = sym->name;
        slots[c->ntests].cname = name;
        c->tests = slots;
        c->ntests++;
    }
}
