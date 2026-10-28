#include <string.h>

#include "check/internal.h"

/* The instantiation graph of Clause 8.7.3. Every generic use recorded
   in a checked body is an edge from the body's declaration to the
   callee, carrying the callee's scheme variables as the actuals the
   call site determined, written in the caller's variables. A use in a
   body with no variables is a ground seed; expanding the seeds along
   the edges enumerates the monomorphization instances. A finite set is
   a conforming program; a set larger than the bound means expansion
   does not terminate, and the edge that produced the overflow names
   the call site in the diagnostic. */

#define CK_INST_LIMIT 1024

typedef struct {
    const ck_scheme *decl;
    const ek_type **args;
    size_t nargs;
} inst_state;

/* The surface symbol a declaration node belongs to, if any. A nested
   binding has no symbol: its scheme pointer identifies it. */
const ek_symbol *ck_decl_symbol(ck_ctx *c, const ek_node *m)
{
    size_t i;

    if (!c || !m || !c->cur) {
        return NULL;
    }
    for (i = 0; i < c->cur->count; i++) {
        if (c->cur->syms[i].decl == m) {
            return &c->cur->syms[i];
        }
    }
    return NULL;
}

/* The body's variables reordered to the canonical scheme's variable
   order. The two sets come from two elaborations of one annotation;
   a variable's spelling is unique there, so the names pair them. */
static const ek_type **caller_vars(ck_ctx *c, const ck_scheme *caller,
                                   size_t *n)
{
    const ek_type **vs;
    size_t i, j;

    *n = 0;
    if (!caller || !caller->nvars) {
        return NULL;
    }
    if (c->inst_nvars != caller->nvars) {
        return NULL;
    }
    vs = ek_arena_alloc(c->arena, caller->nvars * sizeof *vs);
    if (!vs) {
        c->oom = 1;
        return NULL;
    }
    for (i = 0; i < caller->nvars; i++) {
        const ek_type *want = caller->vars[i];

        vs[i] = NULL;
        for (j = 0; j < c->inst_nvars; j++) {
            const ek_type *have = c->inst_vars[j];

            if (have == want) {
                vs[i] = have;
                break;
            }
            if (have && want && have->kind == EK_T_TVAR
                && want->kind == EK_T_TVAR
                && strcmp(have->u.tvar.name, want->u.tvar.name) == 0) {
                vs[i] = have;
                break;
            }
        }
        if (!vs[i]) {
            return NULL;
        }
    }
    *n = caller->nvars;
    return vs;
}

/* Records one instantiation at the use marked by inst_call. The
   capture maps each callee scheme variable to the inference variable
   the instantiation made for it; unification has since determined
   those, and the simulation reads them after the module pass. */
static void inst_note(ck_ctx *c, const char *name, const ck_scheme *s,
                      const ck_map *capture)
{
    ck_inst_edge *e;
    const ek_type **vs;
    size_t nvs = 0, i;

    if (c->oom || !c->inst_body || !c->inst_call || !s || !s->nvars
        || !s->type) {
        return;
    }
    vs = caller_vars(c, c->inst_decl, &nvs);
    if (c->inst_decl && c->inst_decl->nvars && !vs) {
        return; /* the body's variables do not align with the scheme */
    }
    if (c->ninst == c->cinst) {
        c->inst_edges = ck_grow(c, c->inst_edges, c->ninst + 1, &c->cinst,
                                sizeof *c->inst_edges);
        if (c->oom) {
            return;
        }
    }
    e = &c->inst_edges[c->ninst++];
    memset(e, 0, sizeof *e);
    e->caller = c->inst_decl;
    e->callee = s;
    e->name = name;
    e->mod = c->cur;
    e->at = c->inst_call;
    e->caller_vars = vs;
    e->ncaller_vars = nvs;
    e->nargs = s->nvars;
    e->args = ek_arena_alloc(c->arena, s->nvars * sizeof *e->args);
    if (!e->args) {
        c->oom = 1;
        return;
    }
    for (i = 0; i < s->nvars; i++) {
        const ek_type *v = capture ? ck_map_get(capture, s->vars[i]) : NULL;

        e->args[i] = v ? v : s->vars[i];
    }
}

/* Instantiates a scheme for a use, capturing the variable mapping
   when the use is a generic call inside a checked body. */
const ek_type *ck_instantiate_use(ck_ctx *c, const char *name,
                                  const ck_scheme *s)
{
    ck_map capture;
    const ek_type *t;

    if (!s || !s->type) {
        return NULL;
    }
    if (!c->inst_body || !c->inst_call || !s->nvars) {
        return ck_instantiate(c, s);
    }
    memset(&capture, 0, sizeof capture);
    {
        ck_map *saved = c->inst_capture;

        c->inst_capture = &capture;
        t = ck_instantiate(c, s);
        c->inst_capture = saved;
    }
    inst_note(c, name, s, &capture);
    return t;
}

/* Whether a type carries no variable at all: an instance whose
   arguments are not ground cannot be expanded. */
