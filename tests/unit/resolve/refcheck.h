#ifndef EK_TESTS_RESOLVE_REFCHECK_H
#define EK_TESTS_RESOLVE_REFCHECK_H

/* The reference-table invariants of resolve.h over a resolved run:
   refs strictly sorted by node address, and the table holds exactly
   the reference sites - every EK_N_NAME at an expression head,
   EK_N_FIELD, EK_N_CALL, EK_N_APPLY, EK_N_T_NAME in type position, and
   EK_N_P_CONS in pattern position, each exactly once. `_` is the unit
   value or the unit type, never a name; a lambda parameter is a
   binding site, not a reference; a composite key resolves in check
   (Clause 7.5.3). Violations print to stderr; rfx_check_refs returns
   their number. */

#include <stdlib.h>

#include "common.h"

typedef enum {
    RFK_NAME = 0, RFK_FIELD, RFK_CALL, RFK_APPLY, RFK_TNAME, RFK_PCONS,
    RFK_COUNT
} rfk_kind;

static inline const char *rfk_name(int k)
{
    switch (k) {
    case RFK_NAME: return "NAME";
    case RFK_FIELD: return "FIELD";
    case RFK_CALL: return "CALL";
    case RFK_APPLY: return "APPLY";
    case RFK_TNAME: return "T_NAME";
    default: return "P_CONS";
    }
}

/* Whether n is a reference site, and its kind when it is. */
static inline int rfk_of(const ek_node *n, int *out)
{
    switch (n->kind) {
    case EK_N_NAME:
        *out = RFK_NAME;
        return strcmp(n->u.path.data[0], "_") != 0;
    case EK_N_FIELD:
        *out = RFK_FIELD;
        return 1;
    case EK_N_CALL:
        *out = RFK_CALL;
        return 1;
    case EK_N_APPLY:
        *out = RFK_APPLY;
        return 1;
    case EK_N_T_NAME:
        *out = RFK_TNAME;
        return n->u.path.count == 1
                   ? strcmp(n->u.path.data[0], "_") != 0 : 1;
    case EK_N_P_CONS:
        *out = RFK_PCONS;
        return 1;
    default:
        return 0;
    }
}

