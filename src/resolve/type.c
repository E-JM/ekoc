#include <string.h>

#include "resolve/internal.h"

/* Type position: a type variable of the enclosing annotation or
   declaration, a declared type in unqualified scope, or a
   module-qualified type. A type variable spelled like an imported
   module is a same-layer conflict. Lookup order per position: type
   variables first, then declared types; module segments are prefixes,
   not lookups. */

static void tvar(r_ctx *c, r_file *f, const ek_node *t, const char *name)
{
    if (strcmp(name, "_") == 0) {
        /* `_` at type position is the unit type, never a variable;
           check owns it. */
        return;
    }
    if (r_lookup_mod(f, name)) {
        r_error(c, f->mod->file, t->line, t->col, "E-name-conflict",
                "type variable `%s` collides with an imported module "
                "(Clause 7.4.6)", name);
    }
    r_add_ref(c, t, EK_SYM_TVAR, EK_ST_TYPE, NULL);
}

static void declared(r_ctx *c, r_file *f, const ek_node *t, const char *name)
{
    const ek_symbol *first = NULL;
    size_t n = r_lookup_type(f, name, &first);

    if (n > 1) {
        /* Same-named types coexist; the unqualified spelling is the
           point of rejection. */
        r_error(c, f->mod->file, t->line, t->col, "E-name-conflict",
                "`%s` names several visible types; qualify one "
                "(Clauses 7.4.5, 7.4.6)", name);
        r_add_ref(c, t, EK_SYM_EXTERNAL, EK_ST_TYPE, NULL);
        return;
    }
    if (n == 1) {
        r_add_ref(c, t, EK_SYM_TYPE, EK_ST_TYPE, first);
        return;
    }
    /* Unknown here and in std: the unresolved bridge (resolve.h). */
    r_add_ref(c, t, EK_SYM_EXTERNAL, EK_ST_TYPE, NULL);
}

static void qualified(r_ctx *c, r_file *f, const ek_node *t)
{
    const char *file = f->mod->file;
    const char *mname = t->u.path.data[0];
    const char *tname = t->u.path.data[1];
    const ek_symbol *mod = r_lookup_mod(f, mname);
    r_mod *m;

    if (!mod) {
        r_error(c, file, t->line, t->col, "E-unbound-name",
                "`%s` is not a module in scope (Clause 7.4.4)", mname);
        r_add_ref(c, t, EK_SYM_EXTERNAL, EK_ST_TYPE, NULL);
        return;
    }
    m = r_module_find(c, mod->name);
    if (!m || !m->surf) {
        /* The std placeholder surface (resolve.h): nothing to walk. */
        r_add_ref(c, t, EK_SYM_EXTERNAL, EK_ST_TYPE, NULL);
        return;
    }
    {
        size_t i;

        for (i = 0; i < m->surf->count; i++) {
            const ek_symbol *s = &m->surf->syms[i];

            if (s->kind == EK_SYM_TYPE && strcmp(s->name, tname) == 0) {
                if (!s->public) {
                    r_error(c, file, t->line, t->col, "E-private-name",
                            "type `%s` is private in `%s` (Clause 7.3.1)",
                            tname, mname);
                }
                r_add_ref(c, t, EK_SYM_TYPE, EK_ST_QUALIFIED, s);
                return;
            }
        }
    }
    r_error(c, file, t->line, t->col, "E-unbound-member",
            "module `%s` declares no type `%s` (Clause 7.4.4)", mname,
            tname);
    r_add_ref(c, t, EK_SYM_EXTERNAL, EK_ST_TYPE, NULL);
}

/* The recursive container and function forms. */
static void r_type_sub(r_ctx *c, r_file *f, const ek_node *t)
{
    size_t i;

    switch (t->kind) {
    case EK_N_T_APPLY:
        r_type(c, f, t->u.t_apply.head);
        for (i = 0; i < t->u.t_apply.args.count; i++) {
            r_type(c, f, t->u.t_apply.args.data[i]);
        }
        return;
    case EK_N_T_LIST:
    case EK_N_T_PAREN:
        r_type(c, f, t->u.wrap.inner);
        return;
    case EK_N_T_DICT:
        r_type(c, f, t->u.pair.a);
        r_type(c, f, t->u.pair.b);
        return;
    case EK_N_T_TUPLE:
    case EK_N_T_RECORD:
        for (i = 0; i < t->u.kids.count; i++) {
            r_type(c, f, t->u.kids.data[i]);
        }
        return;
    case EK_N_T_FN:
        for (i = 0; i < t->u.t_fn.params.count; i++) {
            r_type(c, f, t->u.t_fn.params.data[i]);
        }
        if (t->u.t_fn.result) {
            r_type(c, f, t->u.t_fn.result);
        }
        return;
    case EK_N_T_FIELD:
    case EK_N_T_PARAM:
        r_type(c, f, t->u.t_field.type);
        return;
    default:
        return;
    }
}

void r_type(r_ctx *c, r_file *f, const ek_node *t)
{
    if (c->oom || !t) {
        return;
    }
    if (t->kind == EK_N_T_NAME) {
        if (t->u.path.count == 1) {
            if (r_is_snake(t->u.path.data[0])) {
                tvar(c, f, t, t->u.path.data[0]);
            } else {
                declared(c, f, t, t->u.path.data[0]);
            }
        } else if (t->u.path.count == 2) {
            qualified(c, f, t);
        } else {
            r_error(c, f->mod->file, t->line, t->col, "E-unbound-name",
                    "`%s.%s` is not a type position form; a qualified "
                    "type has one module segment (Clause 7.4.4)",
                    t->u.path.data[0],
                    t->u.path.data[t->u.path.count - 1]);
            r_add_ref(c, t, EK_SYM_EXTERNAL, EK_ST_TYPE, NULL);
        }
        return;
    }
    r_type_sub(c, f, t);
}
