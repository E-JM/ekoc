#include "check/internal.h"

/* The anonymous representation of a record- or tuple-shaped nominal
   type: the bracket group of its declaration, with the declaration's
   binders replaced by the applied arguments. */

/* The record representation, binders replaced by the applied
   arguments. */
static const ek_type *repr_record(ck_ctx *c, const ck_decl *d,
                                  const ek_node *rec, const ek_type *nom,
                                  ck_tvars *tv)
{
    ek_tfield *fs;
    size_t i, n = rec->u.kids.count;

    fs = ek_arena_alloc(c->arena, n * sizeof *fs);
    if (!fs) {
        c->oom = 1;
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const ek_node *f = rec->u.kids.data[i];

        fs[i].name = f->u.t_field.name;
        fs[i].type = ck_apply_binders(c,
                                      ck_elaborate(c, f->u.t_field.type,
                                                   tv),
                                      d->binders, nom->u.nominal.args,
                                      d->nbinders);
    }
    return ck_record(c, fs, n);
}

/* The tuple representation, binders replaced likewise. */
static const ek_type *repr_tuple(ck_ctx *c, const ck_decl *d,
                                 const ek_node *rec, const ek_type *nom,
                                 ck_tvars *tv)
{
    const ek_type **items;
    size_t i, n = rec->u.kids.count;

    items = ek_arena_alloc(c->arena, n * sizeof *items);
    if (!items) {
        c->oom = 1;
        return NULL;
    }
    for (i = 0; i < n; i++) {
        const ek_node *item = rec->u.kids.data[i];

        items[i] = ck_apply_binders(c,
                                    ck_elaborate(c,
                                                 item->kind == EK_N_T_FIELD
                                                     ? item->u.t_field
                                                           .type
                                                     : item,
                                                 tv),
                                    d->binders, nom->u.nominal.args,
                                    d->nbinders);
    }
    return ck_tuple(c, items, n);
}

/* The anonymous representation of a record- or tuple-shaped nominal
   type, with its binders replaced by the applied arguments. */
const ek_type *ck_decl_repr(ck_ctx *c, const ek_type *nom)
{
    const ck_decl *d;
    const ek_node *rec;
    ck_tvars tv;

    if (!nom->u.nominal.sym) {
        return NULL; /* std-opaque: no representation */
    }
    d = ck_decl_of(c, nom->u.nominal.sym);
    if (!d || (d->rhs != EK_TYPE_RHS_RECORD && d->rhs != EK_TYPE_RHS_TUPLE)) {
        return NULL;
    }
    if (d->nbinders && nom->u.nominal.argc != d->nbinders) {
        return NULL; /* unapplied: partial application does not exist */
    }
    rec = d->rec;
    if (!rec) {
        return NULL;
    }
    ck_binder_scope(c, d, &tv);
    if (d->rhs == EK_TYPE_RHS_RECORD) {
        return repr_record(c, d, rec, nom, &tv);
    }
    return repr_tuple(c, d, rec, nom, &tv);
}