static inline void rfk_walk(const ek_node *n, int suppress,
                            size_t counts[RFK_COUNT],
                            const ek_node ***sites, size_t *nsites,
                            size_t *cap)
{
    size_t i;

    if (!n) {
        return;
    }
    if (!suppress) {
        int k;

        if (rfk_of(n, &k)) {
            counts[k]++;
            if (sites) {
                if (*nsites == *cap) {
                    size_t ncap = *cap ? *cap * 2 : 256;
                    const ek_node **grown =
                        realloc(*sites, ncap * sizeof *grown);

                    if (!grown) {
                        return;
                    }
                    *sites = grown;
                    *cap = ncap;
                }
                (*sites)[(*nsites)++] = n;
            }
        }
        if (n->kind == EK_N_LAMBDA_PARAMS) {
            suppress = 1; /* parameter names are binding sites */
        }
    }
    switch (n->kind) {
    case EK_N_MODULE:
    case EK_N_BLOCK:
    case EK_N_LAMBDA_PARAMS:
    case EK_N_P_PAREN:
    case EK_N_P_OR:
    case EK_N_COMPOSITE:
    case EK_N_T_TUPLE:
    case EK_N_T_RECORD:
        for (i = 0; i < n->u.kids.count; i++) {
            rfk_walk(n->u.kids.data[i], suppress, counts, sites, nsites,
                     cap);
        }
        return;
    case EK_N_LET:
    case EK_N_REBIND:
    case EK_N_INVERT:
        rfk_walk(n->u.bind.pattern, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.bind.type, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.bind.value, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_NAMESPACE:
        rfk_walk(n->u.namespace.type, suppress, counts, sites, nsites,
                 cap);
        rfk_walk(n->u.namespace.value, suppress, counts, sites, nsites,
                 cap);
        return;
    case EK_N_TYPE_DECL:
        for (i = 0; i < n->u.type_decl.variants.count; i++) {
            rfk_walk(n->u.type_decl.variants.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        rfk_walk(n->u.type_decl.type, suppress, counts, sites, nsites,
                 cap);
        return;
    case EK_N_VARIANT:
        rfk_walk(n->u.variant.payload, suppress, counts, sites, nsites,
                 cap);
        return;
    case EK_N_CTYPE_DECL:
    case EK_N_TEST_DECL:
        rfk_walk(n->u.decl.body, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_EXTERN_DECL:
        rfk_walk(n->u.extern_decl.type, suppress, counts, sites, nsites,
                 cap);
        return;
    case EK_N_STRING_LIT:
        for (i = 0; i < n->u.str.count; i++) {
            rfk_walk(n->u.str.segs[i].expr, suppress, counts, sites,
                     nsites, cap);
        }
        return;
    case EK_N_FIELD:
        rfk_walk(n->u.field.recv, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_CALL:
        rfk_walk(n->u.call.callee, suppress, counts, sites, nsites, cap);
        for (i = 0; i < n->u.call.args.count; i++) {
            rfk_walk(n->u.call.args.data[i], suppress, counts, sites,
                     nsites, cap);
        }
        return;
    case EK_N_APPLY:
    case EK_N_LAMBDA:
    case EK_N_T_DICT:
        rfk_walk(n->u.pair.a, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.pair.b, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_COMPOSITE_ITEM:
        rfk_walk(n->u.pair.a, 1, counts, sites, nsites, cap);
        rfk_walk(n->u.pair.b, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_PAREN:
    case EK_N_UNARY:
    case EK_N_P_LIT:
    case EK_N_T_LIST:
    case EK_N_T_PAREN:
        rfk_walk(n->u.wrap.inner, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_BINARY:
        rfk_walk(n->u.binary.lhs, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.binary.rhs, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_IF:
        rfk_walk(n->u.ifexpr.cond, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.ifexpr.then, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.ifexpr.els, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_CASE:
        for (i = 0; i < n->u.caseexpr.scrutinees.count; i++) {
            rfk_walk(n->u.caseexpr.scrutinees.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        for (i = 0; i < n->u.caseexpr.arms.count; i++) {
            rfk_walk(n->u.caseexpr.arms.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        return;
    case EK_N_ARM:
        for (i = 0; i < n->u.arm.patterns.count; i++) {
            rfk_walk(n->u.arm.patterns.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        rfk_walk(n->u.arm.guard, suppress, counts, sites, nsites, cap);
        rfk_walk(n->u.arm.body, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_P_CONS:
        rfk_walk(n->u.cons.payload, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_P_BRACE:
        for (i = 0; i < n->u.p_brace.items.count; i++) {
            rfk_walk(n->u.p_brace.items.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        return;
    case EK_N_P_FIELD:
        rfk_walk(n->u.p_field.pattern, suppress, counts, sites, nsites,
                 cap);
        return;
    case EK_N_T_APPLY:
        rfk_walk(n->u.t_apply.head, suppress, counts, sites, nsites, cap);
        for (i = 0; i < n->u.t_apply.args.count; i++) {
            rfk_walk(n->u.t_apply.args.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        return;
    case EK_N_T_FIELD:
    case EK_N_T_PARAM:
        rfk_walk(n->u.t_field.type, suppress, counts, sites, nsites, cap);
        return;
    case EK_N_T_FN:
        for (i = 0; i < n->u.t_fn.params.count; i++) {
            rfk_walk(n->u.t_fn.params.data[i], suppress, counts,
                     sites, nsites, cap);
        }
        rfk_walk(n->u.t_fn.result, suppress, counts, sites, nsites, cap);
        return;
    default:
        return;
    }
}

static inline int rfk_addr_cmp(const void *a, const void *b)
{
    const ek_node *const *x = a;
    const ek_node *const *y = b;

    if (*x < *y) {
        return -1;
    }
    return *x > *y;
}

/* Checks the invariants over one resolved run: every module AST with a
   root supplies exactly the reference sites, refs are sorted, and each
   site is covered once. sites may be NULL to count only. Returns the
   violation count. */
static inline int rfx_check_refs(const rfx *f)
{
    size_t counts[RFK_COUNT] = {0};
    size_t refkinds[RFK_COUNT] = {0};
    const ek_node **sites = NULL;
    size_t nsites = 0, cap = 0;
    size_t i, bad = 0, k;

    for (k = 0; k < f->res.module_count; k++) {
        rfk_walk(f->res.modules[k].ast, 0, counts, &sites, &nsites, &cap);
    }
    for (i = 1; i < f->res.ref_count; i++) {
        if (f->res.refs[i - 1].node >= f->res.refs[i].node) {
            bad++;
            fprintf(stderr, "rfx_check_refs: refs not strictly sorted at "
                    "%zu\n", i);
            break;
        }
    }
    for (i = 0; i < f->res.ref_count; i++) {
        int k2;

        if (rfk_of(f->res.refs[i].node, &k2)) {
            refkinds[k2]++;
        } else {
            bad++;
            fprintf(stderr, "rfx_check_refs: ref %zu on a non-reference "
                    "node kind %d\n", i, (int)f->res.refs[i].node->kind);
        }
    }
    if (nsites) {
        qsort(sites, nsites, sizeof *sites, rfk_addr_cmp);
    }
    for (k = 0; k < RFK_COUNT; k++) {
        if (counts[k] != refkinds[k]) {
            bad++;
            fprintf(stderr, "rfx_check_refs: %s sites %zu, refs %zu\n",
                    rfk_name((int)k), counts[k], refkinds[k]);
        }
    }
    for (i = 0; i < nsites; i++) {
        size_t lo = 0, hi = f->res.ref_count;
        int k3;

        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;

            if (f->res.refs[mid].node < sites[i]) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo >= f->res.ref_count || f->res.refs[lo].node != sites[i]
            || (lo + 1 < f->res.ref_count
                && f->res.refs[lo + 1].node == sites[i])) {
            if (bad < 5 && rfk_of(sites[i], &k3)) {
                fprintf(stderr, "rfx_check_refs: %s at %s:%d:%d not "
                        "covered exactly once\n", rfk_name(k3),
                        f->res.module_count
                            ? f->res.modules[0].file : "?",
                        sites[i]->line, sites[i]->col);
            }
            bad++;
        }
    }
    free(sites);
    return (int)bad;
}

#endif
