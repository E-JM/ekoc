#include <string.h>

#include "resolve/internal.h"

/* The type rows of a module's surface, and the spellings it exports: a
   declared type or ctype enters the identity layer with its
   constructors, and every nominal spelling a public export mentions
   must be public where importers see it. */

/* A constructor of the name already emitted for the type declaration
   whose first symbol sits at first. */
static int variant_seen(const r_surf *s, size_t first, const char *name)
{
    size_t i;

    for (i = first; i < s->n; i++) {
        if (s->syms[i].kind == EK_SYM_CONSTRUCTOR
            && strcmp(s->syms[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The constructor rows of a type declaration: one per variant, the
   type's visibility beside a `_` spelling. */
static void collect_variants(r_surf *s, const ek_node *d,
                             const ek_symbol *type, size_t first)
{
    const char *name = d->u.type_decl.name;
    size_t i;

    for (i = 0; i < d->u.type_decl.variants.count; i++) {
        const ek_node *v = d->u.type_decl.variants.data[i];
        ek_symbol *cs;

        if (variant_seen(s, first, v->u.variant.name)) {
            r_error(s->c, s->m->file, v->line, v->col, "E-name-conflict",
                    "the constructor `%s` is declared twice in the type "
                    "`%s` (Clause 7.4.6)", v->u.variant.name, name);
            continue;
        }
        cs = r_sym(s->c, v->u.variant.name, EK_SYM_CONSTRUCTOR,
                   s->m->stem, v->line, v->col);

        if (!cs) {
            return;
        }
        cs->decl = d;
        cs->owner = d->u.type_decl.name;
        /* Visibility follows the type; a `_`-prefixed constructor
           spelling is the one split, private to the declaring
           module. */
        cs->public = type->public && v->u.variant.name[0] != '_';
        r_surf_emit(s, cs);
    }
}

/* A declared type or ctype: one symbol of the identity layer. */
void r_surf_type(r_surf *s, const ek_node *d)
{
    const char *name = d->kind == EK_N_TYPE_DECL
                           ? d->u.type_decl.name : d->u.decl.name;
    ek_symbol *sym = r_sym(s->c, name, EK_SYM_TYPE,
                           s->m->stem, d->line, d->col);
    size_t first = s->n;
    int seen;

    if (!sym) {
        return;
    }
    if (strcmp(name, "Never") == 0) {
        /* The spelling belongs to the language's bottom type; no
           module declares it. */
        r_error(s->c, s->m->file, d->line, d->col, "E-name-conflict",
                "`Never` is a type of the language and cannot be declared "
                "(Clause 7.5.4)");
    }
    sym->decl = d;
    sym->public = name[0] != '_';
    seen = r_surf_key_seen(s->tkeys, s->nt, name);
    if (seen >= 0) {
        r_error(s->c, s->m->file, d->line, d->col, "E-name-conflict",
                "`%s` is declared twice in this module (Clause 7.4.6)",
                name);
    } else {
        r_surf_key_push(s, &s->tkeys, &s->nt, &s->ctk, name, d->line,
                        d->col);
    }
    r_surf_emit(s, sym);
    if (d->kind != EK_N_TYPE_DECL) {
        return;
    }
    collect_variants(s, d, sym, first);
}

/* A private type declared in this module, by name. */
static int private_type_here(const r_surf *s, const char *name)
{
    size_t i;

    for (i = 0; i < s->n; i++) {
        if (s->syms[i].kind == EK_SYM_TYPE
            && !s->syms[i].public && strcmp(s->syms[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Every nominal spelling a type expression mentions must be public
   where the expression is exported surface. */
static void private_in_type(r_surf *s, const ek_node *t, const char *what);

/* The type forms whose children are lists: the function's parameters
   and result, the tuple and record members, the apply head and args. */
static void private_in_type_kids(r_surf *s, const ek_node *t,
                                 const char *what)
{
    size_t i;

    switch (t->kind) {
    case EK_N_T_FN:
        for (i = 0; i < t->u.t_fn.params.count; i++) {
            private_in_type(s, t->u.t_fn.params.data[i], what);
        }
        private_in_type(s, t->u.t_fn.result, what);
        return;
    case EK_N_T_TUPLE:
    case EK_N_T_RECORD:
        for (i = 0; i < t->u.kids.count; i++) {
            private_in_type(s, t->u.kids.data[i], what);
        }
        return;
    case EK_N_T_APPLY:
        private_in_type(s, t->u.t_apply.head, what);
        for (i = 0; i < t->u.t_apply.args.count; i++) {
            private_in_type(s, t->u.t_apply.args.data[i], what);
        }
        return;
    default:
        return;
    }
}

/* Every nominal spelling a type expression mentions must be public
   where the expression is exported surface. */
static void private_in_type(r_surf *s, const ek_node *t, const char *what)
{
    if (!t || s->c->oom) {
        return;
    }
    switch (t->kind) {
    case EK_N_T_NAME: {
        const char *n = t->u.path.data[t->u.path.count - 1];

        if (private_type_here(s, n)) {
            r_error(s->c, s->m->file, t->line, t->col, "E-private-name",
                    "the %s names the private type `%s` (Clause 7.3.3)",
                    what, n);
        }
        return;
    }
    case EK_N_T_FIELD:
    case EK_N_T_PARAM:
        private_in_type(s, t->u.t_field.type, what);
        return;
    case EK_N_T_LIST:
    case EK_N_T_PAREN:
        private_in_type(s, t->u.wrap.inner, what);
        return;
    case EK_N_T_DICT:
        private_in_type(s, t->u.pair.a, what);
        private_in_type(s, t->u.pair.b, what);
        return;
    default:
        private_in_type_kids(s, t, what);
        return;
    }
}

/* The exported surface spells only public types: a public binding's
   annotation, and the payload and field types of a public type or
   ctype. A value of a public type flows to importers, so every name
   its surface mentions must be spellable there. */
void r_surf_visibility(r_surf *s)
{
    const ek_node *mod = s->m->ast;
    size_t i, k;

    for (i = 0; i < mod->u.kids.count; i++) {
        const ek_node *d = mod->u.kids.data[i];

        if (d->kind == EK_N_TYPE_DECL) {
            if (d->u.type_decl.name[0] == '_') {
                continue;
            }
            for (k = 0; k < d->u.type_decl.variants.count; k++) {
                private_in_type(s, d->u.type_decl.variants.data[k]
                                   ->u.variant.payload,
                                "surface of a public type");
            }
            private_in_type(s, d->u.type_decl.type,
                            "surface of a public type");
        } else if (d->kind == EK_N_CTYPE_DECL) {
            if (d->u.decl.name[0] == '_') {
                continue;
            }
            private_in_type(s, d->u.decl.body, "surface of a public ctype");
        } else if (d->kind == EK_N_LET && d->u.bind.type
                   && d->u.bind.pattern
                   && d->u.bind.pattern->kind == EK_N_P_VAR
                   && d->u.bind.pattern->u.path.count == 1
                   && d->u.bind.pattern->u.path.data[0][0] != '_') {
            private_in_type(s, d->u.bind.type, "public binding signature");
        } else if (d->kind == EK_N_EXTERN_DECL
                   && d->u.extern_decl.name[0] != '_') {
            private_in_type(s, d->u.extern_decl.type,
                            "public extern signature");
        } else if (d->kind == EK_N_NAMESPACE
                   && d->u.namespace.name[0] != '_') {
            private_in_type(s, d->u.namespace.type,
                            "public member signature");
        }
    }
}
