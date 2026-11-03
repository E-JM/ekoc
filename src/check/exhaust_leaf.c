#include <string.h>

#include "check/internal.h"

/* The column branches of the exhaustiveness walk that split over
   leaves: the empty and head-and-rest branches of a list column and
   the literal split of a scalar column. */

/* Whether two literal patterns name the same value. Two spellings of
   one integer count as two literals: the split only grows, and a
   wider split never accepts a combination the rows leave open. */
static int lit_same(const ek_node *a, const ek_node *b)
{
    size_t i;

    if (!a || !b || a->kind != b->kind) {
        return 0;
    }
    switch (a->kind) {
    case EK_N_P_LIT:
    case EK_N_UNARY:
        return lit_same(a->u.wrap.inner, b->u.wrap.inner);
    case EK_N_INT_LIT:
        return strcmp(a->u.int_lit.text, b->u.int_lit.text) == 0;
    case EK_N_FLOAT_LIT:
        return a->u.float_lit.value == b->u.float_lit.value;
    case EK_N_CHAR_LIT:
        return a->u.char_lit.scalar == b->u.char_lit.scalar;
    case EK_N_STRING_LIT:
        if (a->u.str.count != b->u.str.count) {
            return 0;
        }
        for (i = 0; i < a->u.str.count; i++) {
            if (a->u.str.segs[i].expr || b->u.str.segs[i].expr
                || a->u.str.segs[i].text_len != b->u.str.segs[i].text_len
                || memcmp(a->u.str.segs[i].text, b->u.str.segs[i].text,
                          a->u.str.segs[i].text_len) != 0) {
                return 0;
            }
        }
        return 1;
    default:
        return 0;
    }
}

/* Does one of pats cover every value of t? The engine over one
   column. This is the prefix-rest gate and the public judgment the
   rest of the checker reaches for. */
int ck_covers(ck_ctx *c, const ek_type *t, const ek_node *const *pats,
              size_t n)
{
    const ek_type *cols[1];
    const ek_node **rows;
    ex_state st;
    ex_memo memo;
    size_t i;

    rows = ek_arena_alloc(c->arena, (n ? n : 1) * sizeof *rows);
    if (!rows) {
        c->oom = 1;
        return 1;
    }
    for (i = 0; i < n; i++) {
        rows[i] = pats[i];
    }
    cols[0] = t;
    st.cols = cols;
    st.ncols = 1;
    st.rows = &rows;
    st.nrows = n;
    memset(&memo, 0, sizeof memo);
    return covers_walk(c, &st, 0, &memo, 0);
}

/* One list-pattern alternative's contributions to the head and tail
   columns of the non-empty branch. Returns 0 when the alternative
   cannot match a non-empty list. */
static int cons_contrib(ck_ctx *c, const ek_node *a, const ek_type *elem,
                        const ek_node **out)
{
    size_t k;

    out[0] = NULL;
    out[1] = NULL;
    if (ck_pat_catchall(a)) {
        return 1;
    }
    if (a->kind != EK_N_P_BRACE) {
        return 0;
    }
    if (a->u.p_brace.rest == EK_REST_SUFFIX) {
        /* The head owes its own coverage; the rest always matches. */
        out[0] = a->u.p_brace.items.count ? a->u.p_brace.items.data[0]
                                          : NULL;
        return 1;
    }
    if (a->u.p_brace.rest == EK_REST_PREFIX) {
        const ek_node *one;

        k = a->u.p_brace.items.count;
        if (!k) {
            return 0;
        }
        one = a->u.p_brace.items.data[k - 1];
        if (!ck_covers(c, elem, &one, 1)) {
            return 0;
        }
        return 1;
    }
    if (a->u.p_brace.rest == EK_REST_NONE && a->u.p_brace.items.count) {
        /* A fixed length covers the head and leaves the shorter
           fixed pattern for the tail. */
        ek_node *kept = ek_arena_alloc(c->arena, sizeof *kept);

        if (!kept) {
            c->oom = 1;
            return 1;
        }
        memset(kept, 0, sizeof *kept);
        kept->kind = EK_N_P_BRACE;
        kept->u.p_brace.rest = EK_REST_NONE;
        kept->u.p_brace.items.count = a->u.p_brace.items.count - 1;
        kept->u.p_brace.items.data = a->u.p_brace.items.data + 1;
        kept->line = a->line;
        kept->col = a->col;
        out[0] = a->u.p_brace.items.data[0];
        out[1] = kept;
        return 1;
    }
    return 0; /* the exact empty list matches no non-empty value */
}

/* A list column: the empty branch drops the column, the non-empty
   branch takes the element and the tail as two columns. */
