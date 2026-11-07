#include <string.h>

#include "check/internal.h"

/* Pattern typing: a pattern is checked against the scrutinee type;
   the expected type decides the brace family, as it decides a
   composite literal. Constructor patterns live in patcons.c,
   or-patterns and the value binding shapes in patbind.c. */

/* The std type a string or character literal is placed at. */
static const char *lit_std_name(const ek_node *lit)
{
    return lit->kind == EK_N_CHAR_LIT ? "Char" : "String";
}

/* A literal pattern is placed at the scrutinee type; against an
   internally known composite it is a type error, and a string or
   character literal does not conform to a scrutinee of any other
   visible type. */
static const ek_type *lit_pattern(ck_ctx *c, const ek_node *p,
                                  const ek_type *scrut)
{
    const ek_node *lit = p->u.wrap.inner;
    int neg = lit && lit->kind == EK_N_UNARY;
    const ek_type *s = ck_prune(scrut);

    if (neg) {
        lit = lit->u.wrap.inner;
    }
    if (lit) {
        ck_place(c, lit, scrut, neg);
    }
    if (lit && (lit->kind == EK_N_STRING_LIT || lit->kind == EK_N_CHAR_LIT)
        && s && s->kind == EK_T_NOMINAL && s->u.nominal.sym
        && !s->u.nominal.sym->decl
        && !ck_is_std(s, lit_std_name(lit))) {
        ck_error(c, lit, "E-type-mismatch",
                 "a %s literal does not match %s (Clause 7.7.1)",
                 lit->kind == EK_N_CHAR_LIT ? "character" : "string",
                 ck_type_name(c, s));
    }
    if (s && (s->kind == EK_T_LIST || s->kind == EK_T_DICT
              || s->kind == EK_T_TUPLE || s->kind == EK_T_RECORD
              || s->kind == EK_T_FN
              || (s->kind == EK_T_NOMINAL && s->u.nominal.sym
                  && s->u.nominal.sym->decl))) {
        ck_error(c, p, "E-type-mismatch",
                 "a literal pattern does not match %s (Clause 7.7.1)",
                 ck_type_name(c, s));
    }
    return scrut;
}

/* Record-pattern item: the field name, whether a `f: P` pair or the
   shorthand `f`. */
static const char *pat_field(const ek_node *item, const ek_node **sub)
{
    if (item->kind == EK_N_P_FIELD) {
        *sub = item->u.p_field.pattern;
        return item->u.p_field.name;
    }
    if (item->kind == EK_N_P_VAR) {
        *sub = item;
        return item->u.path.count == 1 ? item->u.path.data[0] : NULL;
    }
    *sub = NULL;
    return NULL;
}

static const ek_type *record_pattern(ck_ctx *c, const ek_node *p,
                                     const ek_type *rec)
{
    size_t i, k;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        const ek_node *item = p->u.p_brace.items.data[i];
        const ek_node *sub = NULL;
        const char *field = pat_field(item, &sub);
        const ek_type *ft = NULL;

        if (!field) {
            continue;
        }
        for (k = 0; k < rec->u.record.count; k++) {
            if (strcmp(rec->u.record.fields[k].name, field) == 0) {
                ft = rec->u.record.fields[k].type;
            }
        }
        if (!ft) {
            ck_error(c, item, "E-field-missing",
                     "the record type has no field `%s` (Clause 7.7.1)",
                     field);
            continue;
        }
        if (sub && sub != item) {
            ck_pattern(c, sub, ft);
        } else if (sub) {
            /* Shorthand: the item itself binds. */
            ck_scope_bind(c, field, ft);
        }
    }
    return rec;
}

static const ek_type *list_pattern(ck_ctx *c, const ek_node *p,
                                   const ek_type *lst)
{
    size_t i;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        ck_pattern(c, p->u.p_brace.items.data[i], lst->u.list.elem);
    }
    if (p->u.p_brace.rest_name) {
        ck_scope_bind(c, p->u.p_brace.rest_name, lst);
    }
    return lst;
}

