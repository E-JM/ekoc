#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "check/internal.h"

/* The row table and its diagnostics. One row per typed node, grown in
   check order and finished once every body of the closure is checked;
   the error and warning paths report under the file of the module
   being checked. */

static int row_cmp(const void *a, const void *b)
{
    const ek_ck_row *x = a, *y = b;

    if ((size_t)x->node < (size_t)y->node) {
        return -1;
    }
    return x->node == y->node ? 0 : 1;
}

/* The final table: the rows sort by node address for lower's binary
   search, the snapshot substitutes the bound variables away, and the
   world bits fill over the sorted rows. */
void finish_rows(ck_ctx *c)
{
    size_t i;

    qsort(c->rows, c->nrows, sizeof *c->rows, row_cmp);
    for (i = 0; i < c->nrows; i++) {
        c->rows[i].type = ck_subst(c, c->rows[i].type);
    }
    ck_fill_bits(c);
}

void ck_row(ck_ctx *c, const ek_node *n, const ek_type *t, ek_reading r)
{
    if (c->oom || !n || c->sweep_recheck) {
        /* A recheck reads the table; it does not grow it. */
        return;
    }
    if (c->nrows == c->crows) {
        c->rows = ck_grow(c, c->rows, c->nrows, &c->crows, sizeof *c->rows);
        if (c->oom) {
            return;
        }
    }
    c->rows[c->nrows].node = n;
    c->rows[c->nrows].type = t;
    c->rows[c->nrows].reading = r;
    c->rows[c->nrows].bit = EK_BIT_UNSET;
    c->nrows++;
}

/* The sorted row of one node, by address. */
const ek_ck_row *ck_row_of(const ck_ctx *c, const ek_node *n)
{
    size_t lo = 0, hi = c->nrows;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        if ((size_t)c->rows[mid].node < (size_t)n) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < c->nrows && c->rows[lo].node == n ? &c->rows[lo] : NULL;
}

static void row_diag(ck_ctx *c, ek_severity sev, const ek_node *n,
                     const char *code, const char *fmt, va_list ap)
{
    char msg[400];

    vsnprintf(msg, sizeof msg, fmt, ap);
    ek_diag_emit(c->diag, sev, code, ck_file(c),
                 n ? n->line : 1, n ? n->col : 1, "%s", msg);
}

void ck_error(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
              ...)
{
    va_list ap;

    if (c->oom) {
        return;
    }
    va_start(ap, fmt);
    row_diag(c, EK_SEV_ERROR, n, code, fmt, ap);
    va_end(ap);
}

void ck_warn(ck_ctx *c, const ek_node *n, const char *code, const char *fmt,
             ...)
{
    va_list ap;

    if (c->oom) {
        return;
    }
    va_start(ap, fmt);
    row_diag(c, EK_SEV_WARNING, n, code, fmt, ap);
    va_end(ap);
}

const char *ck_file(const ck_ctx *c)
{
    if (c->cur) {
        return c->cur->file;
    }
    return c->in->module_count ? c->root->file : "test.eko";
}
