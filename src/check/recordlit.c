#include <string.h>

#include "check/internal.h"

/* The record family of composite literals: the items are colon pairs
   naming fields, each value is checked at its field's type, and a
   literal outside `with` names every field. */

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

/* A field named twice in one literal is E-field-duplicate. */
static void dup_field(ck_ctx *c, const ek_node *e, const ek_node *item,
                      size_t i, const char *field)
{
    size_t k;

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
}

/* A literal outside `with` must name every field. */
static void check_complete(ck_ctx *c, const ek_node *e, const ek_type *rec)
{
    size_t found = 0, k, i;

    for (k = 0; k < rec->u.record.count; k++) {
        for (i = 0; i < e->u.kids.count; i++) {
            const ek_node *item = e->u.kids.data[i];
            const char *field = item->kind == EK_N_COMPOSITE_ITEM
                                    ? item_field(item) : NULL;

            if (field
                && strcmp(rec->u.record.fields[k].name, field) == 0) {
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

/* One item value at its expected type, conformed back: a placement
   alone cannot catch a String where an Int was expected. Inside a
   `with` argument the partial-literal permission follows the field
   values down, layer by layer (Clause 9.8.6); every other position
   resets it, so a partial literal nested under a list or dictionary
   element stays illegal. */
void ck_item_value(ck_ctx *c, const ek_node *val, const ek_type *expected,
                   int allow_partial)
{
    int saved = c->partial_ok;
    const ek_type *vt;

    c->partial_ok = allow_partial;
    vt = ck_expr(c, val, expected);
    c->partial_ok = saved;
    if (expected && ck_conforms(c, vt, expected, 1) != CK_OK) {
        ck_error(c, val, "E-type-mismatch",
                 "the item is %s, the expected type is %s (Clause 7.5.3)",
                 ck_type_name(c, vt), ck_type_name(c, expected));
    }
}

const ek_type *ck_record_lit(ck_ctx *c, const ek_node *e,
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
        dup_field(c, e, item, i, field);
        ck_item_value(c, item->u.pair.b, ft, allow_partial);
    }
    if (!allow_partial && rec->u.record.count) {
        check_complete(c, e, rec);
    }
    return rec;
}
