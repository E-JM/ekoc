#include <string.h>

#include "check/internal.h"

/* The eq and JSON halt judgments, as exact reachability over the type
   graph. A node halts when the service itself stops at the node or
   any node it contains halts. A declaration enters through its
   representation or its instantiated constructor payloads, and a node
   joins the graph once, so a recursive shape gets the verdict of its
   acyclic unrolling and no depth bound is needed. */

typedef struct {
    ck_map idx;             /* type node -> row + 1 */
    const ek_type **nodes;
    unsigned char *halts;   /* the propagated verdict */
    unsigned char *roots;   /* the service stops at the node itself */
    size_t *off;            /* n + 1 child-window bounds */
    size_t *kids;           /* the child windows, in row order */
    size_t n, cap, nk, kcap;
    int json;               /* which stop set applies */
} halt_graph;

#define HALT_NONE ((size_t)-1)

static size_t halt_add(ck_ctx *c, halt_graph *g, const ek_type *t);

/* Whether a ctype declaration carries the opaque pointer field: the
   field's equality would compare addresses, which the language does
   not observe (Clause 8.11). Nested ctypes need no walk of their own
   -- their own declaration stops through this same test. */
static int ctype_carries_ptr(const ek_type *t)
{
    const ek_node *dn = t->u.nominal.sym ? t->u.nominal.sym->decl : NULL;
    const ek_node *rec;
    size_t i;

    if (!dn || dn->kind != EK_N_CTYPE_DECL
        || !dn->u.decl.body || dn->u.decl.body->kind != EK_N_T_RECORD) {
        return 0;
    }
    rec = dn->u.decl.body;
    for (i = 0; i < rec->u.kids.count; i++) {
        const ek_node *tn = rec->u.kids.data[i]->u.t_field.type;

        tn = tn && tn->kind == EK_N_T_PAREN ? tn->u.wrap.inner : tn;
        if (tn && tn->kind == EK_N_T_NAME && tn->u.path.count == 3
            && strcmp(tn->u.path.data[0], "C") == 0
            && strcmp(tn->u.path.data[1], "Ptr") == 0
            && strcmp(tn->u.path.data[2], "Void") == 0) {
            return 1;
        }
    }
    return 0;
}

/* Whether the service stops at the node itself. */
static int halt_stops(const halt_graph *g, const ek_type *t)
{
    const ek_type *key;

    if (t->kind == EK_T_FN) {
        return 1;
    }
    if (t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && ctype_carries_ptr(t)) {
        return 1; /* the pointer's equality observes an address */
    }
    if (!g->json) {
        return ck_is_capability(t) || ck_is_handle(t);
    }
    if (t->kind == EK_T_DICT) {
        key = ck_prune(t->u.dict.key);
        if (key && key->kind != EK_T_IVAR && key->kind != EK_T_TVAR
            && !ck_is_std(key, "String")) {
            return 1; /* a non-string key has no default form */
        }
    }
    /* A range is an iteration form, not a data shape: no row of the
       mapping table names it, so the pair halts here. */
    if (t->kind == EK_T_NOMINAL && ck_is_std(t, "Range")) {
        return 1;
    }
    return ck_is_capability(t) || ck_is_handle(t);
}

static void kid_push(ck_ctx *c, halt_graph *g, size_t row)
{
    if (row == HALT_NONE || c->oom) {
        return;
    }
    if (g->nk == g->kcap) {
        size_t ncap = g->kcap ? g->kcap * 2 : 16;
        size_t *nk = ek_arena_alloc(c->arena, ncap * sizeof *nk);

        if (!nk) {
            c->oom = 1;
            return;
        }
        if (g->nk) {
            memcpy(nk, g->kids, g->nk * sizeof *nk);
        }
        g->kids = nk;
        g->kcap = ncap;
    }
    g->kids[g->nk++] = row;
}

/* The element the compiler-supplied set and vector placeholders
   directly contain: the two runtime-backed containers carry no
   declaration whose representation or payloads could be read, so
   their single argument is the only type they hold. */
static int placeholder_element(const ek_type *t, const ek_type **elem)
{
    if (!t->u.nominal.sym || t->u.nominal.sym->decl
        || t->u.nominal.argc != 1) {
        return 0;
    }
    if (!ck_is_std(t, "Set") && !ck_is_std(t, "Vector")) {
        return 0;
    }
    *elem = t->u.nominal.args[0];
    return 1;
}

/* The nodes a nominal directly contains: its representation, or the
   constructor payloads with the instance's arguments applied. */
static void nominal_children(ck_ctx *c, halt_graph *g, const ek_type *t)
{
    const ek_type *repr;
    const ck_decl *d;
    size_t i;

    if (!t->u.nominal.sym) {
        return;
    }
    repr = ck_decl_repr(c, t);
    if (repr) {
        kid_push(c, g, halt_add(c, g, repr));
        return;
    }
    {
        const ek_type *elem;

        if (placeholder_element(t, &elem)) {
            kid_push(c, g, halt_add(c, g, elem));
            return;
        }
    }
    d = ck_decl_of(c, t->u.nominal.sym);
    if (!d) {
        return;
    }
    for (i = 0; i < d->nctors; i++) {
        const ek_type *pl = d->ctors[i] ? d->ctors[i]->payload : NULL;

        if (!pl) {
            continue;
        }
        if (d->nbinders && t->u.nominal.argc != d->nbinders) {
            continue; /* an unapplied head keeps its opacity */
        }
        if (d->nbinders) {
            pl = ck_apply_binders(c, pl, d->binders, t->u.nominal.args,
                                  d->nbinders);
        }
        kid_push(c, g, halt_add(c, g, pl));
    }
}

