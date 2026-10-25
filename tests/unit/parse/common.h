#ifndef EK_TESTS_PARSE_COMMON_H
#define EK_TESTS_PARSE_COMMON_H

/* Shared fixtures for the parse unit suites: ek_lex, ek_layout, and
   ek_parse over in-memory sources, plus a generic AST walk. */

#include "harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lex/lex.h"
#include "layout/layout.h"
#include "parse/parse.h"
#include "support/arena.h"
#include "support/diag.h"

typedef struct {
    ek_arena *arena;
    ek_diag *diag;
    ek_lex_result lex;
    ek_layout_result lay;
    ek_parse_result ast;
    const ek_diagnostic *ds;
    size_t dn;
} parse_fixture;

static inline void pfx_begin(parse_fixture *f, const char *src)
{
    memset(f, 0, sizeof *f);
    f->arena = ek_arena_create();
    f->diag = ek_diag_create();
    f->lex = ek_lex(src, strlen(src), "test.eko", f->arena, f->diag);
    f->lay = ek_layout(f->lex.tokens, f->lex.count, "test.eko", f->arena,
                       f->diag);
    f->ast = ek_parse(f->lay.tokens, f->lay.count, "test.eko", f->arena,
                      f->diag);
}

static inline void pfx_end(parse_fixture *f)
{
    ek_diag_destroy(f->diag);
    ek_arena_destroy(f->arena);
}

static inline size_t pfx_errors(const parse_fixture *f)
{
    return ek_diag_count(f->diag, EK_SEV_ERROR);
}

/* Drains the buffer once; query the cache afterwards. */
static inline void pfx_collect(parse_fixture *f)
{
    f->ds = ek_diag_drain(f->diag, &f->dn);
}

static inline int pfx_has_code(const parse_fixture *f, const char *code)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].code && strcmp(f->ds[i].code, code) == 0) {
            return 1;
        }
    }
    return 0;
}

static inline int pfx_msg_has(const parse_fixture *f, const char *needle)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].message && strstr(f->ds[i].message, needle)) {
            return 1;
        }
    }
    return 0;
}