int ex_list_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                       int depth)
{
    const ek_type *elem = st->cols[pos]->u.list.elem;
    const ek_type *ins[2];
    const ek_node **contrib = ek_arena_alloc(c->arena, 2 * sizeof *contrib);
    ex_rows empty, cons;
    ex_buf buf;
    size_t i;
    int covered;

    memset(&empty, 0, sizeof empty);
    memset(&cons, 0, sizeof cons);
    memset(&buf, 0, sizeof buf);
    if (!contrib) {
        c->oom = 1;
        return 1;
    }
    ins[0] = elem;
    ins[1] = st->cols[pos];
    for (i = 0; i < st->nrows && !c->oom; i++) {
        const ek_node *p = st->rows[i][pos];
        const ek_node *any[2];
        size_t nalt, na;
        int hit_empty = 0, all = 0;

        if (!p) {
            any[0] = NULL;
            any[1] = NULL;
            ex_rows_add(c, &empty, ex_vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                       0));
            ex_rows_add(c, &cons, ex_vec_at(c, st->rows[i], st->ncols, pos, any,
                                      2));
            continue;
        }
        nalt = ex_flat_into(c, p, &buf);
        for (na = 0; na < nalt; na++) {
            if (ck_pat_catchall(buf.v[na])) {
                all = 1;
            }
        }
        if (all) {
            any[0] = NULL;
            any[1] = NULL;
            ex_rows_add(c, &empty, ex_vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                       0));
            ex_rows_add(c, &cons, ex_vec_at(c, st->rows[i], st->ncols, pos, any,
                                      2));
            continue;
        }
        /* Every alternative that can match a branch contributes its
           own row: alternatives of one cell are independent of the
           other cells. */
        for (na = 0; na < nalt; na++) {
            const ek_node *a = buf.v[na];

            if (a->kind == EK_N_P_BRACE
                && a->u.p_brace.rest == EK_REST_NONE
                && a->u.p_brace.items.count == 0) {
                hit_empty = 1;
            }
        }
        if (hit_empty) {
            ex_rows_add(c, &empty, ex_vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                       0));
        }
        for (na = 0; na < nalt && !c->oom; na++) {
            if (cons_contrib(c, buf.v[na], elem, contrib)) {
                ex_rows_add(c, &cons, ex_vec_at(c, st->rows[i], st->ncols, pos,
                                          contrib, 2));
            }
        }
    }
    if (c->oom) {
        return 1;
    }
    if (!empty.n || !cons.n) {
        return 0;
    }
    {
        ex_state next;

        next.cols = ex_cols_at(c, st->cols, st->ncols, pos, NULL, 0);
        next.ncols = st->ncols - 1;
        next.rows = empty.v;
        next.nrows = empty.n;
        covered = covers_walk(c, &next, pos, memo, depth + 1);
    }
    if (covered) {
        ex_state next;

        next.cols = ex_cols_at(c, st->cols, st->ncols, pos, ins, 2);
        next.ncols = st->ncols + 1;
        next.rows = cons.v;
        next.nrows = cons.n;
        if (c->oom) {
            return 1;
        }
        covered = covers_walk(c, &next, pos, memo, depth + 1);
    }
    return covered;
}

/* A scalar column: each named literal is one value, everything else
   wants a catch-all row. */
int ex_scalar_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                         int depth)
{
    ex_rows rest, lit;
    ex_buf buf;
    const ek_node **lits = NULL;
    size_t nlits = 0, litcap = 0, i, k, q;
    int covered = 1;

    memset(&rest, 0, sizeof rest);
    memset(&buf, 0, sizeof buf);
    for (i = 0; i < st->nrows && !c->oom; i++) {
        const ek_node *p = st->rows[i][pos];

        if (!p) {
            ex_rows_add(c, &rest, ex_vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                      0));
            continue;
        }
        {
            size_t nalt = ex_flat_into(c, p, &buf);

            for (k = 0; k < nalt; k++) {
                if (ck_pat_catchall(buf.v[k])) {
                    ex_rows_add(c, &rest, ex_vec_at(c, st->rows[i], st->ncols, pos,
                                              NULL, 0));
                } else if (buf.v[k]->kind == EK_N_P_LIT) {
                    int seen = 0;

                    for (q = 0; q < nlits; q++) {
                        if (lit_same(lits[q], buf.v[k])) {
                            seen = 1;
                        }
                    }
                    if (!seen) {
                        lits = ck_grow(c, lits, nlits + 1, &litcap,
                                       sizeof *lits);
                        if (c->oom) {
                            return 1;
                        }
                        lits[nlits++] = buf.v[k];
                    }
                }
            }
        }
    }
    if (c->oom) {
        return 1;
    }
    for (i = 0; i < nlits && covered; i++) {
        memset(&lit, 0, sizeof lit);
        for (k = 0; k < st->nrows && !c->oom; k++) {
            const ek_node *p = st->rows[k][pos];
            int hit = 0;

            if (!p) {
                hit = 1;
            } else {
                size_t nalt = ex_flat_into(c, p, &buf);

                for (q = 0; q < nalt; q++) {
                    if (ck_pat_catchall(buf.v[q])
                        || lit_same(buf.v[q], lits[i])) {
                        hit = 1;
                    }
                }
            }
            if (hit) {
                ex_rows_add(c, &lit, ex_vec_at(c, st->rows[k], st->ncols, pos, NULL,
                                         0));
            }
        }
        if (c->oom) {
            return 1;
        }
        if (!lit.n) {
            return 0;
        }
        {
            ex_state next;

            next.cols = ex_cols_at(c, st->cols, st->ncols, pos, NULL, 0);
            next.ncols = st->ncols - 1;
            next.rows = lit.v;
            next.nrows = lit.n;
            covered = covers_walk(c, &next, pos, memo, depth + 1);
        }
    }
    if (covered) {
        if (!rest.n) {
            return 0;
        }
        {
            ex_state next;

            next.cols = ex_cols_at(c, st->cols, st->ncols, pos, NULL, 0);
            next.ncols = st->ncols - 1;
            next.rows = rest.v;
            next.nrows = rest.n;
            covered = covers_walk(c, &next, pos, memo, depth + 1);
        }
    }
    return covered;
}
