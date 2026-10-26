#include <stdlib.h>
#include <string.h>

#include "check/internal.h"

/* The entry point. One ek_check call checks the compilation root of
   the resolve result against the closure's interfaces; the rows come
   back sorted by node address with inference variables substituted
   away (check.h). */

static int row_cmp(const void *a, const void *b)
{
    const ek_ck_row *x = a, *y = b;

    if ((size_t)x->node < (size_t)y->node) {
        return -1;
    }
    return x->node == y->node ? 0 : 1;
}

ek_check_result ek_check(ek_resolve_result in, ek_arena *arena,
                         ek_diag *diag)
{
    ck_ctx c;
    ek_check_result r;
    const ek_node *mod;
    size_t i, n;

    memset(&c, 0, sizeof c);
    memset(&r, 0, sizeof r);
    c.arena = arena;
    c.diag = diag;
    c.in = &in;
    if (!in.modules || !in.module_count) {
        return r;
    }
    c.root = &in.modules[0];
    mod = c.root->ast;
    if (!mod || mod->kind != EK_N_MODULE) {
        return r;
    }
    ck_prebind(&c);
    ck_decls(&c);
    n = mod->u.kids.count;
    for (i = 0; i < n && !c.oom; i++) {
        const ek_node *d = mod->u.kids.data[i];

        switch (d->kind) {
        case EK_N_LET:
        case EK_N_REBIND:
        case EK_N_INVERT:
        case EK_N_NAMESPACE:
        case EK_N_TEST_DECL:
        case EK_N_EXTERN_DECL:
            ck_stmt(&c, d, 1);
            break;
        case EK_N_TYPE_DECL:
        case EK_N_CTYPE_DECL:
        case EK_N_IMPORT:
            break; /* built by ck_decls; imports carry no body */
        default:
            ck_expr(&c, d, NULL);
            break;
        }
    }
    ck_externs(&c);
    if (c.nrows) {
        qsort(c.rows, c.nrows, sizeof *c.rows, row_cmp);
        for (i = 0; i < c.nrows; i++) {
            c.rows[i].type = ck_subst(&c, c.rows[i].type);
        }
        ck_fill_bits(&c);
    }
    r.rows = c.rows;
    r.count = c.nrows;
    return r;
}