static const ek_type *tuple_pattern(ck_ctx *c, const ek_node *p,
                                    const ek_type *tup)
{
    size_t i, n = 0;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        if (p->u.p_brace.items.data[i]->kind != EK_N_P_FIELD) {
            n++;
        }
    }
    if (p->u.p_brace.rest != EK_REST_NONE || n != tup->u.tuple.count) {
        ck_error(c, p, "E-type-mismatch",
                 "the tuple pattern has %zu components, the scrutinee has "
                 "%zu (Clause 7.7.1)", n, tup->u.tuple.count);
    }
    for (i = 0; i < p->u.p_brace.items.count && i < tup->u.tuple.count; i++) {
        ck_pattern(c, p->u.p_brace.items.data[i], tup->u.tuple.items[i]);
    }
    return tup;
}

/* The opaque readings: the brace family is not decidable, so items
   bind against fresh variables and nothing is rejected. */
static const ek_type *opaque_pattern(ck_ctx *c, const ek_node *p)
{
    size_t i;

    for (i = 0; i < p->u.p_brace.items.count; i++) {
        const ek_node *item = p->u.p_brace.items.data[i];
        const ek_node *sub = NULL;
        const char *field = pat_field(item, &sub);

        if (field && sub && sub != item) {
            ck_pattern(c, sub, ck_opaque(c));
        } else if (item->kind == EK_N_P_VAR && item->u.path.count == 1) {
            ck_scope_bind(c, item->u.path.data[0], ck_opaque(c));
        } else if (item->kind != EK_N_P_FIELD) {
            ck_pattern(c, item, ck_opaque(c));
        }
    }
    if (p->u.p_brace.rest_name) {
        ck_scope_bind(c, p->u.p_brace.rest_name, ck_opaque(c));
    }
    return ck_opaque(c);
}

static const ek_type *brace_pattern(ck_ctx *c, const ek_node *p,
                                    const ek_type *scrut)
{
    const ek_type *s = ck_prune(scrut);

    if (s && s->kind == EK_T_LIST) {
        return list_pattern(c, p, s);
    }
    if (s && s->kind == EK_T_TUPLE) {
        return tuple_pattern(c, p, s);
    }
    if (s && s->kind == EK_T_RECORD) {
        return record_pattern(c, p, s);
    }
    if (s && s->kind == EK_T_NOMINAL && s->u.nominal.sym) {
        const ek_type *repr = ck_decl_repr(c, s);

        if (repr && repr->kind == EK_T_RECORD) {
            return record_pattern(c, p, repr);
        }
        if (repr && repr->kind == EK_T_TUPLE) {
            return tuple_pattern(c, p, repr);
        }
    }
    if (!s || s->kind == EK_T_IVAR || s->kind == EK_T_TVAR
        || (s->kind == EK_T_NOMINAL && !s->u.nominal.sym)) {
        return opaque_pattern(c, p);
    }
    ck_error(c, p, "E-type-mismatch",
             "a brace pattern does not match %s (Clause 7.7.1)",
             ck_type_name(c, s));
    return opaque_pattern(c, p);
}

const ek_type *ck_pattern(ck_ctx *c, const ek_node *p, const ek_type *scrut)
{
    if (c->oom || !p) {
        return scrut;
    }
    ck_row(c, p, scrut, EK_RD_NONE);
    switch (p->kind) {
    case EK_N_P_WILDCARD:
        return scrut;
    case EK_N_P_VAR:
        if (p->u.path.count == 1 && strcmp(p->u.path.data[0], "_") != 0) {
            ck_scope_bind(c, p->u.path.data[0], scrut);
        }
        return scrut;
    case EK_N_P_LIT:
        return lit_pattern(c, p, scrut);
    case EK_N_P_CONS:
        return cons_pattern(c, p, scrut);
    case EK_N_P_BRACE:
        return brace_pattern(c, p, scrut);
    case EK_N_P_PAREN:
        if (p->u.kids.count == 1) {
            return ck_pattern(c, p->u.kids.data[0], scrut);
        }
        return scrut; /* multi-scrutinee groups are typed per column */
    case EK_N_P_OR:
        return ck_or_pattern(c, p, scrut);
    case EK_N_P_FIELD:
        return ck_pattern(c, p->u.p_field.pattern, scrut);
    default:
        return scrut;
    }
}
