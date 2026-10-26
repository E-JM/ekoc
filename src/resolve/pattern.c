#include <string.h>

#include "resolve/internal.h"

/* Pattern positions of Clause 7.7.1: the names a pattern binds, the
   duplicate rule of Clause 7.4.6 within one alternative, and the
   constructor references of Clause 7.4.2 row 3 (unqualified or
   module-qualified). */

#define R_PAT_MAX 64

/* A binding name binds a local symbol (Clause 7.7.1 row 1); `_` binds
   nothing. */
static void bind_var(r_ctx *c, r_file *f, const char *name, int line,
                     int col)
{
    ek_symbol *sym;

    if (!name || strcmp(name, "_") == 0 || c->oom) {
        return;
    }
    sym = r_sym(c, name, EK_SYM_LOCAL, f->mod->stem, line, col);
    if (sym) {
        r_scope_add(c, f->locals, name, sym);
    }
}

/* A module-qualified pattern constructor head: `Mod.Ctor` names the
   constructor even when a type shares the spelling (Clauses 7.4.1,
   7.4.4). */
static void cons_ref_qualified(r_ctx *c, r_file *f, const ek_node *pat,
                               const char *mname, const char *cname)
{
    const ek_symbol *mod = r_lookup_mod(f, mname);
    const ek_symbol *cs;
    r_mod *target;

    if (!mod) {
        r_error(c, f->mod->file, pat->line, pat->col, "E-unbound-name",
                "`%s` is not a module in scope (Clause 7.4.4)", mname);
        r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_CONS, NULL);
        return;
    }
    target = r_module_find(c, mod->name);
    cs = r_module_ctor(target, cname);
    if (!cs && strcmp(mod->name, "std") == 0) {
        /* The std placeholder bridge: the companion's surface is
           pending lib/ (resolve.h). */
        cs = r_std_ctor(c, cname);
        if (cs) {
            r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_EXTERNAL, cs);
            return;
        }
    }
    if (!cs) {
        r_error(c, f->mod->file, pat->line, pat->col, "E-unbound-member",
                "module `%s` declares no constructor `%s` "
                "(Clause 7.4.4)", mname, cname);
        r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_CONS, NULL);
        return;
    }
    if (!cs->public) {
        r_error(c, f->mod->file, pat->line, pat->col, "E-private-name",
                "constructor `%s` is private in `%s` (Clause 7.3.1)",
                cname, mname);
    }
    r_add_ref(c, pat, EK_SYM_CONSTRUCTOR, EK_ST_QUALIFIED, cs);
}

/* A pattern constructor head is a constructor reference, unqualified
   or module-qualified (Clauses 7.4.2 row 3, 7.4.4). */
static void cons_ref(r_ctx *c, r_file *f, const ek_node *pat)
{
    const ek_slist *path = &pat->u.cons.name;
    const ek_symbol *first = NULL;
    size_t k;

    if (path->count > 2) {
        r_error(c, f->mod->file, pat->line, pat->col, "E-unbound-name",
                "`%s.%s` is not a constructor position form; a "
                "qualified constructor has one module segment "
                "(Clause 7.4.4)", path->data[0],
                path->data[path->count - 1]);
        r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_CONS, NULL);
        return;
    }
    if (path->count == 2) {
        cons_ref_qualified(c, f, pat, path->data[0], path->data[1]);
        return;
    }
    k = r_lookup_ctor(f, path->count == 1 ? path->data[0] : "", &first);
    if (k > 1) {
        r_error(c, f->mod->file, pat->line, pat->col, "E-name-conflict",
                "`%s` names several visible constructors; qualify one "
                "(Clause 7.4.6)", path->data[0]);
        r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_CONS, NULL);
        return;
    }
    if (k == 1) {
        r_add_ref(c, pat, EK_SYM_CONSTRUCTOR, EK_ST_CONS, first);
        return;
    }
    r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_EXTERNAL,
              r_std_ctor(c, path->count == 1 ? path->data[0] : ""));
}

