#include <string.h>

#include "resolve/internal.h"

/* Pattern positions: the names a pattern binds, the duplicate rule
   within one alternative, and the constructor references (unqualified
   or module-qualified). */

#define R_PAT_MAX 64

/* Records one local binding site for the unused-binding warning. */
static void note_local(r_ctx *c, const ek_symbol *sym)
{
    if (c->oom) {
        return;
    }
    if (c->nlocals == c->clocals) {
        c->locals = r_grow(c, c->locals, c->nlocals, &c->clocals,
                           sizeof *c->locals);
        if (c->oom) {
            return;
        }
    }
    c->locals[c->nlocals].sym = sym;
    c->locals[c->nlocals].group = c->pat_group;
    c->nlocals++;
}

/* A binding name binds a local symbol; `_` binds nothing. */
static void bind_var(r_ctx *c, r_file *f, const char *name, int line,
                     int col)
{
    ek_symbol *sym;

    if (!name || strcmp(name, "_") == 0 || c->oom) {
        return;
    }
    sym = r_sym(c, name, EK_SYM_LOCAL, f->mod->stem, line, col);
    if (sym) {
        if (c->binding_node && c->binding_node->deprecated) {
            sym->decl = c->binding_node;
        }
        r_scope_add(c, f->locals, name, sym);
        note_local(c, sym);
    }
}

/* A module-qualified pattern constructor head: `Mod.Ctor` names the
   constructor even when a type shares the spelling. */
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
    if (cs && r_is_placeholder(cs)) {
        /* A placeholder constructor keeps the bridged reading. */
        r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_EXTERNAL, cs);
        return;
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
   or module-qualified. An unqualified head hands every visible
   same-named constructor to check; a qualified head names one. */
static void cons_ref(r_ctx *c, r_file *f, const ek_node *pat)
{
    const ek_slist *path = &pat->u.cons.name;
    const ek_symbol *const *cands = NULL;
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
    k = r_lookup_ctor(c, f, path->count == 1 ? path->data[0] : "",
                      &cands);
    if (k == 0) {
        const ek_symbol *priv = r_std_find(f->std, EK_SYM_CONSTRUCTOR,
                                           path->data[0]);

        if (path->count == 1 && strcmp(path->data[0], "Never") == 0) {
            /* The language's bottom type has no constructor; the
               pattern miss is not the bridge. */
            r_error(c, f->mod->file, pat->line, pat->col,
                    "E-unbound-name",
                    "`Never` is a type of the language; it names no "
                    "constructor (Clause 7.5.4)");
            r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_EXTERNAL, NULL);
            return;
        }
        if (priv && !priv->public) {
            r_error(c, f->mod->file, pat->line, pat->col,
                    "E-private-name",
                    "the constructor `%s` is private in module `%s` "
                    "(Clause 7.3.3)", path->data[0], priv->module);
        }
        /* Unknown to the file, std, and cap: an unbound name, not the
           unresolved-external bridge (resolve.h). */
        r_error(c, f->mod->file, pat->line, pat->col, "E-unbound-name",
                "`%s` names no visible constructor (Clause 7.4.2)",
                path->data[0]);
        r_add_ref(c, pat, EK_SYM_EXTERNAL, EK_ST_EXTERNAL, NULL);
        return;
    }
    r_add_ctor_ref(c, pat, EK_SYM_CONSTRUCTOR, EK_ST_CONS, cands[0],
                   cands, k);
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

static void binds_walk(r_ctx *c, r_file *f, const ek_node *pat)
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
            binds_walk(c, f, pat->u.kids.data[i]);
        }
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < pat->u.p_brace.items.count; i++) {
            binds_walk(c, f, pat->u.p_brace.items.data[i]);
        }
        bind_var(c, f, pat->u.p_brace.rest_name, pat->line, pat->col);
        return;
    case EK_N_P_FIELD:
        binds_walk(c, f, pat->u.p_field.pattern);
        return;
    case EK_N_P_CONS:
        binds_walk(c, f, pat->u.cons.payload);
        return;
    default:
        return;
    }
}

/* The public entry marks the whole pattern as one binding group, so the
   alternatives of an or-pattern share it. */
void r_pattern_binds(r_ctx *c, r_file *f, const ek_node *pat)
{
    const ek_node *saved = c->pat_group;

    if (c->oom || !pat) {
        return;
    }
    c->pat_group = pat;
    binds_walk(c, f, pat);
    c->pat_group = saved;
}

/* One binding occurrence within one alternative: the second of the
   same name is the conflict. */
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

/* Duplicates within one alternative of one pattern, diagnosed at the
   second occurrence. */
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
        /* The rule applies separately to each or-alternative. */
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
