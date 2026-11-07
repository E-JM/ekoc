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

/* All alternatives a pattern contributes at one depth. */
size_t ex_flat_into(ck_ctx *c, const ek_node *p, ex_buf *b)
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
const ek_node **ex_vec_at(ck_ctx *c, const ek_node **v, size_t ncols,
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

const ek_type **ex_cols_at(ck_ctx *c, const ek_type **cols, size_t ncols,
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

void ex_rows_add(ck_ctx *c, ex_rows *r, const ek_node **vec)
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

/* One surviving row answered with k wildcard contributions: the
   branch's rows gain the row with the column replaced. */
void ex_rows_wild(ck_ctx *c, ex_rows *r, const ek_node **row,
                  size_t ncols, size_t pos, const ek_node **wild, size_t k)
{
    size_t j;

    for (j = 0; j < k; j++) {
        wild[j] = NULL;
    }
    ex_rows_add(c, r, ex_vec_at(c, row, ncols, pos, k ? wild : NULL, k));
}

/* One branch's child state: the surviving rows with the column
   replaced by `nins` inserted columns, walked at the same position. */
int ex_step(ck_ctx *c, const ex_state *st, size_t pos, ex_memo *memo,
            int depth, const ek_type *const *ins, size_t nins,
            const ex_rows *rows)
{
    ex_state next;

    next.cols = ex_cols_at(c, st->cols, st->ncols, pos, ins, nins);
    next.ncols = st->ncols - 1 + nins;
    next.rows = rows->v;
    next.nrows = rows->n;
    return covers_walk(c, &next, pos, memo, depth + 1);
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

/* One walk step at column `pos`. */
int covers_walk(ck_ctx *c, ex_state *st, size_t pos, ex_memo *memo,
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
        covered = ex_list_branch(c, st, pos, memo, depth);
        break;
    case EK_T_TUPLE:
    case EK_T_RECORD:
        covered = ex_shape_branch(c, st, pos, memo, depth, t);
        break;
    case EK_T_NOMINAL: {
        const ek_type *repr;

        if (!t->u.nominal.sym) {
            covered = ex_scalar_branch(c, st, pos, memo, depth);
            break;
        }
        d = ck_decl_of(c, t->u.nominal.sym);
        if (d && d->rhs == EK_TYPE_RHS_VARIANTS && d->nctors) {
            size_t k;

            for (k = 0; k < d->nctors && covered; k++) {
                covered = ex_ctor_branch(c, st, pos, memo, depth, d->ctors[k]);
            }
        } else if (d && (d->rhs == EK_TYPE_RHS_RECORD
                         || d->rhs == EK_TYPE_RHS_TUPLE)
                   && (repr = ck_decl_repr(c, t)) && repr != t) {
            st->cols[pos] = repr;
            covered = covers_walk(c, st, pos, memo, depth + 1);
        } else {
            covered = ex_scalar_branch(c, st, pos, memo, depth);
        }
        break;
    }
    default:
        covered = ex_scalar_branch(c, st, pos, memo, depth);
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