/* Constructor references at pattern positions. */
void r_pattern_refs(r_ctx *c, r_file *f, const ek_node *pat)
{
    size_t i;

    if (!pat || c->oom) {
        return;
    }
    switch (pat->kind) {
    case EK_N_P_CONS:
        cons_ref(c, f, pat);
        r_pattern_refs(c, f, pat->u.cons.payload);
        return;
    case EK_N_P_PAREN:
    case EK_N_P_OR:
        for (i = 0; i < pat->u.kids.count; i++) {
            r_pattern_refs(c, f, pat->u.kids.data[i]);
        }
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            r_pattern_refs(c, f, pat->u.p_brace.items.data[i]);
        }
        return;
    case EK_N_P_FIELD:
        r_pattern_refs(c, f, pat->u.p_field.pattern);
        return;
    default:
        return;
    }
}

void r_pattern_binds(r_ctx *c, r_file *f, const ek_node *pat)
{
    size_t i;

    if (c->oom || !pat) {
        return;
    }
    switch (pat->kind) {
    case EK_N_P_VAR:
        bind_var(c, f, pat->u.path.data[0], pat->line, pat->col);
        return;
    case EK_N_P_PAREN:
    case EK_N_P_OR:
        for (i = 0; i < pat->u.kids.count; i++) {
            r_pattern_binds(c, f, pat->u.kids.data[i]);
        }
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            r_pattern_binds(c, f, pat->u.p_brace.items.data[i]);
        }
        bind_var(c, f, pat->u.p_brace.rest_name, pat->line, pat->col);
        return;
    case EK_N_P_FIELD:
        r_pattern_binds(c, f, pat->u.p_field.pattern);
        return;
    case EK_N_P_CONS:
        r_pattern_binds(c, f, pat->u.cons.payload);
        return;
    default:
        return;
    }
}

/* One binding occurrence within one alternative: the second of the
   same name is the conflict (Clauses 7.4.6, 7.7.1). */
static void mark_seen(r_ctx *c, const r_file *f, const char *name,
                      int line, int col, const char **seen, size_t *n)
{
    size_t i;

    for (i = 0; i < *n; i++) {
        if (strcmp(seen[i], name) == 0) {
            r_error(c, f->mod->file, line, col, "E-name-conflict",
                    "`%s` is bound twice in one pattern (Clauses "
                    "7.4.6, 7.7.1)", name);
            return;
        }
    }
    if (*n < R_PAT_MAX) {
        seen[(*n)++] = name;
    }
}

/* Duplicates within one alternative of one pattern (Clauses 7.4.6,
   7.7.1), diagnosed at the second occurrence. */
static void dup_walk(r_ctx *c, const r_file *f, const ek_node *pat,
                     const char **seen, size_t *n)
{
    size_t i;

    if (!pat || c->oom) {
        return;
    }
    switch (pat->kind) {
    case EK_N_P_VAR:
        if (strcmp(pat->u.path.data[0], "_") == 0) {
            return;
        }
        mark_seen(c, f, pat->u.path.data[0], pat->line, pat->col, seen, n);
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            dup_walk(c, f, pat->u.p_brace.items.data[i], seen, n);
        }
        if (pat->u.p_brace.rest_name) {
            mark_seen(c, f, pat->u.p_brace.rest_name, pat->line,
                      pat->col, seen, n);
        }
        return;
    case EK_N_P_FIELD:
        dup_walk(c, f, pat->u.p_field.pattern, seen, n);
        return;
    case EK_N_P_CONS:
        dup_walk(c, f, pat->u.cons.payload, seen, n);
        return;
    case EK_N_P_PAREN:
        for (i = 0; i < pat->u.kids.count; i++) {
            dup_walk(c, f, pat->u.kids.data[i], seen, n);
        }
        return;
    default:
        return;
    }
}

void r_pattern_dup(r_ctx *c, r_file *f, const ek_node *pat)
{
    size_t i;

    if (!pat || c->oom) {
        return;
    }
    if (pat->kind == EK_N_P_OR) {
        /* The rule applies separately to each or-alternative
           (Clause 7.7.1). */
        for (i = 0; i < pat->u.kids.count; i++) {
            const char *alt[R_PAT_MAX];
            size_t an = 0;

            dup_walk(c, f, pat->u.kids.data[i], alt, &an);
        }
        return;
    }
    {
        const char *seen[R_PAT_MAX];
        size_t n = 0;

        dup_walk(c, f, pat, seen, &n);
    }
}
