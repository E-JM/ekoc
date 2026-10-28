#include <string.h>

#include "check/internal.h"

/* Exhaustiveness by column specialization. The scrutinee types are
   columns and the arms are rows of contributed patterns; the rows
   must cover every value combination. Each walk step consumes one
   column: an algebraic column splits over its constructors and takes
   the instantiated payload as the column's replacement, a tuple or
   record column expands into its component columns, a list column
   splits into the empty list and the head-and-tail columns of a
   non-empty one, and a scalar column splits over the literals its
   rows name plus everything else, which only a catch-all row
   answers. An or-pattern splices its alternatives into rows. A
   column every surviving row answers with a catch-all carries no
   constraint and is skipped, and so does a column whose type is
   still open. Visited states are memoized on the remaining columns
   and the surviving rows' remaining contributions. */

/* A soft walk-depth guard: pattern structure is finite, and a state
   this deep is pathological input. */
#define EXH_MAX_DEPTH 512

typedef struct {
    const ek_type **cols;
    size_t ncols;
    const ek_node ***rows; /* one contribution vector per row */
    size_t nrows;
} ex_state;

typedef struct {
    ex_state snap; /* the copied pointer arrays; vectors are shared */
    int covered;
} ex_entry;

typedef struct {
    ex_entry *entries;
    size_t count, cap;
} ex_memo;

/* A scratch buffer of flattened alternatives, grown to fit. */
typedef struct {
    const ek_node **v;
    size_t cap;
} ex_buf;

static int covers_walk(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                       int depth);

/* All alternatives a pattern contributes at one depth. */
static size_t flat_into(ck_ctx *c, const ek_node *p, ex_buf *b)
{
    size_t n = ck_pat_flatten(c, p, b->v, 0, b->cap);

    while (n == b->cap && !c->oom) {
        b->v = ck_grow(c, b->v, b->cap + 1, &b->cap, sizeof *b->v);
        n = ck_pat_flatten(c, p, b->v, 0, b->cap);
    }
    return n;
}

/* A state's identity past `pos`: the remaining columns and every
   row's remaining contributions, by pointer. */
static int state_same(const ex_state *a, const ex_state *b, size_t pos)
{
    size_t i, j;

    if (a->ncols != b->ncols || a->nrows != b->nrows) {
        return 0;
    }
    for (j = pos; j < a->ncols; j++) {
        if (a->cols[j] != b->cols[j]) {
            return 0;
        }
    }
    for (i = 0; i < a->nrows; i++) {
        for (j = pos; j < a->ncols; j++) {
            if (a->rows[i][j] != b->rows[i][j]) {
                return 0;
            }
        }
    }
    return 1;
}

static int memo_get(const ex_memo *m, const ex_state *s, size_t pos)
{
    size_t i;

    for (i = 0; i < m->count; i++) {
        if (state_same(&m->entries[i].snap, s, pos)) {
            return m->entries[i].covered;
        }
    }
    return -1;
}

/* The snapshot copies the two pointer arrays; the contribution
   vectors live in the arena and are shared by pointer. */
static void memo_put(ck_ctx *c, ex_memo *m, const ex_state *s, int covered)
{
    ex_entry *e;
    size_t j;

    if (c->oom) {
        return;
    }
    if (m->count == m->cap) {
        m->entries = ck_grow(c, m->entries, m->count + 1, &m->cap,
                             sizeof *m->entries);
        if (c->oom) {
            return;
        }
    }
    e = &m->entries[m->count++];
    e->covered = covered;
    e->snap.ncols = s->ncols;
    e->snap.nrows = s->nrows;
    e->snap.cols = ek_arena_alloc(c->arena, (s->ncols ? s->ncols : 1)
                                          * sizeof *e->snap.cols);
    e->snap.rows = ek_arena_alloc(c->arena, (s->nrows ? s->nrows : 1)
                                          * sizeof *e->snap.rows);
    if (!e->snap.cols || !e->snap.rows) {
        c->oom = 1;
        return;
    }
    for (j = 0; j < s->ncols; j++) {
        e->snap.cols[j] = s->cols[j];
    }
    for (j = 0; j < s->nrows; j++) {
        e->snap.rows[j] = s->rows[j];
    }
}

