#include <stdlib.h>
#include <string.h>

#include "lower/internal.h"

/* The generated struct layer: every composite value lives behind a
   reference to a struct that embeds the runtime object header first.
   A declared type's record or tuple representation, each constructed
   constructor of an algebraic type, the anonymous record and tuple
   shapes a body builds or reads, and one environment struct per
   closure register here while the bodies lower; the families
   themselves are emitted into the unit before the functions. Registry
   identity is the symbol pointer for the declared families and the
   shape key for the anonymous ones, so two spellings of one shape
   share a struct and two constructors never do. */

/* Finds a registered family by symbol or shape key. */
lower_gtype *lower_find_family(const lower_ctx *c, const ek_symbol *sym,
                                const char *key)
{
    size_t i;

    for (i = 0; i < c->ngt; i++) {
        const lower_gtype *g = &c->gtypes[i];

        if (sym ? g->key_sym == sym
                : (g->key_str && key && strcmp(g->key_str, key) == 0)) {
            return &c->gtypes[i];
        }
    }
    return NULL;
}

lower_gtype *lower_add_family(lower_ctx *c)
{
    lower_gtype *slots = lower_grow(c, c->gtypes, &c->cgt, c->ngt + 1,
                                    sizeof *slots);

    if (!slots) {
        return NULL;
    }
    c->gtypes = slots;
    memset(&slots[c->ngt], 0, sizeof slots[c->ngt]);
    return &slots[c->ngt++];
}

/* Finds a registered family by its stem, the name the typeinfo and
   the drop carry; NULL when no family registers under it. */
lower_gtype *lower_gtype_of_stem(lower_ctx *c, const char *stem)
{
    size_t i;

    for (i = 0; i < c->ngt; i++) {
        if (strcmp(c->gtypes[i].stem, stem) == 0) {
            return &c->gtypes[i];
        }
    }
    return NULL;
}

/* Registers the record or tuple representation of a declared type:
   the layout follows the declaration's right-hand side, records in
   declaration order. A ctype's family takes its fields from its own
   record body, in declaration order, and the layout is frozen there;
   the C-side mirror registers separately at the boundary. A std
   placeholder or an algebraic type has no representation and
   registers nothing. */
/* Fills the family slot of one declared representation from its field
   kids: a record layout carries the written names, a tuple layout
   stays positional. */
static lower_gtype *register_repr(lower_ctx *c, const ek_symbol *tsym,
                                  const ek_nlist *kids, lower_gshape shape)
{
    lower_gtype *g;
    size_t cap = 0, i;

    g = lower_add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_sym = tsym;
    g->stem = lower_str(c, "%s$%s", tsym->module, tsym->name);
    g->helper = lower_str(c, "ekn_%s$%s$rec", tsym->module, tsym->name);
    g->shape = shape;
    g->nf = kids->count;
    if (shape == GT_REC) {
        g->fnames = lower_grow(c, NULL, &cap, g->nf, sizeof *g->fnames);
        if (!g->fnames) {
            c->ngt--;
            return NULL;
        }
        for (i = 0; i < g->nf; i++) {
            g->fnames[i] = kids->data[i]->u.t_field.name;
        }
    }
    g->used = 1;
    return g;
}

/* Registers the record or tuple representation of a declared type:
   the layout follows the declaration's right-hand side, records in
   declaration order. A ctype's family takes its fields from its own
   record body, in declaration order, and the layout is frozen there;
   the C-side mirror registers separately at the boundary. A std
   placeholder or an algebraic type has no representation and
   registers nothing. */
