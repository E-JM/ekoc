#include <string.h>

#include "check/internal.h"

/* Multi-scrutinee exhaustiveness and the ck_exhaust entry. The
   product of the scrutinee types: an arm covers a combination when
   each column covers its scrutinee, and the arms must cover every
   combination. Columns whose type has no internally visible case
   structure defer; a catch-all covers every combination. The judgment
   descends column by column over the rows each constructor keeps; its
   (column, surviving rows) states are memoized, so a wide case over
   small ADTs costs work proportional to its arms, not to their
   product. */

/* The live rows of one judgment as a bitmap (at most 128 arms). */
typedef struct {
    unsigned long long lo, hi;
} row_set;

static row_set set_add(row_set s, size_t i)
{
    if (i < 64) {
        s.lo |= 1ull << i;
    } else {
        s.hi |= 1ull << (i - 64);
    }
    return s;
}

/* One memoized judgment: the column plus the surviving rows. */
typedef struct {
    size_t col;
    row_set rows;
    int covered;
    unsigned used;
} prod_memo;

typedef struct {
    prod_memo *entries;
    size_t cap, count;
} prod_table;

static size_t memo_hash(size_t col, row_set s)
{
    unsigned long long h = s.lo;

    h = (h ^ (s.hi * 0x9e3779b97f4a7c15ull)) * 0xff51afd7ed558ccdull;
    h = (h ^ ((unsigned long long)col + 0x165667b19e3779f9ull))
        * 0xc4ceb9fe1a85ec53ull;
    return (size_t)(h ^ (h >> 33));
}

static int memo_get(const prod_table *t, size_t col, row_set s)
{
    size_t j;

    if (!t->cap) {
        return -1;
    }
    for (j = memo_hash(col, s) & (t->cap - 1); t->entries[j].used;
         j = (j + 1) & (t->cap - 1)) {
        if (t->entries[j].col == col && t->entries[j].rows.lo == s.lo
            && t->entries[j].rows.hi == s.hi) {
            return t->entries[j].covered;
        }
    }
    return -1;
}

static void memo_put(ck_ctx *c, prod_table *t, size_t col, row_set s,
                     int covered)
{
    size_t j;

    if (t->count * 2 >= t->cap) {
        size_t ncap = t->cap ? t->cap * 4 : 64;
        prod_memo *ne = ek_arena_alloc(c->arena, ncap * sizeof *ne);
        size_t i;

        if (!ne) {
            c->oom = 1;
            return;
        }
        memset(ne, 0, ncap * sizeof *ne);
        for (i = 0; i < t->cap; i++) {
            if (t->entries[i].used) {
                size_t k = memo_hash(t->entries[i].col, t->entries[i].rows)
                           & (ncap - 1);

                while (ne[k].used) {
                    k = (k + 1) & (ncap - 1);
                }
                ne[k] = t->entries[i];
            }
        }
        t->entries = ne;
        t->cap = ncap;
    }
    for (j = memo_hash(col, s) & (t->cap - 1); t->entries[j].used;
         j = (j + 1) & (t->cap - 1)) {
        if (t->entries[j].col == col && t->entries[j].rows.lo == s.lo
            && t->entries[j].rows.hi == s.hi) {
            return;
        }
    }
    t->entries[j].col = col;
    t->entries[j].rows = s;
    t->entries[j].covered = covered;
    t->entries[j].used = 1;
    t->count++;
}

/* The arms of rows whose column pattern matches one constructor of
   column `col`, or is a catch-all there: their nodes, their indices,
   and their bitmap. */