static int ground_type(const ek_type *t)
{
    size_t i;

    t = ck_prune(t);
    if (!t) {
        return 0;
    }
    if (t->kind == EK_T_IVAR || t->kind == EK_T_TVAR) {
        return 0;
    }
    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            if (!ground_type(t->u.nominal.args[i])) {
                return 0;
            }
        }
        return 1;
    case EK_T_LIST:
        return ground_type(t->u.list.elem);
    case EK_T_DICT:
        return ground_type(t->u.dict.key) && ground_type(t->u.dict.val);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (!ground_type(t->u.tuple.items[i])) {
                return 0;
            }
        }
        return 1;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (!ground_type(t->u.record.fields[i].type)) {
                return 0;
            }
        }
        return 1;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            if (!ground_type(t->u.fn.params[i])) {
                return 0;
            }
        }
        return ground_type(t->u.fn.result);
    default:
        return 1;
    }
}

/* The closed form of one argument: bound inference variables resolved
   away and the structure canonicalized for pointer identity. */
static const ek_type *norm_type(ck_ctx *c, const ek_type *t)
{
    return ck_intern(c, ck_subst(c, t));
}

static int state_same(const inst_state *s, const ck_scheme *decl,
                      const ek_type **args)
{
    size_t i;

    if (s->decl != decl || s->nargs == 0) {
        return 0;
    }
    for (i = 0; i < s->nargs; i++) {
        if (s->args[i] != args[i]) {
            return 0;
        }
    }
    return 1;
}

static int state_seen(const inst_state *states, size_t n,
                      const ck_scheme *decl, const ek_type **args)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (state_same(&states[i], decl, args)) {
            return 1;
        }
    }
    return 0;
}

/* The instance set exceeded the bound: the edge that produced the
   overflowing state is the call site the diagnostic names. */
static void inst_overflow(ck_ctx *c, const ck_inst_edge *e)
{
    const ek_surface *saved = c->cur;

    if (e->mod) {
        c->cur = e->mod;
    }
    ck_error(c, e->at, "E-instantiation-depth",
             "generic instantiation does not terminate: `%s` reaches more "
             "than %d distinct instances (Clause 8.7.3)",
             e->name ? e->name : "the called function", CK_INST_LIMIT);
    c->cur = saved;
}

void ck_inst_depth(ck_ctx *c)
{
    inst_state *states = NULL;
    size_t n = 0, cap = 0, i, k;

    if (c->oom || !c->ninst) {
        return;
    }
    /* Seeds: uses in bodies whose declaration has no type variables.
       The actuals are ground once the module pass has settled the
       inference variables. */
    for (i = 0; i < c->ninst && !c->oom; i++) {
        const ck_inst_edge *e = &c->inst_edges[i];
        const ek_type **args;
        size_t q;
        int ok = 1;

        if ((e->caller && e->caller->nvars) || !e->callee
            || !e->callee->nvars || !e->nargs) {
            continue;
        }
        args = ek_arena_alloc(c->arena, e->nargs * sizeof *args);
        if (!args) {
            c->oom = 1;
            return;
        }
        for (q = 0; q < e->nargs; q++) {
            args[q] = norm_type(c, e->args[q]);
            if (!ground_type(args[q])) {
                ok = 0;
            }
        }
        if (!ok || state_seen(states, n, e->callee, args)) {
            continue;
        }
        if (n == CK_INST_LIMIT) {
            inst_overflow(c, e);
            return;
        }
        if (n == cap) {
            states = ck_grow(c, states, n + 1, &cap, sizeof *states);
            if (c->oom) {
                return;
            }
        }
        states[n].decl = e->callee;
        states[n].args = args;
        states[n].nargs = e->nargs;
        n++;
    }
    /* Expansion: a state supplies the actuals of its declaration's
       variables to every edge leaving it. */
    for (k = 0; k < n && !c->oom; k++) {
        for (i = 0; i < c->ninst && !c->oom; i++) {
            const ck_inst_edge *e = &c->inst_edges[i];
            const ek_type **args;
            size_t q;
            int ok = 1;

            if (e->caller != states[k].decl || !e->callee
                || !e->callee->nvars || !e->nargs) {
                continue;
            }
            if (e->ncaller_vars != states[k].nargs) {
                continue;
            }
            args = ek_arena_alloc(c->arena, e->nargs * sizeof *args);
            if (!args) {
                c->oom = 1;
                return;
            }
            for (q = 0; q < e->nargs; q++) {
                const ek_type *t = ck_apply_binders(
                    c, e->args[q], e->caller_vars, states[k].args,
                    e->ncaller_vars);

                args[q] = norm_type(c, t);
                if (!ground_type(args[q])) {
                    ok = 0;
                }
            }
            if (!ok || state_seen(states, n, e->callee, args)) {
                continue;
            }
            if (n == CK_INST_LIMIT) {
                inst_overflow(c, e);
                return;
            }
            if (n == cap) {
                states = ck_grow(c, states, n + 1, &cap, sizeof *states);
                if (c->oom) {
                    return;
                }
            }
            states[n].decl = e->callee;
            states[n].args = args;
            states[n].nargs = e->nargs;
            n++;
        }
    }
}
