#include <string.h>

#include "check/internal.h"

/* The entry rules and the root pre-pass. A test root has no `main`
   requirement, so the rules fire only where a binding named `main`
   exists; entry-module uniqueness belongs to the host. */

/* Unannotated top-level bindings are monomorphic: one inference
   variable per name, shared by every reference. */
void ck_prebind(ck_ctx *c)
{
    size_t i;

    for (i = 0; i < c->cur->count; i++) {
        const ek_symbol *sym = &c->cur->syms[i];

        if ((sym->kind == EK_SYM_FUNC || sym->kind == EK_SYM_EXTERN)
            && !sym->type && !ck_map_get(&c->schemes, sym)) {
            ck_map_put(c, &c->schemes, sym, ck_mono(c, ck_opaque(c)));
        }
    }
}

static int is_capability(const ek_type *t)
{
    static const char *caps[] = {"Out",  "In",   "Fs",    "Env",  "Args",
                                 "Clock", "Rand", "Proc", "Conc"};
    size_t i;

    t = ck_prune(t);
    if (!t || t->kind != EK_T_NOMINAL || !t->u.nominal.sym
        || t->u.nominal.argc != 0) {
        return 0;
    }
    for (i = 0; i < sizeof caps / sizeof *caps; i++) {
        if (strcmp(t->u.nominal.sym->name, caps[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Notes one capability name of `main`; a repeat is a name conflict. */
static void note_capability(ck_ctx *c, const ek_symbol *sym,
                            const char **seen, size_t *nseen,
                            const char *cn)
{
    size_t q;
    int dup = 0;

    for (q = 0; q < *nseen; q++) {
        if (strcmp(seen[q], cn) == 0) {
            dup = 1;
        }
    }
    if (dup) {
        ck_error(c, sym->decl, "E-name-conflict",
                 "a capability type occurs twice in `main` "
                 "(Clauses 7.1.3, 7.4.6)");
    } else if (*nseen < 9) {
        seen[(*nseen)++] = cn;
    }
}

/* The type-level rules for a `main` of the root module. */
static void check_main(ck_ctx *c, const ek_symbol *sym)
{
    const char *seen[9];
    ck_tvars tv;
    const ek_type *t;
    size_t j, nseen = 0;

    if (!sym->public) {
        ck_error(c, sym->decl, "E-entry-point",
                 "`main` MUST be public (Clause 7.1.3)");
    }
    memset(&tv, 0, sizeof tv);
    t = sym->type ? ck_elaborate(c, sym->type, &tv) : NULL;
    if (!t || t->kind != EK_T_FN) {
        ck_error(c, sym->decl, "E-entry-point",
                 "`main` carries a function annotation (Clause 7.1.3)");
        return;
    }
    if (ck_prune(t->u.fn.result)->kind != EK_T_UNIT) {
        ck_error(c, sym->decl, "E-entry-point",
                 "the result type of `main` is `_` (Clause 7.1.3)");
    }
    for (j = 0; j < t->u.fn.count; j++) {
        const ek_type *p = t->u.fn.params[j];

        if (!is_capability(p)) {
            ck_error(c, sym->decl, "E-entry-point",
                     "every parameter of `main` has a capability type "
                     "(Clause 7.1.3)");
            continue;
        }
        note_capability(c, sym, seen, &nseen,
                        ck_prune(p)->u.nominal.sym->name);
    }
}

/* The `main` rules read the whole closure once: the entry name is
   legal only in the entry module. */
void ck_entry_rules(ck_ctx *c)
{
    size_t k, i;

    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        for (i = 0; i < s->count; i++) {
            const ek_symbol *sym = &s->syms[i];
            int is_main = strcmp(sym->name, "main") == 0;
            int is_priv = strcmp(sym->name, "_main") == 0;

            if (!is_main && !is_priv) {
                continue;
            }
            if (sym->kind != EK_SYM_FUNC && sym->kind != EK_SYM_EXTERN) {
                continue;
            }
            if (is_priv) {
                ck_error(c, sym->decl, "E-entry-point",
                         "`_main` is not a legal entry name (Clause 7.1.3)");
            } else if (k != 0) {
                ck_error(c, sym->decl, "E-entry-point",
                         "`main` is legal only in the entry module "
                         "(Clause 7.1.3)");
            } else {
                check_main(c, sym);
            }
        }
    }
}

/* The current module's declared types, with the phantom-binder
   check. */
void ck_decls(ck_ctx *c)
{
    size_t i;

    for (i = 0; i < c->cur->count; i++) {
        const ek_symbol *sym = &c->cur->syms[i];

        if (sym->kind == EK_SYM_TYPE && sym->decl) {
            ck_decl_of(c, sym);
        }
    }
}
