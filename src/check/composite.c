#include <string.h>

#include "check/internal.h"

/* Composite literals (Clause 7.5.3): the expected type decides the
   family, as it decides a brace pattern. A literal under an expected
   type the bridge ruling leaves open takes the opaque reading: values
   are checked, fields and keys are not. */

/* The family of a composite literal's items: 1 when the items are
   colon pairs. */
static int has_colons(const ek_node *e)
{
    size_t i;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM && item->u.pair.a) {
            return 1;
        }
    }
    return 0;
}

/* The field name of a colon item; NULL when the key is an expression
   (a dictionary key). */
static const char *item_field(const ek_node *item)
{
    const ek_node *key = item->u.pair.a;

    if (key && key->kind == EK_N_NAME && key->u.path.count == 1) {
        return key->u.path.data[0];
    }
    return NULL;
}

/* One item value at its expected type, conformed back: a placement
   alone cannot catch a String where an Int was expected. */
static void item_value(ck_ctx *c, const ek_node *val, const ek_type *expected)
{
    const ek_type *vt = ck_expr(c, val, expected);

    if (expected && ck_conforms(c, vt, expected, 1) != CK_OK) {
        ck_error(c, val, "E-type-mismatch",
                 "the item is %s, the expected type is %s (Clause 7.5.3)",
                 ck_type_name(c, vt), ck_type_name(c, expected));
    }
}

static const ek_type *record_lit(ck_ctx *c, const ek_node *e,
                                 const ek_type *rec, int allow_partial)
{
    size_t i, k;

    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];
        const char *field;
        const ek_type *ft = NULL;

        if (item->kind != EK_N_COMPOSITE_ITEM) {
            continue;
        }
        field = item_field(item);
        if (!field) {
            ck_error(c, item, "E-type-mismatch",
                     "a record literal names fields, not expressions "
                     "(Clause 7.5.3)");
            ck_expr(c, item->u.pair.b, NULL);
            continue;
        }
        for (k = 0; k < rec->u.record.count; k++) {
            if (strcmp(rec->u.record.fields[k].name, field) == 0) {
                ft = rec->u.record.fields[k].type;
            }
        }
        if (!ft) {
            ck_error(c, item, "E-field-missing",
                     "the record type has no field `%s` (Clause 7.5.3)",
                     field);
            ck_expr(c, item->u.pair.b, NULL);
            continue;
        }
        (void)0;
        for (k = 0; k < i; k++) {
            const ek_node *prev = e->u.kids.data[k];

            if (prev->kind == EK_N_COMPOSITE_ITEM) {
                const char *pf = item_field(prev);

                if (pf && strcmp(pf, field) == 0) {
                    ck_error(c, item, "E-field-duplicate",
                             "the field `%s` is named twice in the literal "
                             "(Clause 7.5.3)", field);
                }
            }
        }
        item_value(c, item->u.pair.b, ft);
    }
    if (!allow_partial && rec->u.record.count) {
        size_t found = 0;

        for (k = 0; k < rec->u.record.count; k++) {
            for (i = 0; i < e->u.kids.count; i++) {
                const ek_node *item = e->u.kids.data[i];
                const char *field = item->kind == EK_N_COMPOSITE_ITEM
                                        ? item_field(item) : NULL;

                if (field && strcmp(rec->u.record.fields[k].name, field) == 0) {
                    found++;
                    break;
                }
            }
        }
        if (found < rec->u.record.count) {
            ck_error(c, e, "E-partial-record",
                     "the record literal names %zu of %zu fields; a partial "
                     "literal is legal only under `with` (Clauses 7.5.3, "
                     "9.8.6)", found, rec->u.record.count);
        }
    }
    return rec;
}