/* Gathers the child nodes of any kind; NULL children are skipped. */
static inline int pfx_children(const ek_node *n, const ek_node **out, int max)
{
    int cnt = 0;
    size_t i;

#define PFX_ADD(x)                                             \
    do {                                                       \
        if ((x) && cnt < max) {                                \
            out[cnt++] = (x);                                  \
        }                                                      \
    } while (0)
    switch (n->kind) {
    case EK_N_MODULE:
    case EK_N_BLOCK:
    case EK_N_LAMBDA_PARAMS:
    case EK_N_P_PAREN:
    case EK_N_P_OR:
    case EK_N_T_TUPLE:
    case EK_N_T_RECORD:
        for (i = 0; i < n->u.kids.count; i++) {
            PFX_ADD(n->u.kids.data[i]);
        }
        break;
    case EK_N_STRING_LIT:
        for (i = 0; i < n->u.str.count; i++) {
            PFX_ADD(n->u.str.segs[i].expr);
        }
        break;
    case EK_N_PAREN:
    case EK_N_UNARY:
    case EK_N_P_LIT:
    case EK_N_T_PAREN:
    case EK_N_T_LIST:
        PFX_ADD(n->u.wrap.inner);
        break;
    case EK_N_BINARY:
        PFX_ADD(n->u.binary.lhs);
        PFX_ADD(n->u.binary.rhs);
        break;
    case EK_N_FIELD:
        PFX_ADD(n->u.field.recv);
        break;
    case EK_N_CALL:
        PFX_ADD(n->u.call.callee);
        for (i = 0; i < n->u.call.args.count; i++) {
            PFX_ADD(n->u.call.args.data[i]);
        }
        break;
    case EK_N_APPLY:
    case EK_N_LAMBDA:
    case EK_N_COMPOSITE_ITEM:
    case EK_N_T_DICT:
        PFX_ADD(n->u.pair.a);
        PFX_ADD(n->u.pair.b);
        break;
    case EK_N_LET:
    case EK_N_REBIND:
    case EK_N_INVERT:
        PFX_ADD(n->u.bind.pattern);
        PFX_ADD(n->u.bind.type);
        PFX_ADD(n->u.bind.value);
        break;
    case EK_N_NAMESPACE:
        PFX_ADD(n->u.namespace.type);
        PFX_ADD(n->u.namespace.value);
        break;
    case EK_N_TYPE_DECL:
        for (i = 0; i < n->u.type_decl.variants.count; i++) {
            PFX_ADD(n->u.type_decl.variants.data[i]);
        }
        PFX_ADD(n->u.type_decl.type);
        break;
    case EK_N_VARIANT:
        PFX_ADD(n->u.variant.payload);
        break;
    case EK_N_CTYPE_DECL:
    case EK_N_TEST_DECL:
        PFX_ADD(n->u.decl.body);
        break;
    case EK_N_EXTERN_DECL:
        PFX_ADD(n->u.extern_decl.type);
        break;
    case EK_N_IF:
        PFX_ADD(n->u.ifexpr.cond);
        PFX_ADD(n->u.ifexpr.then);
        PFX_ADD(n->u.ifexpr.els);
        break;
    case EK_N_CASE:
        for (i = 0; i < n->u.caseexpr.scrutinees.count; i++) {
            PFX_ADD(n->u.caseexpr.scrutinees.data[i]);
        }
        for (i = 0; i < n->u.caseexpr.arms.count; i++) {
            PFX_ADD(n->u.caseexpr.arms.data[i]);
        }
        break;
    case EK_N_ARM:
        for (i = 0; i < n->u.arm.patterns.count; i++) {
            PFX_ADD(n->u.arm.patterns.data[i]);
        }
        PFX_ADD(n->u.arm.guard);
        PFX_ADD(n->u.arm.body);
        break;
    case EK_N_P_CONS:
        PFX_ADD(n->u.cons.payload);
        break;
    case EK_N_P_FIELD:
        PFX_ADD(n->u.p_field.pattern);
        break;
    case EK_N_P_BRACE:
        for (i = 0; i < n->u.p_brace.items.count; i++) {
            PFX_ADD(n->u.p_brace.items.data[i]);
        }
        break;
    case EK_N_T_APPLY:
        PFX_ADD(n->u.t_apply.head);
        for (i = 0; i < n->u.t_apply.args.count; i++) {
            PFX_ADD(n->u.t_apply.args.data[i]);
        }
        break;
    case EK_N_T_FN:
        for (i = 0; i < n->u.t_fn.params.count; i++) {
            PFX_ADD(n->u.t_fn.params.data[i]);
        }
        PFX_ADD(n->u.t_fn.result);
        break;
    case EK_N_T_FIELD:
    case EK_N_T_PARAM:
        PFX_ADD(n->u.t_field.type);
        break;
    default:
        break;
    }
#undef PFX_ADD
    return cnt;
}

static inline const ek_node *pfx_find(const ek_node *n, ek_node_kind kind)
{
    const ek_node *kids[8];
    int cnt, i;

    if (!n) {
        return NULL;
    }
    if (n->kind == kind) {
        return n;
    }
    cnt = pfx_children(n, kids, 8);
    for (i = 0; i < cnt; i++) {
        const ek_node *hit = pfx_find(kids[i], kind);

        if (hit) {
            return hit;
        }
    }
    return NULL;
}

static inline size_t pfx_count_kind(const ek_node *n, ek_node_kind kind)
{
    const ek_node *kids[8];
    size_t total = 0;
    int cnt, i;

    if (!n) {
        return 0;
    }
    if (n->kind == kind) {
        total++;
    }
    cnt = pfx_children(n, kids, 8);
    for (i = 0; i < cnt; i++) {
        total += pfx_count_kind(kids[i], kind);
    }
    return total;
}

/* The module's i-th top-level item, or NULL. */
static inline const ek_node *pfx_item(const parse_fixture *f, size_t i)
{
    if (!f->ast.module || i >= f->ast.module->u.kids.count) {
        return NULL;
    }
    return f->ast.module->u.kids.data[i];
}

/* The annotation of a failing `x: <type> = 1` line may be absent. */
static inline const ek_node *pfx_ann(const parse_fixture *f)
{
    const ek_node *item = pfx_item(f, 0);

    return item ? item->u.bind.type : NULL;
}

#endif
