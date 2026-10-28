#include <string.h>

#include "check/internal.h"

/* Multi-scrutinee exhaustiveness and the ck_exhaust entry. The
   product of the scrutinee types: an arm covers a combination when
   each column covers its scrutinee, and the arms must cover every
   combination. An ADT column splits over its constructors; each split
   owes a payload obligation, judged over the rows that survive every
   further split, so a constructor's payload is covered exactly where
   the combination that reaches it is covered. A scalar column splits
   over the literals its rows name plus everything else, which only a
   catch-all row answers. A column of any other kind — a shape column,
   or one whose type is still open — ends the split: the obligations
   collected so far are judged and the rest suspends. The (column,
   surviving rows, obligations) states are memoized, so a wide case
   over small ADTs costs work proportional to its arms, not to their
   product. */

/* The live rows of one judgment as a bitmap (at most 128 arms). */
typedef struct {
    unsigned long long lo, hi;
} row_set;

/* One constructor payload owed by the path: the constructor chosen at
   `col`, over the scrutinee type of that column. */
typedef struct {
    const ck_ctor *ct;
    const ek_type *scrut;
    size_t col;
} prod_pend;

#define PROD_MAX_PEND 16

static row_set set_add(row_set s, size_t i)
{
    if (i < 64) {
        s.lo |= 1ull << i;
    } else {
        s.hi |= 1ull << (i - 64);
    }
    return s;
}

/* One memoized judgment: the column, the surviving rows, and the
   payload obligations on the path. */
typedef struct {
    size_t col;
    row_set rows;
    unsigned long long sig;
    int covered;
    unsigned used;
} prod_memo;

typedef struct {
    prod_memo *entries;
    size_t cap, count;
} prod_table;

static unsigned long long pend_sig(const prod_pend *pd, size_t np)
{
    unsigned long long h = 0x9e3779b97f4a7c15ull;
    size_t i;

    for (i = 0; i < np; i++) {
        h ^= ((unsigned long long)(uintptr_t)pd[i].ct->sym + pd[i].col)
             * 0xff51afd7ed558ccdull;
        h = (h << 31) | (h >> 33);
    }
    return h;
}

static size_t memo_hash(size_t col, row_set s, unsigned long long sig)
{
    unsigned long long h = s.lo;

    h = (h ^ (s.hi * 0x9e3779b97f4a7c15ull)) * 0xff51afd7ed558ccdull;
    h = (h ^ sig) * 0xc4ceb9fe1a85ec53ull;
    h = (h ^ ((unsigned long long)col + 0x165667b19e3779f9ull))
        * 0x94d049bb133111ebull;
    return (size_t)(h ^ (h >> 33));
}

static int memo_get(const prod_table *t, size_t col, row_set s,
                    unsigned long long sig)
{
    size_t j;

    if (!t->cap) {
        return -1;
    }
    for (j = memo_hash(col, s, sig) & (t->cap - 1); t->entries[j].used;
         j = (j + 1) & (t->cap - 1)) {
        if (t->entries[j].col == col && t->entries[j].rows.lo == s.lo
            && t->entries[j].rows.hi == s.hi && t->entries[j].sig == sig) {
            return t->entries[j].covered;
        }
    }
    return -1;
}

static void memo_put(ck_ctx *c, prod_table *t, size_t col, row_set s,
                     unsigned long long sig, int covered)
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
                size_t k = memo_hash(t->entries[i].col, t->entries[i].rows,
                                     t->entries[i].sig)
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
    for (j = memo_hash(col, s, sig) & (t->cap - 1); t->entries[j].used;
         j = (j + 1) & (t->cap - 1)) {
        if (t->entries[j].col == col && t->entries[j].rows.lo == s.lo
            && t->entries[j].rows.hi == s.hi && t->entries[j].sig == sig) {
            return;
        }
    }
    t->entries[j].col = col;
    t->entries[j].rows = s;
    t->entries[j].sig = sig;
    t->entries[j].covered = covered;
    t->entries[j].used = 1;
    t->count++;
}

/* The arms of rows whose column pattern matches one constructor of
   column `col`, or is a catch-all there: their nodes and their
   bitmap. */
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

