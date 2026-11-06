#include <string.h>

#include "check/internal.h"

/* The compiler-provided members of the std namespaces: the intrinsic
   boundary of the shipped surface. A declared namespace member wins
   over a row, and the derived services win over both, so a row is the
   default of a name no declaration owns. A namespace the table owns
   is complete: a name no station answers is absent, not bridged. */

static const ek_type *build(ck_ctx *c, const ek_sig *s, const ek_type **slots,
                            size_t nslots);

/* An embedded module's type symbol of one name, or NULL: the handles
   and data types live in the system vocabulary, the private bundle
   type of an injected pair only in the capability module. */
static const ek_symbol *module_sym(ck_ctx *c, const char *module,
                                   const char *name)
{
    size_t k, i;

    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind == EK_SYM_TYPE && strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

static const ek_type *build_fn(ck_ctx *c, const ek_sig *s,
                               const ek_type **slots, size_t nslots)
{
    const ek_type **ps = ek_arena_alloc(c->arena, s->nparams * sizeof *ps);
    const ek_type *r;
    size_t i;

    if (!ps) {
        c->oom = 1;
        return ck_ivar(c);
    }
    for (i = 0; i < s->nparams; i++) {
        ps[i] = build(c, s->params[i], slots, nslots);
    }
    r = build(c, s->result, slots, nslots);
    return ck_fn(c, ps, s->nparams, r, s->arrow);
}

static const ek_type *build(ck_ctx *c, const ek_sig *s, const ek_type **slots,
                            size_t nslots)
{
    const ek_symbol *sym;

    switch (s->kind) {
    case NS_VAR:
        return s->slot < (int)nslots && slots[s->slot]
                   ? slots[s->slot]
                   : (slots[s->slot] = ck_opaque(c));
    case NS_STD:
        if (strcmp(s->std, "Never") == 0) {
            return ck_never(c); /* the bottom type is no lookup */
        }
        return ck_std_type(c, s->std);
    case NS_EMB:
        sym = module_sym(c, s->mod, s->std);
        return sym ? ck_nominal(c, sym, NULL, 0) : ck_opaque(c);
    case NS_EMB1:
        sym = module_sym(c, s->mod, s->std);
        {
            const ek_type **args = ek_arena_alloc(c->arena, sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            return sym ? ck_nominal(c, sym, args, 1) : ck_opaque(c);
        }
    case NS_UNIT:
        return ck_unit(c);
    case NS_FN:
        return build_fn(c, s, slots, nslots);
    case NS_LIST:
        return ck_list(c, build(c, s->arg, slots, nslots));
    case NS_OPTION:
        sym = ck_std_symbol(c, "Option");
        {
            const ek_type **args = ek_arena_alloc(c->arena, sizeof *args);
            const ek_type *a = build(c, s->arg, slots, nslots);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = a;
            return sym ? ck_nominal(c, sym, args, 1) : ck_opaque(c);
        }
    case NS_RESULT:
        sym = ck_std_symbol(c, "Result");
        {
            const ek_symbol *perr = ck_std_symbol(c, "ParseError");
            const ek_type **args = ek_arena_alloc(c->arena, 2 * sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            args[1] = perr ? ck_nominal(c, perr, NULL, 0) : ck_opaque(c);
            return sym ? ck_nominal(c, sym, args, 2) : ck_opaque(c);
        }
    case NS_APP1:
        sym = ck_std_symbol(c, s->std);
        {
            const ek_type **args = ek_arena_alloc(c->arena, sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            return sym ? ck_nominal(c, sym, args, 1) : ck_opaque(c);
        }
    case NS_APP2:
        sym = ck_std_symbol(c, s->std);
        {
            const ek_type **args = ek_arena_alloc(c->arena, 2 * sizeof *args);

            if (!args) {
                c->oom = 1;
                return ck_ivar(c);
            }
            args[0] = build(c, s->arg, slots, nslots);
            args[1] = build(c, s->arg2, slots, nslots);
            return sym ? ck_nominal(c, sym, args, 2) : ck_opaque(c);
        }
    case NS_DICT:
        return ck_dict(c, build(c, s->arg, slots, nslots),
                       build(c, s->arg2, slots, nslots));
    default:
        return ck_opaque(c);
    }
}

/* The namespace a receiver or head type carries: the structural list
   is List; a nominal is named by its std or capability symbol -- the
   capability placeholders stay std rows, and the handles are
   declarations of the system vocabulary module (Clause 9.1.1). NULL
   names a namespace the table does not own. */
static const char *namespace_of(const ek_type *t)
{
    if (t->kind == EK_T_LIST) {
        return "List";
    }
    if (t->kind == EK_T_DICT) {
        return "Dict";
    }
    if (t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && t->u.nominal.sym->module
        && (strcmp(t->u.nominal.sym->module, "std") == 0
            || strcmp(t->u.nominal.sym->module, "sys") == 0
            || strcmp(t->u.nominal.sym->module, "cap") == 0
            || strcmp(t->u.nominal.sym->module, "time") == 0)) {
        return t->u.nominal.sym->name;
    }
    return NULL;
}

/* The one row a name carries over a namespace the table owns, with
   the row's variables instantiated fresh for this consultation. */
const ek_type *ck_ns_row(ck_ctx *c, const ek_type *recv, const char *name,
                         int *owned)
{
    const char *ns = namespace_of(recv);
    const ek_type *slots[4] = {0, 0, 0, 0};
    const ek_surface_row *row;
    size_t i;

    *owned = 0;
    if (!ns) {
        return NULL;
    }
    for (i = 0; i < ek_surface_rows_len; i++) {
        if (ek_surface_rows[i].name && strcmp(ns, ek_surface_rows[i].ns) == 0) {
            *owned = 1;
            if (strcmp(ek_surface_rows[i].name, name) == 0) {
                row = &ek_surface_rows[i];
                return build(c, row->sig, slots, 4);
            }
        }
    }
    return NULL;
}

/* True when the receiver's namespace row set is complete: a name no
   station answered is absent, not bridged. */
int ck_ns_owned(const ek_type *recv)
{
    return ek_surface_owned(namespace_of(recv));
}

/* The schemes of the injected pairs, hung on the cap module's
   undeclared function rows in the per-module phase: a body of the cap
   source reaches them by name, and every other module reaches neither
   the names nor the symbols. The signature's variables generalize, so
   a use instantiates them fresh. */
void ck_cap_schemes(ck_ctx *c)
{
    size_t k, i, j;

    for (k = 0; k < c->in->module_count && !c->oom; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, "cap") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];

            if (sym->kind != EK_SYM_FUNC || sym->decl || sym->type) {
                continue;
            }
            for (j = 0; j < ck_ns_pair_rows_len; j++) {
                if (strcmp(sym->name, ck_ns_pair_rows[j].name) == 0) {
                    const ek_type *slots[4] = {0, 0, 0, 0};
                    const ek_type *t;
                    ck_scheme *held;
                    size_t v;

                    for (v = 0; v < 4; v++) {
                        ek_type *tv = ck_fresh(c, EK_T_TVAR);

                        if (!tv) {
                            return;
                        }
                        tv->u.tvar.name = ck_ns_pair_rows[j].name;
                        slots[v] = tv;
                    }
                    t = build(c, ck_ns_pair_rows[j].sig, slots, 4);
                    held = ck_mono(c, t);
                    if (held) {
                        *held = ck_generalize(c, t);
                        held->bridge = 1;
                    }
                    ck_map_put(c, &c->schemes, sym, held);
                }
            }
        }
    }
}