lower_gtype *lower_decl_repr(lower_ctx *c, const ek_symbol *tsym)
{
    lower_gtype *g;
    const ek_node *dn;

    if (!tsym) {
        return NULL;
    }
    g = lower_find_family(c, tsym, NULL);
    if (g) {
        g->used = 1;
        return g;
    }
    dn = tsym->decl;
    if (!dn) {
        return NULL;
    }
    if (dn->kind == EK_N_CTYPE_DECL) {
        const ek_node *rec = dn->u.decl.body;

        if (!rec || rec->kind != EK_N_T_RECORD) {
            return NULL;
        }
        return register_repr(c, tsym, &rec->u.kids, GT_REC);
    }
    if (dn->kind != EK_N_TYPE_DECL) {
        return NULL;
    }
    if (dn->u.type_decl.rhs != EK_TYPE_RHS_RECORD
        && dn->u.type_decl.rhs != EK_TYPE_RHS_TUPLE) {
        return NULL;
    }
    return register_repr(c, tsym, &dn->u.type_decl.type->u.kids,
                         dn->u.type_decl.rhs == EK_TYPE_RHS_TUPLE ? GT_TUP
                                                                  : GT_REC);
}

/* The registry key of an anonymous record: its field names in the
   canonical sorted order, so one shape shares one family whichever
   path reads it -- the checker stores record fields sorted, the
   type-syntax walk in written order. */