/* The rows one node directly feeds. */
static void halt_children(ck_ctx *c, halt_graph *g, const ek_type *t)
{
    size_t i;

    switch (t->kind) {
    case EK_T_LIST:
        kid_push(c, g, halt_add(c, g, t->u.list.elem));
        return;
    case EK_T_DICT:
        kid_push(c, g, halt_add(c, g, t->u.dict.key));
        kid_push(c, g, halt_add(c, g, t->u.dict.val));
        return;
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            kid_push(c, g, halt_add(c, g, t->u.tuple.items[i]));
        }
        return;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            kid_push(c, g, halt_add(c, g, t->u.record.fields[i].type));
        }
        return;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            kid_push(c, g, halt_add(c, g, t->u.fn.params[i]));
        }
        kid_push(c, g, halt_add(c, g, t->u.fn.result));
        return;
    case EK_T_NOMINAL:
        nominal_children(c, g, t);
        return;
    default:
        return;
    }
}

/* One node into the graph, once. Nodes join in discovery order;
   their child windows are laid down later, contiguously, by the
   driver loop. */
static size_t halt_add(ck_ctx *c, halt_graph *g, const ek_type *t)
{
    size_t id;

    t = ck_prune(t);
    if (!t || c->oom) {
        return HALT_NONE;
    }
    id = (size_t)ck_map_get(&g->idx, t);
    if (id) {
        return id - 1;
    }
    if (g->n == g->cap) {
        size_t ncap = g->cap ? g->cap * 2 : 16;
        const ek_type **nn = ek_arena_alloc(c->arena, ncap * sizeof *nn);
        unsigned char *nh = ek_arena_alloc(c->arena, ncap);
        unsigned char *nr = ek_arena_alloc(c->arena, ncap);
        size_t *no = ek_arena_alloc(c->arena, (ncap + 1) * sizeof *no);

        if (!nn || !nh || !nr || !no) {
            c->oom = 1;
            return HALT_NONE;
        }
        if (g->n) {
            memcpy(nn, g->nodes, g->n * sizeof *nn);
            memcpy(nh, g->halts, g->n);
            memcpy(nr, g->roots, g->n);
            memcpy(no, g->off, (g->n + 1) * sizeof *no);
        }
        g->nodes = nn;
        g->halts = nh;
        g->roots = nr;
        g->off = no;
        g->cap = ncap;
    }
    id = g->n++;
    g->nodes[id] = t;
    g->halts[id] = 0;
    g->roots[id] = halt_stops(g, t) != 0;
    ck_map_put(c, &g->idx, t, (const void *)(id + 1));
    return id;
}

/* Walks the discovered nodes in order, laying each node's child
   window down contiguously; nodes the windows discover join the
   back of the walk. */
static void halt_edges(ck_ctx *c, halt_graph *g)
{
    size_t i;

    for (i = 0; i < g->n && !c->oom; i++) {
        g->off[i] = g->nk;
        halt_children(c, g, g->nodes[i]);
    }
    g->off[i] = g->nk;
}

/* Verdicts propagate to a fixpoint: a node halts when it stops the
   service or contains a node that halts. */
static void halt_fix(halt_graph *g)
{
    int changed = 1;
    size_t i, k;

    for (i = 0; i < g->n; i++) {
        g->halts[i] = g->roots[i];
    }
    while (changed) {
        changed = 0;
        for (i = 0; i < g->n; i++) {
            if (g->halts[i]) {
                continue;
            }
            for (k = g->off[i]; k < g->off[i + 1]; k++) {
                if (g->halts[g->kids[k]]) {
                    g->halts[i] = 1;
                    changed = 1;
                    break;
                }
            }
        }
    }
}

static int halt_query(ck_ctx *c, const ek_type *t, int json)
{
    halt_graph g;
    size_t root;

    if (c->oom) {
        return 0;
    }
    memset(&g, 0, sizeof g);
    g.json = json;
    root = halt_add(c, &g, t);
    if (root == HALT_NONE) {
        return 0;
    }
    halt_edges(c, &g);
    halt_fix(&g);
    return g.halts[root];
}

int ck_no_eq(ck_ctx *c, const ek_type *t)
{
    return halt_query(c, t, 0);
}

int ck_no_json(ck_ctx *c, const ek_type *t)
{
    return halt_query(c, t, 1);
}

void ck_no_eq_error(ck_ctx *c, const ek_node *at, const ek_type *t)
{
    if (c->oom || !ck_no_eq(c, t)) {
        return;
    }
    ck_error(c, at, "E-no-eq",
             "equality over %s has no derivation; capabilities, "
             "handles, functions, and ctypes carrying C.Ptr.Void have "
             "no `eq` (Clause 9.8.4)",
             ck_type_name(c, t));
}

/* The key-domain rule at a construction site: the pair a dictionary
   or a set builds over its domain keys by the domain's own equality,
   so the judgment is the equality halt itself. A domain the site does
   not determine rides the obligation record and is decided at the
   type the site takes. */
void ck_key_domain_error(ck_ctx *c, const ek_node *at, const ek_type *t)
{
    ck_req_note(c, at, t, "eq");
    ck_no_eq_error(c, at, t);
}

void ck_json_shape_error(ck_ctx *c, const ek_node *at, const ek_type *t)
{
    if (c->oom || !ck_no_json(c, t)) {
        return;
    }
    ck_error(c, at, "E-json-shape",
             "the type %s has no default JSON form; capabilities, "
             "handles, functions, and non-string-keyed dictionaries "
             "have no `to_json` (Clause 9.9.3)", ck_type_name(c, t));
}