/* One row vector with the column at `pos` replaced by `nins`
   entries; nins 0 removes the column. */
static const ek_node **vec_at(ck_ctx *c, const ek_node **v, size_t ncols,
                              size_t pos, const ek_node *const *ins,
                              size_t nins)
{
    size_t n = ncols - 1 + nins;
    const ek_node **out = ek_arena_alloc(c->arena, (n ? n : 1)
                                                * sizeof *out);

    if (!out) {
        c->oom = 1;
        return NULL;
    }
    memcpy(out, v, pos * sizeof *out);
    if (nins) {
        memcpy(out + pos, ins, nins * sizeof *out);
    }
    memcpy(out + pos + nins, v + pos + 1, (ncols - pos - 1) * sizeof *out);
    return out;
}

static const ek_type **cols_at(ck_ctx *c, const ek_type **cols, size_t ncols,
                               size_t pos, const ek_type *const *ins,
                               size_t nins)
{
    size_t n = ncols - 1 + nins;
    const ek_type **out = ek_arena_alloc(c->arena, (n ? n : 1)
                                               * sizeof *out);

    if (!out) {
        c->oom = 1;
        return NULL;
    }
    memcpy(out, cols, pos * sizeof *out);
    if (nins) {
        memcpy(out + pos, ins, nins * sizeof *out);
    }
    memcpy(out + pos + nins, cols + pos + 1, (ncols - pos - 1) * sizeof *out);
    return out;
}

/* The derived rows of one branch. */
typedef struct {
    const ek_node ***v;
    size_t n, cap;
} ex_rows;

static void rows_add(ck_ctx *c, ex_rows *r, const ek_node **vec)
{
    if (c->oom) {
        return;
    }
    if (r->n == r->cap) {
        r->v = ck_grow(c, r->v, r->n + 1, &r->cap, sizeof *r->v);
        if (c->oom) {
            return;
        }
    }
    r->v[r->n++] = vec;
}

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

/* True when every surviving row answers the column with a
   catch-all — no contribution, or a variable or wildcard pattern: the
   column constrains nothing. */
static int all_catchall_at(const ex_state *st, size_t pos)
{
    size_t i;

    for (i = 0; i < st->nrows; i++) {
        if (st->rows[i][pos] && !ck_pat_catchall(st->rows[i][pos])) {
            return 0;
        }
    }
    return 1;
}

/* The constructor branch of an algebraic column: each surviving row
   contributes the constructor's payload pattern, and the instantiated
   payload type replaces the column. */