static int key_name_order(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static const char *record_key(lower_ctx *c, const ek_type *rec)
{
    ek_buf b;
    const char **names;
    size_t cap = 0, i;

    ek_buf_init(&b);
    names = lower_grow(c, NULL, &cap, rec->u.record.count, sizeof *names);
    if (!names && rec->u.record.count) {
        ek_buf_destroy(&b);
        return NULL;
    }
    for (i = 0; i < rec->u.record.count; i++) {
        names[i] = rec->u.record.fields[i].name;
    }
    qsort(names, rec->u.record.count, sizeof *names, key_name_order);
    for (i = 0; i < rec->u.record.count; i++) {
        if (i && !ek_buf_append_char(&b, ',')) {
            c->oom = 1;
        }
        if (!ek_buf_append_str(&b, names[i])) {
            c->oom = 1;
        }
    }
    return lower_take(c, &b);
}

/* Registers the anonymous record shape of a record type: fields in
   the checker's sorted order, shared with every equal field set. */
lower_gtype *lower_anon_record(lower_ctx *c, const ek_type *rec)
{
    const char *key = record_key(c, rec);
    lower_gtype *g;
    size_t cap = 0, i;

    if (c->oom) {
        return NULL;
    }
    g = lower_find_family(c, NULL, key);
    if (g) {
        g->used = 1;
        return g;
    }
    g = lower_add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_str = key;
    g->stem = lower_str(c, "rec%u", c->anon_rec++);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_REC;
    g->nf = rec->u.record.count;
    if (g->nf) {
        g->fnames = lower_grow(c, NULL, &cap, g->nf, sizeof *g->fnames);
        if (!g->fnames) {
            c->ngt--;
            return NULL;
        }
        for (i = 0; i < g->nf; i++) {
            size_t k = rec->u.record.decl ? rec->u.record.decl[i] : i;

            g->fnames[i] = rec->u.record.fields[k].name;
        }
    }
    g->used = 1;
    return g;
}

/* Registers the anonymous tuple shape of an arity: same arity, same
   layout, one family. */
lower_gtype *lower_anon_tuple(lower_ctx *c, size_t n)
{
    const char *key = lower_str(c, "tup:%zu", n);
    lower_gtype *g = lower_find_family(c, NULL, key);

    if (g) {
        g->used = 1;
        return g;
    }
    g = lower_add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_str = key;
    g->stem = lower_str(c, "tup_%zu", n);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_TUP;
    g->nf = n;
    g->used = 1;
    return g;
}

/* The C text reading one field of a receiver temp: the receiver's
   type decides the family, and the read is an owned copy through the
   retention helper. */
const char *lower_field_slot(lower_ctx *c, const ek_node *at,
                             const ek_type *recv, const char *temp,
                             const char *name)
{
    lower_gtype *g = NULL;

    if (recv->kind == EK_T_NOMINAL) {
        if (recv->u.nominal.sym) {
            g = lower_decl_repr(c, recv->u.nominal.sym);
        }
    } else if (recv->kind == EK_T_RECORD) {
        g = lower_anon_record(c, recv);
    }
    if (!g || g->shape != GT_REC) {
        lower_error(c, at, "the field `%s` has no lowering", name);
        return NULL;
    }
    return lower_str(c, "((ekS_%s *)%s.as.ref)->f_%s", g->stem, temp,
                     name);
}

void lower_use_list(lower_ctx *c)
{
    c->uses_list = 1;
}

/* The declared record or tuple representation of a nominal as a value
   type, the binders replaced by the applied arguments; NULL when the
   declaration carries neither shape. The composite literals read the
   field and component types out of it. */
const ek_type *lower_repr_type(lower_ctx *c, const ek_type *nom)
{
    const ek_symbol *tsym;
    const ek_node *dn;

    if (!nom || nom->kind != EK_T_NOMINAL || !nom->u.nominal.sym) {
        return NULL;
    }
    tsym = nom->u.nominal.sym;
    dn = tsym->decl;
    if (!dn || dn->kind != EK_N_TYPE_DECL
        || (dn->u.type_decl.rhs != EK_TYPE_RHS_RECORD
            && dn->u.type_decl.rhs != EK_TYPE_RHS_TUPLE)) {
        return NULL;
    }
    return lower_applied_type(c, dn->u.type_decl.type, dn, nom);
}

/* The field type of one written field name in a record value type; a
   NULL name names no field, so a positional item of an ill-typed
   literal reads no expectation and check's own rejection stands. */
const ek_type *lower_record_field(const ek_type *rec, const char *name)
{
    size_t i;

    if (!rec || rec->kind != EK_T_RECORD || !name) {
        return NULL;
    }
    for (i = 0; i < rec->u.record.count; i++) {
        if (strcmp(rec->u.record.fields[i].name, name) == 0) {
            return rec->u.record.fields[i].type;
        }
    }
    return NULL;
}

/* The payload type of the constructor a reference denotes, applied
   under the owner's instantiation; NULL for a zero-payload variant or
   a constructor the owner's declaration does not carry. */
const ek_type *lower_ctor_payload_type(lower_ctx *c, const ek_ref *ref,
                                       const ek_type *nom)
{
    const ek_symbol *tsym;
    const ek_symbol *cs;
    const ek_node *dn;
    size_t i;

    if (!nom || nom->kind != EK_T_NOMINAL || !nom->u.nominal.sym) {
        return NULL;
    }
    tsym = nom->u.nominal.sym;
    cs = lower_pick_ctor(ref, tsym);
    dn = tsym->decl;
    if (!cs || !dn || dn->kind != EK_N_TYPE_DECL) {
        return NULL;
    }
    for (i = 0; i < dn->u.type_decl.variants.count; i++) {
        const ek_node *v = dn->u.type_decl.variants.data[i];

        if (strcmp(v->u.variant.name, cs->name) == 0) {
            if (!v->u.variant.payload) {
                return NULL;
            }
            return lower_applied_type(c, v->u.variant.payload, dn, nom);
        }
    }
    return NULL;
}

/* The spelling piece of one range element slot. */
static const char *range_slot_name(lower_scalar slot)
{
    switch (slot) {
    case EKSC_UINT: return "UInt";
    case EKSC_CHAR: return "Char";
    default: return "Int";
    }
}

/* The generated struct family of one Range over an element scalar:
   the endpoints and the closed flag, shared by the two operator
   forms and keyed by the element slot, so Int and Char ranges never
   share a layout. The runtime's traversal entries read the fields
   through the layout contract in rt/ekort.h. */
lower_gtype *lower_range_family(lower_ctx *c, lower_scalar slot)
{
    const char *elem = range_slot_name(slot);
    const char *key = lower_str(c, "range:%s", elem);
    lower_gtype *g = lower_find_family(c, NULL, key);
    const char **slots;
    size_t cap = 0;

    if (g) {
        g->used = 1;
        return g;
    }
    g = lower_add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_str = key;
    g->stem = lower_str(c, "std$Range$%s", elem);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_REC;
    g->nf = 3;
    slots = lower_grow(c, NULL, &cap, 3, sizeof *slots);
    if (!slots) {
        c->ngt--;
        return NULL;
    }
    slots[0] = "start";
    slots[1] = "end";
    slots[2] = "closed";
    g->fnames = slots;
    g->used = 1;
    g->constructed = 1;
    return g;
}