const ek_type *ck_composite(ck_ctx *c, const ek_node *e,
                            const ek_type *expected, int allow_partial)
{
    const ek_type *exp = ck_prune(expected);
    int colons = has_colons(e);
    size_t i;

    if (c->oom) {
        return NULL;
    }
    /* The opaque placements: no expected type, or one the bridge
       ruling leaves undetermined. Values are still checked; a bare
       name key stays unread (record field or dictionary key is not
       decidable under opacity, and resolve resolved no such key). */
    if (!exp || exp->kind == EK_T_IVAR || exp->kind == EK_T_TVAR
        || (exp->kind == EK_T_NOMINAL && !exp->u.nominal.sym)) {
        for (i = 0; i < e->u.kids.count; i++) {
            const ek_node *item = e->u.kids.data[i];

            if (item->kind == EK_N_COMPOSITE_ITEM) {
                if (item->u.pair.a && item->u.pair.a->kind != EK_N_NAME) {
                    ck_expr(c, item->u.pair.a, ck_ivar(c));
                }
                ck_expr(c, item->u.pair.b, ck_ivar(c));
            }
        }
        if (!exp) {
            ck_error(c, e, "E-literal-expected",
                     "a composite literal never stands alone; it requires "
                     "an expected type or an annotation (Clause 7.5.3)");
            return ck_ivar(c);
        }
        return exp;
    }
    if (colons) {
        if (exp->kind == EK_T_RECORD) {
            return record_lit(c, e, exp, allow_partial);
        }
        if (exp->kind == EK_T_NOMINAL) {
            const ek_type *repr = ck_decl_repr(c, exp);

            if (repr && repr->kind == EK_T_RECORD) {
                record_lit(c, e, repr, allow_partial);
                return exp; /* the literal lands at the nominal type */
            }
        }
        if (exp->kind == EK_T_DICT) {
            for (i = 0; i < e->u.kids.count; i++) {
                const ek_node *item = e->u.kids.data[i];

                if (item->kind != EK_N_COMPOSITE_ITEM) {
                    continue;
                }
                item_value(c, item->u.pair.a, exp->u.dict.key);
                item_value(c, item->u.pair.b, exp->u.dict.val);
            }
            return exp;
        }
        ck_error(c, e, "E-type-mismatch",
                 "a colon literal expects a record or a dictionary, not "
                 "%s (Clause 7.5.3)", ck_type_name(c, exp));
        for (i = 0; i < e->u.kids.count; i++) {
            const ek_node *item = e->u.kids.data[i];

            if (item->kind == EK_N_COMPOSITE_ITEM) {
                ck_expr(c, item->u.pair.b, NULL);
            }
        }
        return exp;
    }
    switch (exp->kind) {
    case EK_T_LIST:
        for (i = 0; i < e->u.kids.count; i++) {
            const ek_node *item = e->u.kids.data[i];

            if (item->kind == EK_N_COMPOSITE_ITEM) {
                item_value(c, item->u.pair.b, exp->u.list.elem);
            }
        }
        return exp;
    case EK_T_TUPLE: {
        size_t n = 0;

        for (i = 0; i < e->u.kids.count; i++) {
            if (e->u.kids.data[i]->kind == EK_N_COMPOSITE_ITEM) {
                n++;
            }
        }
        if (n != exp->u.tuple.count) {
            ck_error(c, e, "E-type-mismatch",
                     "the tuple literal has %zu components, the expected "
                     "type has %zu (Clause 7.5.3)", n, exp->u.tuple.count);
        }
        for (i = 0; i < e->u.kids.count && i < exp->u.tuple.count; i++) {
            const ek_node *item = e->u.kids.data[i];

            if (item->kind == EK_N_COMPOSITE_ITEM) {
                item_value(c, item->u.pair.b, exp->u.tuple.items[i]);
            }
        }
        return exp;
    }
    case EK_T_NOMINAL: {
        /* A tuple shape extends to a generic instantiation by the
           shape of its right-hand side (Clause 7.5.5). */
        const ek_type *repr = ck_decl_repr(c, exp);

        if (repr && repr->kind == EK_T_TUPLE) {
            ck_composite(c, e, repr, 0);
            return exp; /* the literal lands at the instantiation */
        }
        ck_error(c, e, "E-type-mismatch",
                 "a comma literal expects a list or a tuple, not %s "
                 "(Clause 7.5.3)", ck_type_name(c, exp));
        break;
    }
    default:
        ck_error(c, e, "E-type-mismatch",
                 "a comma literal expects a list or a tuple, not %s "
                 "(Clause 7.5.3)", ck_type_name(c, exp));
        break;
    }
    for (i = 0; i < e->u.kids.count; i++) {
        const ek_node *item = e->u.kids.data[i];

        if (item->kind == EK_N_COMPOSITE_ITEM) {
            ck_expr(c, item->u.pair.b, NULL);
        }
    }
    return exp;
}
