#include <string.h>

#include "check/internal.h"

/* The column branches of the exhaustiveness walk that split over
   structure: the constructor branch of an algebraic column and the
   component branch of a tuple or record column. */

/* The constructor branch of an algebraic column: each surviving row
   contributes the constructor's payload pattern, and the instantiated
   payload type replaces the column. */
int ex_ctor_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                       int depth, const ck_ctor *ct)
{
    const ck_decl *d = ck_decl_of(c, ct->owner);
    const ek_type *payload = NULL;
    const ek_type *ins[1];
    ex_rows out;
    ex_buf buf;
    size_t i, nb = d ? d->nbinders : 0;

    memset(&out, 0, sizeof out);
    memset(&buf, 0, sizeof buf);
    if (ct->payload) {
        if (nb && st->cols[pos]->kind == EK_T_NOMINAL
            && st->cols[pos]->u.nominal.argc == nb) {
            payload = ck_apply_binders(c, ct->payload, d->binders,
                                       st->cols[pos]->u.nominal.args, nb);
        } else {
            payload = ct->payload;
        }
        ins[0] = payload;
    }
    for (i = 0; i < st->nrows && !c->oom; i++) {
        const ek_node *p = st->rows[i][pos];
        size_t nalt, na;
        int all = 0;

        if (!p) {
            const ek_node *any[1];

            any[0] = NULL;
            ex_rows_add(c, &out, ex_vec_at(c, st->rows[i], st->ncols, pos,
                                     ct->payload ? any : NULL,
                                     ct->payload ? 1 : 0));
            continue;
        }
        nalt = ex_flat_into(c, p, &buf);
        for (na = 0; na < nalt; na++) {
            if (ck_pat_catchall(buf.v[na])) {
                all = 1;
            }
        }
        if (all) {
            const ek_node *any[1];

            any[0] = NULL;
            ex_rows_add(c, &out, ex_vec_at(c, st->rows[i], st->ncols, pos,
                                     ct->payload ? any : NULL,
                                     ct->payload ? 1 : 0));
            continue;
        }
        for (na = 0; na < nalt && !c->oom; na++) {
            const ek_node *alt = buf.v[na];
            const ek_node *one[1];

            /* A bare arm over a payload-carrying constructor matches
               nothing. */
            if (alt->kind != EK_N_P_CONS || ck_pat_head(c, alt) != ct->sym
                || (!alt->u.cons.payload && ct->payload)) {
                continue;
            }
            one[0] = alt->u.cons.payload;
            ex_rows_add(c, &out, ex_vec_at(c, st->rows[i], st->ncols, pos,
                                     ct->payload ? one : NULL,
                                     ct->payload ? 1 : 0));
        }
    }
    if (c->oom) {
        return 1;
    }
    if (!out.n) {
        return 0;
    }
    {
        ex_state next;

        next.cols = ex_cols_at(c, st->cols, st->ncols, pos,
                            ct->payload ? ins : NULL, ct->payload ? 1 : 0);
        next.ncols = st->ncols - 1 + (ct->payload ? 1 : 0);
        next.rows = out.v;
        next.nrows = out.n;
        if (c->oom) {
            return 1;
        }
        return covers_walk(c, &next, pos, memo, depth + 1);
    }
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
    for (j = 0; j < k; j++) {
        ins[j] = shape->kind == EK_T_RECORD ? shape->u.record.fields[j].type
                                            : shape->u.tuple.items[j];
    }
    for (i = 0; i < st->nrows && !c->oom; i++) {
        const ek_node *p = st->rows[i][pos];
        size_t nalt, na;
        int all = 0;

        if (!p) {
            for (j = 0; j < k; j++) {
                contrib[j] = NULL;
            }
            ex_rows_add(c, &out, ex_vec_at(c, st->rows[i], st->ncols, pos, contrib,
                                     k));
            continue;
        }
        nalt = ex_flat_into(c, p, &buf);
        for (na = 0; na < nalt; na++) {
            if (ck_pat_catchall(buf.v[na])) {
                all = 1;
            }
        }
        if (all) {
            for (j = 0; j < k; j++) {
                contrib[j] = NULL;
            }
            ex_rows_add(c, &out, ex_vec_at(c, st->rows[i], st->ncols, pos, contrib,
                                     k));
            continue;
        }
        for (na = 0; na < nalt && !c->oom; na++) {
            if (shape_contrib(buf.v[na], shape, contrib, k)) {
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
    {
        ex_state next;

        next.cols = ex_cols_at(c, st->cols, st->ncols, pos, ins, k);
        next.ncols = st->ncols - 1 + k;
        next.rows = out.v;
        next.nrows = out.n;
        if (c->oom) {
            return 1;
        }
        return covers_walk(c, &next, pos, memo, depth + 1);
    }
}