static int ctor_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
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
            rows_add(c, &out, vec_at(c, st->rows[i], st->ncols, pos,
                                     ct->payload ? any : NULL,
                                     ct->payload ? 1 : 0));
            continue;
        }
        nalt = flat_into(c, p, &buf);
        for (na = 0; na < nalt; na++) {
            if (ck_pat_catchall(buf.v[na])) {
                all = 1;
            }
        }
        if (all) {
            const ek_node *any[1];

            any[0] = NULL;
            rows_add(c, &out, vec_at(c, st->rows[i], st->ncols, pos,
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
            rows_add(c, &out, vec_at(c, st->rows[i], st->ncols, pos,
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

        next.cols = cols_at(c, st->cols, st->ncols, pos,
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
static int shape_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
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
            rows_add(c, &out, vec_at(c, st->rows[i], st->ncols, pos, contrib,
                                     k));
            continue;
        }
        nalt = flat_into(c, p, &buf);
        for (na = 0; na < nalt; na++) {
            if (ck_pat_catchall(buf.v[na])) {
                all = 1;
            }
        }
        if (all) {
            for (j = 0; j < k; j++) {
                contrib[j] = NULL;
            }
            rows_add(c, &out, vec_at(c, st->rows[i], st->ncols, pos, contrib,
                                     k));
            continue;
        }
        for (na = 0; na < nalt && !c->oom; na++) {
            if (shape_contrib(buf.v[na], shape, contrib, k)) {
                rows_add(c, &out, vec_at(c, st->rows[i], st->ncols, pos,
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

        next.cols = cols_at(c, st->cols, st->ncols, pos, ins, k);
        next.ncols = st->ncols - 1 + k;
        next.rows = out.v;
        next.nrows = out.n;
        if (c->oom) {
            return 1;
        }
        return covers_walk(c, &next, pos, memo, depth + 1);
    }
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
static int list_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
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
            rows_add(c, &empty, vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                       0));
            rows_add(c, &cons, vec_at(c, st->rows[i], st->ncols, pos, any,
                                      2));
            continue;
        }
        nalt = flat_into(c, p, &buf);
        for (na = 0; na < nalt; na++) {
            if (ck_pat_catchall(buf.v[na])) {
                all = 1;
            }
        }
        if (all) {
            any[0] = NULL;
            any[1] = NULL;
            rows_add(c, &empty, vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                       0));
            rows_add(c, &cons, vec_at(c, st->rows[i], st->ncols, pos, any,
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
            rows_add(c, &empty, vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                       0));
        }
        for (na = 0; na < nalt && !c->oom; na++) {
            if (cons_contrib(c, buf.v[na], elem, contrib)) {
                rows_add(c, &cons, vec_at(c, st->rows[i], st->ncols, pos,
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

        next.cols = cols_at(c, st->cols, st->ncols, pos, NULL, 0);
        next.ncols = st->ncols - 1;
        next.rows = empty.v;
        next.nrows = empty.n;
        covered = covers_walk(c, &next, pos, memo, depth + 1);
    }
    if (covered) {
        ex_state next;

        next.cols = cols_at(c, st->cols, st->ncols, pos, ins, 2);
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
static int scalar_branch(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
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
            rows_add(c, &rest, vec_at(c, st->rows[i], st->ncols, pos, NULL,
                                      0));
            continue;
        }
        {
            size_t nalt = flat_into(c, p, &buf);

            for (k = 0; k < nalt; k++) {
                if (ck_pat_catchall(buf.v[k])) {
                    rows_add(c, &rest, vec_at(c, st->rows[i], st->ncols, pos,
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
                size_t nalt = flat_into(c, p, &buf);

                for (q = 0; q < nalt; q++) {
                    if (ck_pat_catchall(buf.v[q])
                        || lit_same(buf.v[q], lits[i])) {
                        hit = 1;
                    }
                }
            }
            if (hit) {
                rows_add(c, &lit, vec_at(c, st->rows[k], st->ncols, pos, NULL,
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

            next.cols = cols_at(c, st->cols, st->ncols, pos, NULL, 0);
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

            next.cols = cols_at(c, st->cols, st->ncols, pos, NULL, 0);
            next.ncols = st->ncols - 1;
            next.rows = rest.v;
            next.nrows = rest.n;
            covered = covers_walk(c, &next, pos, memo, depth + 1);
        }
    }
    return covered;
}

/* One walk step at column `pos`. */
static int covers_walk(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
                       int depth)
{
    const ek_type *t;
    const ck_decl *d;
    int hit, covered = 1;

    if (c->oom) {
        return 1;
    }
    if (!st->nrows) {
        return 0;
    }
    while (pos < st->ncols && all_catchall_at(st, pos)) {
        pos++;
    }
    if (pos >= st->ncols) {
        return 1;
    }
    if (depth > EXH_MAX_DEPTH) {
        return 0;
    }
    hit = memo_get(memo, st, pos);
    if (hit >= 0) {
        return hit;
    }
    t = ck_prune(st->cols[pos]);
    if (!t || t->kind == EK_T_IVAR || t->kind == EK_T_TVAR) {
        return 1; /* the bridge or a rigid variable owes the column */
    }
    switch (t->kind) {
    case EK_T_FN:
    case EK_T_DICT:
    case EK_T_UNIT:
    case EK_T_NEVER:
        /* No pattern form but a catch-all matches these. */
        if (!all_catchall_at(st, pos)) {
            covered = 0;
            break;
        }
        covered = covers_walk(c, st, pos + 1, memo, depth + 1);
        break;
    case EK_T_LIST:
        covered = list_branch(c, st, pos, memo, depth);
        break;
    case EK_T_TUPLE:
    case EK_T_RECORD:
        covered = shape_branch(c, st, pos, memo, depth, t);
        break;
    case EK_T_NOMINAL: {
        const ek_type *repr;

        if (!t->u.nominal.sym) {
            covered = scalar_branch(c, st, pos, memo, depth);
            break;
        }
        d = ck_decl_of(c, t->u.nominal.sym);
        if (d && d->rhs == EK_TYPE_RHS_VARIANTS && d->nctors) {
            size_t k;

            for (k = 0; k < d->nctors && covered; k++) {
                covered = ctor_branch(c, st, pos, memo, depth, d->ctors[k]);
            }
        } else if (d && (d->rhs == EK_TYPE_RHS_RECORD
                         || d->rhs == EK_TYPE_RHS_TUPLE)
                   && (repr = ck_decl_repr(c, t)) && repr != t) {
            st->cols[pos] = repr;
            covered = covers_walk(c, st, pos, memo, depth + 1);
        } else {
            covered = scalar_branch(c, st, pos, memo, depth);
        }
        break;
    }
    default:
        covered = scalar_branch(c, st, pos, memo, depth);
        break;
    }
    memo_put(c, memo, st, covered);
    return covered;
}

/* The contributing arms. A guarded arm contributes no coverage — its
   pattern may match and its guard still fall through — so it never
   joins the rows; the flag only drives the final-arm rule. */
static size_t live_arms(const ek_node *const *arms, const int *contrib,
                        size_t narms, const ek_node **live, int *guarded)
{
    size_t i, nlive = 0;

    for (i = 0; i < narms; i++) {
        if (arms[i]->u.arm.guard) {
            *guarded = 1;
            continue;
        }
        if (contrib[i]) {
            live[nlive++] = arms[i];
        }
    }
    return nlive;
}

void ck_exhaust(ck_ctx *c, const ek_node *cs, const ek_type **scrut,
                size_t nscrut, const ek_node *const *arms, const int *contrib,
                size_t narms)
{
    const ek_node **live;
    const ek_node ***rows;
    ex_state st;
    ex_memo memo;
    int guarded = 0, covered;
    size_t i, j, nlive;

    if (c->oom) {
        return;
    }
    if (narms == 0) {
        ck_error(c, cs, "E-exhaust", "the case has no arm (Clause 7.7.2)");
        return;
    }
    live = ek_arena_alloc(c->arena, narms * sizeof *live);
    if (!live) {
        c->oom = 1;
        return;
    }
    nlive = live_arms(arms, contrib, narms, live, &guarded);
    if (guarded && arms[narms - 1]->u.arm.guard) {
        ck_error(c, cs, "E-exhaust",
                 "a case with a guarded arm ends with an unguarded arm "
                 "(Clause 7.7.3)");
        return;
    }
    if (!nlive) {
        return;
    }
    rows = ek_arena_alloc(c->arena, nlive * sizeof *rows);
    if (!rows) {
        c->oom = 1;
        return;
    }
    for (i = 0; i < nlive && !c->oom; i++) {
        const ek_nlist *pats = &live[i]->u.arm.patterns;

        rows[i] = ek_arena_alloc(c->arena, nscrut * sizeof **rows);
        if (!rows[i]) {
            c->oom = 1;
            return;
        }
        for (j = 0; j < nscrut; j++) {
            rows[i][j] = j < pats->count ? pats->data[j] : NULL;
        }
    }
    memset(&memo, 0, sizeof memo);
    st.cols = (const ek_type **)scrut;
    st.ncols = nscrut;
    st.rows = rows;
    st.nrows = nlive;
    covered = covers_walk(c, &st, 0, &memo, 0);
    if (!covered) {
        ck_error(c, cs, "E-exhaust",
                 "the case is not exhaustive (Clause 7.7.2)");
    }
}