static size_t rows_for(ck_ctx *c, const ek_node *const *rows, size_t nr,
                       size_t col, const ck_ctor *ct, const ek_node **out,
                       row_set *keep)
{
    size_t i, nout = 0;

    for (i = 0; i < nr; i++) {
        const ek_node *pat = col < rows[i]->u.arm.patterns.count
                                 ? rows[i]->u.arm.patterns.data[col] : NULL;
        const ek_node *flat[32];
        size_t nf, q;
        int hit = 0;

        if (ck_pat_catchall(pat)) {
            hit = 1;
        } else {
            nf = ck_pat_flatten(c, pat, flat, 0, 32);
            for (q = 0; q < nf && q < 32; q++) {
                if (ck_pat_head(c, flat[q]) == ct->sym) {
                    hit = 1;
                }
            }
        }
        if (hit) {
            out[nout++] = rows[i];
            *keep = set_add(*keep, i);
        }
    }
    return nout;
}

static int covers_prod(ck_ctx *c, prod_table *memo,
                       const ek_node *const *live, size_t nr,
                       const ek_type **scrut, size_t ns, row_set rows,
                       size_t col)
{
    const ek_type *s;
    const ck_decl *d;
    const ek_node *sub[128];
    size_t k;
    int all = 1;
    int hit = memo_get(memo, col, rows);

    if (hit >= 0) {
        return hit;
    }
    s = ck_prune(scrut[col]);
    if (!s || s->kind != EK_T_NOMINAL || !s->u.nominal.sym) {
        return 1; /* the column defers */
    }
    d = ck_decl_of(c, s->u.nominal.sym);
    if (!d || d->rhs != EK_TYPE_RHS_VARIANTS || !d->nctors) {
        return 1;
    }
    for (k = 0; k < d->nctors && all; k++) {
        row_set keep = {0, 0};
        size_t nsub = rows_for(c, live, nr, col, d->ctors[k], sub, &keep);

        if (nsub == 0) {
            all = 0;
            break;
        }
        if (col + 1 < ns) {
            all = covers_prod(c, memo, sub, nsub, scrut, ns, keep, col + 1);
        }
    }
    memo_put(c, memo, col, rows, all);
    return all;
}

/* The contributing arms; a guarded arm contributes no coverage.
   Returns the live count and sets guarded. */
static size_t live_arms(const ek_node *const *arms, const int *contrib,
                        size_t narms, const ek_node **live, int *guarded)
{
    size_t i, nlive = 0;

    for (i = 0; i < narms; i++) {
        if (arms[i]->u.arm.guard) {
            *guarded = 1;
        }
        if (contrib[i] && nlive < 128) {
            live[nlive++] = arms[i];
        }
    }
    return nlive;
}

void ck_exhaust(ck_ctx *c, const ek_node *cs, const ek_type **scrut,
                size_t nscrut, const ek_node *const *arms, const int *contrib,
                size_t narms)
{
    const ek_node *live[128];
    const ek_node *pats[128];
    int guarded = 0, covered;
    size_t i, nlive;

    if (c->oom) {
        return;
    }
    if (narms == 0) {
        ck_error(c, cs, "E-exhaust", "the case has no arm (Clause 7.7.2)");
        return;
    }
    nlive = live_arms(arms, contrib, narms, live, &guarded);
    if (guarded && arms[narms - 1]->u.arm.guard) {
        ck_error(c, cs, "E-exhaust",
                 "a case with a guarded arm ends with an unguarded arm "
                 "(Clause 7.7.3)");
        return;
    }
    if (nlive == 0) {
        /* Unreachable beyond allocation failure: every arm carries a
           pattern, so the arms answer below. */
        return;
    }
    if (nscrut == 1) {
        for (i = 0; i < nlive; i++) {
            pats[i] = live[i]->u.arm.patterns.count
                          ? live[i]->u.arm.patterns.data[0] : NULL;
        }
        covered = ck_covers(c, scrut[0], pats, nlive);
    } else {
        prod_table memo;
        row_set full = {0, 0};

        memset(&memo, 0, sizeof memo);
        for (i = 0; i < nlive; i++) {
            full = set_add(full, i);
        }
        covered = covers_prod(c, &memo, live, nlive, scrut, nscrut, full, 0);
    }
    if (!covered) {
        ck_error(c, cs, "E-exhaust",
                 "the case is not exhaustive (Clause 7.7.2)");
    }
}
