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
static lower_gtype *find_family(const lower_ctx *c, const ek_symbol *sym,
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

static lower_gtype *add_family(lower_ctx *c)
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

/* Registers the record or tuple representation of a declared type:
   the layout follows the declaration's right-hand side, records in
   declaration order. A std placeholder or an algebraic type has no
   representation and registers nothing. */
lower_gtype *lower_decl_repr(lower_ctx *c, const ek_symbol *tsym)
{
    lower_gtype *g;
    const ek_node *dn;
    const ek_nlist *kids;
    size_t cap = 0, i;

    if (!tsym) {
        return NULL;
    }
    g = find_family(c, tsym, NULL);
    if (g) {
        g->used = 1;
        return g;
    }
    dn = tsym->decl;
    if (!dn || dn->kind != EK_N_TYPE_DECL) {
        return NULL;
    }
    if (dn->u.type_decl.rhs != EK_TYPE_RHS_RECORD
        && dn->u.type_decl.rhs != EK_TYPE_RHS_TUPLE) {
        return NULL;
    }
    kids = &dn->u.type_decl.type->u.kids;
    g = add_family(c);
    if (!g) {
        return NULL;
    }
    g->key_sym = tsym;
    g->stem = lower_str(c, "%s_%s", tsym->module, tsym->name);
    g->helper = lower_str(c, "ekn_%s_%s_rec", tsym->module, tsym->name);
    g->shape = dn->u.type_decl.rhs == EK_TYPE_RHS_TUPLE ? GT_TUP : GT_REC;
    g->nf = kids->count;
    if (g->shape == GT_REC) {
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

/* The registry key of an anonymous record: its field names in the
   canonical sorted order the checker reports. */
static const char *record_key(lower_ctx *c, const ek_type *rec)
{
    ek_buf b;
    size_t i;

    ek_buf_init(&b);
    for (i = 0; i < rec->u.record.count; i++) {
        if (i && !ek_buf_append_char(&b, ',')) {
            c->oom = 1;
        }
        if (!ek_buf_append_str(&b, rec->u.record.fields[i].name)) {
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
    g = find_family(c, NULL, key);
    if (g) {
        g->used = 1;
        return g;
    }
    g = add_family(c);
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
            g->fnames[i] = rec->u.record.fields[i].name;
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
    lower_gtype *g = find_family(c, NULL, key);

    if (g) {
        g->used = 1;
        return g;
    }
    g = add_family(c);
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

/* The constructor a reference denotes, re-derived the way check picks
   one: the candidate owned by the expected type's declaration. */
static const ek_symbol *pick_ctor(const ek_ref *ref, const ek_symbol *tsym)
{
    size_t i;

    for (i = 0; i < ref->ncands; i++) {
        const ek_symbol *cand = ref->cands[i];

        if (cand && cand->owner && cand->module && tsym->module
            && strcmp(cand->owner, tsym->name) == 0
            && strcmp(cand->module, tsym->module) == 0) {
            return cand;
        }
    }
    if (ref->ncands == 0 && ref->symbol && ref->symbol->owner
        && strcmp(ref->symbol->owner, tsym->name) == 0) {
        return ref->symbol;
    }
    return NULL;
}

/* Registers the constructor family of an algebraic type: one struct
   per constructor, its payload the variant's own. */
lower_gtype *lower_ctor_family(lower_ctx *c, const ek_node *at,
                               const ek_ref *ref, const ek_symbol *tsym)
{
    const ek_symbol *cs = tsym ? pick_ctor(ref, tsym) : NULL;
    const ek_node *dn = tsym ? tsym->decl : NULL;
    const char **slots;
    lower_gtype *g;
    size_t cap = 0, i;

    if (!cs || !dn || dn->kind != EK_N_TYPE_DECL) {
        lower_error(c, at, "the constructor `%s` is not supported by "
                          "this lowerer",
                    ref && ref->symbol ? ref->symbol->name : "?");
        return NULL;
    }
    g = find_family(c, cs, NULL);
    if (g) {
        return g;
    }
    for (i = 0; i < dn->u.type_decl.variants.count; i++) {
        const ek_node *v = dn->u.type_decl.variants.data[i];

        if (strcmp(v->u.variant.name, cs->name) != 0) {
            continue;
        }
        g = add_family(c);
        if (!g) {
            return NULL;
        }
        g->key_sym = cs;
        g->stem = lower_str(c, "%s_%s_%s", tsym->module, tsym->name,
                            cs->name);
        g->helper = lower_str(c, "ekn_%s", g->stem);
        g->shape = GT_CTOR;
        g->nf = v->u.variant.payload ? 1 : 0;
        g->used = 1;
        g->constructed = 1;
        if (g->nf) {
            slots = lower_grow(c, NULL, &cap, 1, sizeof *slots);
            if (slots) {
                slots[0] = "";
                g->fnames = slots;
            }
        }
        return g;
    }
    lower_error(c, at, "the constructor `%s` is not supported by this "
                       "lowerer", cs->name);
    return NULL;
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

/* Registers the environment struct of one closure: its fields are the
   captured free names, in capture order. Every closure keeps its own
   family, so two capture sets never share a layout. */
lower_gtype *lower_env_family(lower_ctx *c, const char **names, size_t n)
{
    lower_gtype *g = add_family(c);
    const char **slots;
    size_t cap = 0, i;

    if (!g) {
        return NULL;
    }
    g->key_str = lower_str(c, "env:%u", c->anon_env);
    g->stem = lower_str(c, "env%u", c->anon_env++);
    g->helper = lower_str(c, "ekn_%s", g->stem);
    g->shape = GT_REC;
    g->nf = n;
    g->used = 1;
    g->constructed = 1;
    if (n) {
        slots = lower_grow(c, NULL, &cap, n, sizeof *slots);
        if (!slots) {
            c->ngt--;
            return NULL;
        }
        g->fnames = slots;
        for (i = 0; i < n; i++) {
            g->fnames[i] = names[i];
        }
    }
    return g;
}
void lower_use_list(lower_ctx *c)
{
    c->uses_list = 1;
}

#include <string.h>

#include "lower/internal.h"

/* The emission of the generated struct families: the typedefs, the
   drop and typeinfo pair, the construction helpers, and the list fold.
   The families register while the bodies lower; this text joins the
   unit before the functions. */