/* The payload pattern one row contributes to a constructor split at
   `col`: a catch-all column answers the whole payload, a constructor
   alternative answers its own payload pattern. NULL covers. */
static const ek_node *payload_at(ck_ctx *c, const ek_node *row, size_t col,
                                 const ck_ctor *ct)
{
    const ek_node *pat = col < row->u.arm.patterns.count
                             ? row->u.arm.patterns.data[col] : NULL;
    const ek_node *flat[32];
    size_t nf, q;

    if (ck_pat_catchall(pat)) {
        return NULL;
    }
    nf = ck_pat_flatten(c, pat, flat, 0, 32);
    for (q = 0; q < nf && q < 32; q++) {
        if (flat[q]->kind == EK_N_P_CONS
            && ck_pat_head(c, flat[q]) == ct->sym) {
            return flat[q]->u.cons.payload;
        }
    }
    return NULL;
}

/* One owed payload, over the rows that reached this point. NULL in a
   row's slot is a catch-all payload; the payload type is the
   constructor's, at the scrutinee's instantiation. */
static int pend_one(ck_ctx *c, const prod_pend *p, const ek_node *const *live,
                    size_t nr)
{
    const ek_node *pays[128];
    const ck_decl *d = ck_decl_of(c, p->ct->owner);
    const ek_type **repl = NULL;
    const ek_type *pt;
    size_t nb = d ? d->nbinders : 0;
    size_t m;

    if (!p->ct->payload) {
        return 1;
    }
    if (nb) {
        repl = ek_arena_alloc(c->arena, nb * sizeof *repl);
        if (!repl) {
            c->oom = 1;
            return 1;
        }
        for (m = 0; m < nb; m++) {
            repl[m] = p->scrut->u.nominal.args[m];
        }
    }
    pt = ck_apply_binders(c, p->ct->payload, d->binders, repl, nb);
    for (m = 0; m < nr && m < 128; m++) {
        pays[m] = payload_at(c, live[m], p->col, p->ct);
    }
    return ck_covers(c, pt, pays, m);
}

