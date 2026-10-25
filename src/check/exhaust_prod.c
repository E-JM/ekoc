#include <string.h>

#include "check/internal.h"

/* Multi-scrutinee exhaustiveness (Clauses 7.7.2, 7.7.3) and the
   ck_exhaust entry. The product of the scrutinee types: an arm covers
   a combination when each column covers its scrutinee, and the arms
   must cover every combination. Columns whose type has no internally
   visible case structure defer; a catch-all covers every combination. */

/* The rows of arms whose column pattern matches one constructor of
   column `col`, or is a catch-all there. */
static size_t rows_for(ck_ctx *c, const ek_node *const *rows, size_t nr,
                       size_t col, const ck_ctor *ct, const ek_node **out)
{
    size_t i, nout = 0;

    for (i = 0; i < nr && nout < 32; i++) {
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
        }
    }
    return nout;
}

static int covers_prod(ck_ctx *c, const ek_type **scrut, size_t ns,
                       const ek_node *const *rows, size_t nr, size_t col)
{
    const ek_type *s = ck_prune(scrut[col]);
    const ck_decl *d;
    size_t k;
    int all = 1;

    if (!s || !(s->kind == EK_T_NOMINAL && s->u.nominal.sym)) {
        return 1; /* the column defers */
    }
    d = ck_decl_of(c, s->u.nominal.sym);
    if (!d || d->rhs != EK_TYPE_RHS_VARIANTS || !d->nctors) {
        return 1;
    }
    for (k = 0; k < d->nctors && all; k++) {
        const ek_node *sub[32];
        size_t nsub = rows_for(c, rows, nr, col, d->ctors[k], sub);

        if (nsub == 0) {
            all = 0;
            continue;
        }
        if (col + 1 < ns) {
            all = covers_prod(c, scrut, ns, sub, nsub, col + 1);
        }
    }
    return all;
}

void ck_exhaust(ck_ctx *c, const ek_node *cs, const ek_type **scrut,
                size_t nscrut, const ek_node *const *arms, const int *contrib,
                size_t narms)
{
    const ek_node *live[128];
    const ek_node *pats[128];
    int guarded = 0, covered;
    size_t i, nlive = 0;

    if (c->oom) {
        return;
    }
    if (narms == 0) {
        ck_error(c, cs, "E-exhaust", "the case has no arm (Clause 7.7.2)");
        return;
    }
    for (i = 0; i < narms; i++) {
        if (arms[i]->u.arm.guard) {
            guarded = 1;
        }
        if (contrib[i] && nlive < 128) {
            live[nlive++] = arms[i];
        }
    }
    if (guarded && arms[narms - 1]->u.arm.guard) {
        ck_error(c, cs, "E-exhaust",
                 "a case with a guarded arm ends with an unguarded arm "
                 "(Clause 7.7.3)");
        return;
    }
    if (nlive == 0) {
        ck_error(c, cs, "E-exhaust",
                 "every arm diverges; none discharges exhaustiveness "
                 "(Clauses 7.7.2, 7.7.4)");
        return;
    }
    if (nscrut == 1) {
        for (i = 0; i < nlive; i++) {
            pats[i] = live[i]->u.arm.patterns.count
                          ? live[i]->u.arm.patterns.data[0] : NULL;
        }
        covered = ck_covers(c, scrut[0], pats, nlive);
    } else {
        covered = covers_prod(c, scrut, nscrut, live, nlive, 0);
    }
    if (!covered) {
        ck_error(c, cs, "E-exhaust",
                 "the case is not exhaustive (Clause 7.7.2)");
    }
}
