#include <string.h>

#include "check/internal.h"

/* The column branches of the exhaustiveness walk that split over
   structure: the constructor branch of an algebraic column and the
   component branch of a tuple or record column. */

/* The constructor's payload instantiated at the column's arguments. */
static const ek_type *ctor_payload(ck_ctx *c, const ck_ctor *ct,
                                   const ek_type *col, const ck_decl *d)
{
    size_t nb = d ? d->nbinders : 0;

    if (!ct->payload) {
        return NULL;
    }
    if (nb && col->kind == EK_T_NOMINAL && col->u.nominal.argc == nb) {
        return ck_apply_binders(c, ct->payload, d->binders,
                                col->u.nominal.args, nb);
    }
    return ct->payload;
}

/* The constructor's matching alternatives contribute their payload
   patterns; a bare arm over a payload-carrying constructor matches
   nothing. */
static void ctor_alts(ck_ctx *c, ex_rows *out, const ek_node **row,
                      size_t ncols, size_t pos, const ck_ctor *ct,
                      const ek_node *const *alts, size_t nalt)
{
    size_t na;

    for (na = 0; na < nalt && !c->oom; na++) {
        const ek_node *alt = alts[na];
        const ek_node *one[1];

        if (alt->kind != EK_N_P_CONS || ck_pat_head(c, alt) != ct->sym
            || (!alt->u.cons.payload && ct->payload)) {
            continue;
        }
        one[0] = alt->u.cons.payload;
        ex_rows_add(c, out, ex_vec_at(c, row, ncols, pos,
                                ct->payload ? one : NULL,
                                ct->payload ? 1 : 0));
    }
}

/* The constructor branch of an algebraic column: each surviving row
   contributes the constructor's payload pattern, and the instantiated
   payload type replaces the column. */
int ex_ctor_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                       int depth, const ck_ctor *ct)
{
    const ck_decl *d = ck_decl_of(c, ct->owner);
    const ek_node *any[1];
    const ek_type *ins[1];
    ex_rows out;
    ex_buf buf;
    size_t i;

    memset(&out, 0, sizeof out);
    memset(&buf, 0, sizeof buf);
    ins[0] = ctor_payload(c, ct, st->cols[pos], d);
    for (i = 0; i < st->nrows && !c->oom; i++) {
        const ek_node *p = st->rows[i][pos];
        size_t nalt;

        if (!p) {
            ex_rows_wild(c, &out, st->rows[i], st->ncols, pos, any,
                         ct->payload ? 1 : 0);
            continue;
        }
        nalt = ex_flat_into(c, p, &buf);
        if (ex_any_catchall(buf.v, nalt)) {
            ex_rows_wild(c, &out, st->rows[i], st->ncols, pos, any,
                         ct->payload ? 1 : 0);
            continue;
        }
        ctor_alts(c, &out, st->rows[i], st->ncols, pos, ct, buf.v, nalt);
    }
    if (c->oom) {
        return 1;
    }
    if (!out.n) {
        return 0;
    }
    return ex_step(c, st, pos, memo, depth, ins, ct->payload ? 1 : 0, &out);
}

/* The component contributions of one alternative at a shape column:
   a catch-all and a matching brace answer every component; anything
   else cannot match the shape. */
static int shape_contrib(const ek_node *a, const ek_type *t,
                         const ek_node **out, size_t k)
{
    size_t i, j;

    if (ck_pat_catchall(a)) {
        for (i = 0; i < k; i++) {
            out[i] = NULL;
        }
        return 1;
    }
    if (a->kind != EK_N_P_BRACE || a->u.p_brace.rest != EK_REST_NONE) {
        return 0;
    }
    if (t->kind == EK_T_RECORD) {
        for (i = 0; i < k; i++) {
            const char *field = t->u.record.fields[i].name;
            const ek_node *sub = NULL;

            for (j = 0; j < a->u.p_brace.items.count; j++) {
                const ek_node *item = a->u.p_brace.items.data[j];

                if (item->kind == EK_N_P_FIELD
                    && strcmp(item->u.p_field.name, field) == 0) {
                    sub = item->u.p_field.pattern;
                } else if (item->kind == EK_N_P_VAR
                           && item->u.path.count == 1
                           && strcmp(item->u.path.data[0], field) == 0) {
                    sub = item;
                }
            }
            out[i] = sub;
        }
        return 1;
    }
    if (t->kind == EK_T_TUPLE && a->u.p_brace.items.count == k) {
        for (i = 0; i < k; i++) {
            out[i] = a->u.p_brace.items.data[i];
        }
        return 1;
    }
    return 0;
}

/* The component types of a record or tuple shape. */
static void shape_ins(const ek_type *shape, size_t k, const ek_type **ins)
{
    size_t j;

    for (j = 0; j < k; j++) {
        ins[j] = shape->kind == EK_T_RECORD ? shape->u.record.fields[j].type
                                            : shape->u.tuple.items[j];
    }
}

/* A tuple or record column: the components replace the column. */
int ex_shape_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                        int depth, const ek_type *shape)
{
    size_t k = shape->kind == EK_T_RECORD ? shape->u.record.count
                                          : shape->u.tuple.count;
    const ek_type **ins = ek_arena_alloc(c->arena, (k ? k : 1)
                                                * sizeof *ins);
    const ek_node **contrib = ek_arena_alloc(c->arena, (k ? k : 1)
                                                    * sizeof *contrib);
    ex_rows out;
    ex_buf buf;
    size_t i, j;

    memset(&out, 0, sizeof out);
    memset(&buf, 0, sizeof buf);
    if (!ins || !contrib) {
        c->oom = 1;
        return 1;
    }
    shape_ins(shape, k, ins);
    for (i = 0; i < st->nrows && !c->oom; i++) {
        const ek_node *p = st->rows[i][pos];
        size_t nalt;

        if (!p) {
            ex_rows_wild(c, &out, st->rows[i], st->ncols, pos, contrib, k);
            continue;
        }
        nalt = ex_flat_into(c, p, &buf);
        if (ex_any_catchall(buf.v, nalt)) {
            ex_rows_wild(c, &out, st->rows[i], st->ncols, pos, contrib, k);
            continue;
        }
        for (j = 0; j < nalt && !c->oom; j++) {
            if (shape_contrib(buf.v[j], shape, contrib, k)) {
                ex_rows_add(c, &out, ex_vec_at(c, st->rows[i], st->ncols, pos,
                                         contrib, k));
            }
        }
    }
    if (c->oom) {
        return 1;
    }
    if (!out.n) {
        return 0;
    }
    return ex_step(c, st, pos, memo, depth, ins, k, &out);
}