/* Judge the owed payloads over the rows that reached this point. */
static int pend_ok(ck_ctx *c, const prod_pend *pd, size_t np,
                   const ek_node *const *live, size_t nr)
{
    size_t i;

    for (i = 0; i < np; i++) {
        if (!pend_one(c, &pd[i], live, nr)) {
            return 0;
        }
    }
    return 1;
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

/* The rows of one literal split: a catch-all or the same literal at
   the column keeps the row. */
static size_t rows_for_lit(ck_ctx *c, const ek_node *const *rows, size_t nr,
                           size_t col, const ek_node *lit,
                           const ek_node **out, row_set *keep)
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
                if (ck_pat_catchall(flat[q]) || lit_same(flat[q], lit)) {
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

/* The rows of the everything-else split: only a catch-all at the
   column answers a value no literal names. */
static size_t rows_for_rest(ck_ctx *c, const ek_node *const *rows, size_t nr,
                            size_t col, const ek_node **out, row_set *keep)
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
                if (ck_pat_catchall(flat[q])) {
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
                       size_t col, prod_pend *pd, size_t np);

/* A scalar column: the literals its rows name are each one value,
   everything else needs a catch-all row, and the remaining columns
   must be covered under every split. */
static int literal_column(ck_ctx *c, prod_table *memo,
                          const ek_node *const *live, size_t nr,
                          const ek_type **scrut, size_t ns, size_t col,
                          prod_pend *pd, size_t np)
{
    const ek_node *lits[32];
    const ek_node *sub[128];
    size_t nlits = 0, i, k, m, nsub;
    row_set keep;

    for (i = 0; i < nr; i++) {
        const ek_node *pat = col < live[i]->u.arm.patterns.count
                                 ? live[i]->u.arm.patterns.data[col] : NULL;
        const ek_node *flat[32];
        size_t nf = ck_pat_flatten(c, pat, flat, 0, 32);

        for (k = 0; k < nf && k < 32 && nlits < 32; k++) {
            int seen = 0;

            if (flat[k]->kind != EK_N_P_LIT) {
                continue;
            }
            for (m = 0; m < nlits; m++) {
                if (lit_same(lits[m], flat[k])) {
                    seen = 1;
                }
            }
            if (!seen) {
                lits[nlits++] = flat[k];
            }
        }
    }
    for (i = 0; i < nlits; i++) {
        keep.lo = keep.hi = 0;
        nsub = rows_for_lit(c, live, nr, col, lits[i], sub, &keep);
        if (nsub == 0) {
            return 0;
        }
        if (!covers_prod(c, memo, sub, nsub, scrut, ns, keep, col + 1, pd,
                         np)) {
            return 0;
        }
    }
    keep.lo = keep.hi = 0;
    nsub = rows_for_rest(c, live, nr, col, sub, &keep);
    if (nsub == 0) {
        return 0;
    }
    return covers_prod(c, memo, sub, nsub, scrut, ns, keep, col + 1, pd, np);
}

static int covers_prod(ck_ctx *c, prod_table *memo,
                       const ek_node *const *live, size_t nr,
                       const ek_type **scrut, size_t ns, row_set rows,
                       size_t col, prod_pend *pd, size_t np)
{
    const ek_type *s;
    const ck_decl *d;
    const ek_node *sub[128];
    unsigned long long sig = pend_sig(pd, np);
    size_t k;
    int all = 1;
    int hit = memo_get(memo, col, rows, sig);

    if (hit >= 0) {
        return hit;
    }
    if (col >= ns) {
        /* The rows that survive every split answer the combination;
           the owed payloads are judged over exactly these rows. */
        all = pend_ok(c, pd, np, live, nr);
        memo_put(c, memo, col, rows, sig, all);
        return all;
    }
    s = ck_prune(scrut[col]);
    if (!s || s->kind == EK_T_IVAR || s->kind == EK_T_TVAR
        || s->kind != EK_T_NOMINAL || !s->u.nominal.sym) {
        /* The column's type is open or a shape: judge what the path
           owes and suspend the rest. */
        all = pend_ok(c, pd, np, live, nr);
        memo_put(c, memo, col, rows, sig, all);
        return all;
    }
    d = ck_decl_of(c, s->u.nominal.sym);
    if (d && d->rhs == EK_TYPE_RHS_VARIANTS && d->nctors) {
        for (k = 0; k < d->nctors && all; k++) {
            row_set keep = {0, 0};
            size_t nsub = rows_for(c, live, nr, col, d->ctors[k], sub,
                                   &keep);

            if (nsub == 0) {
                all = 0;
                break;
            }
            if (np < PROD_MAX_PEND) {
                pd[np].ct = d->ctors[k];
                pd[np].scrut = s;
                pd[np].col = col;
                all = covers_prod(c, memo, sub, nsub, scrut, ns, keep,
                                  col + 1, pd, np + 1);
            } else {
                all = covers_prod(c, memo, sub, nsub, scrut, ns, keep,
                                  col + 1, pd, np);
            }
        }
        memo_put(c, memo, col, rows, sig, all);
        return all;
    }
    /* A scalar column: no constructors to split, so the literals name
       single values and everything else wants a catch-all row. */
    all = literal_column(c, memo, live, nr, scrut, ns, col, pd, np);
    memo_put(c, memo, col, rows, sig, all);
    return all;
}

/* The contributing arms. A guarded arm contributes no coverage — its
   pattern may match and its guard still fall through — so it never
   joins the live rows; the flag only drives the final-arm rule.
   Returns the live count and sets guarded. */
static size_t live_arms(const ek_node *const *arms, const int *contrib,
                        size_t narms, const ek_node **live, int *guarded)
{
    size_t i, nlive = 0;

    for (i = 0; i < narms; i++) {
        if (arms[i]->u.arm.guard) {
            *guarded = 1;
            continue;
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
    prod_pend pd[PROD_MAX_PEND];
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
        memset(pd, 0, sizeof pd);
        for (i = 0; i < nlive; i++) {
            full = set_add(full, i);
        }
        covered = covers_prod(c, &memo, live, nlive, scrut, nscrut, full, 0,
                              pd, 0);
    }
    if (!covered) {
        ck_error(c, cs, "E-exhaust",
                 "the case is not exhaustive (Clause 7.7.2)");
    }
}
