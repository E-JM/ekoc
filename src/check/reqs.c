#include <string.h>

#include "check/internal.h"

/* The service obligations of a checked body. A derived service that
   can halt, used on a type the body does not determine, is recorded
   here instead of being decided against an open receiver: inference
   variables settle by the end of the module pass and a scheme's rigid
   variables settle at each use, so the halt is decided at the type the
   call actually takes. Without the record a checked body would lower
   the service over a type that does not have it, and the emitter has
   no answer to give. */

static int type_has_var(const ek_type *t, int rigid);

/* Whether a variable sits anywhere in a composite type's components:
   the nominal arguments, a list's or dict's halves, a tuple's items, a
   record's field types, a function's parameters and result. */
static int comp_has_var(const ek_type *t, int rigid)
{
    size_t i;

    switch (t->kind) {
    case EK_T_NOMINAL:
        for (i = 0; i < t->u.nominal.argc; i++) {
            if (type_has_var(t->u.nominal.args[i], rigid)) {
                return 1;
            }
        }
        return 0;
    case EK_T_LIST:
        return type_has_var(t->u.list.elem, rigid);
    case EK_T_DICT:
        return type_has_var(t->u.dict.key, rigid)
            || type_has_var(t->u.dict.val, rigid);
    case EK_T_TUPLE:
        for (i = 0; i < t->u.tuple.count; i++) {
            if (type_has_var(t->u.tuple.items[i], rigid)) {
                return 1;
            }
        }
        return 0;
    case EK_T_RECORD:
        for (i = 0; i < t->u.record.count; i++) {
            if (type_has_var(t->u.record.fields[i].type, rigid)) {
                return 1;
            }
        }
        return 0;
    case EK_T_FN:
        for (i = 0; i < t->u.fn.count; i++) {
            if (type_has_var(t->u.fn.params[i], rigid)) {
                return 1;
            }
        }
        return type_has_var(t->u.fn.result, rigid);
    default:
        return 0;
    }
}

/* Whether the pruned type contains a variable anywhere. rigid selects
   the scheme's own variables; otherwise either kind counts. */
static int type_has_var(const ek_type *t, int rigid)
{
    t = ck_prune(t);
    if (!t) {
        return 0;
    }
    if (t->kind == EK_T_TVAR) {
        return 1;
    }
    if (t->kind == EK_T_IVAR) {
        return !rigid;
    }
    return comp_has_var(t, rigid);
}

/* Appends one obligation to a scheme's list, once; a repeated
   substitution of the same requirement is the same requirement. A
   body that keeps growing one (a recursive instantiation that never
   grounds) stops recording instead of running away. */
static int req_add(ck_ctx *c, ck_scheme *s, const ck_req *q, size_t cap)
{
    ck_reqs *rs = s->reqs;
    size_t i;

    if (!rs) {
        rs = ek_arena_alloc(c->arena, sizeof *rs);
        if (!rs) {
            c->oom = 1;
            return 0;
        }
        memset(rs, 0, sizeof *rs);
        s->reqs = rs;
    }
    for (i = 0; i < rs->n; i++) {
        if (rs->items[i].type == q->type
            && strcmp(rs->items[i].service, q->service) == 0) {
            return 0;
        }
    }
    if (rs->n >= cap) {
        return 0;
    }
    if (rs->n == rs->cap) {
        rs->items = ck_grow(c, rs->items, rs->n, &rs->cap,
                            sizeof *rs->items);
        if (c->oom) {
            return 0;
        }
    }
    rs->items[rs->n++] = *q;
    return 1;
}

/* Whether a member spelling names a derived service whose derivation
   can halt, so a use over an open type owes the halt to the
   instantiation. show and inspect are total and with is rejected at
   the definition, so neither is here. */
int ck_service_halts(const char *name)
{
    return strcmp(name, "eq") == 0 || strcmp(name, "ne") == 0
        || strcmp(name, "lt") == 0 || strcmp(name, "le") == 0
        || strcmp(name, "gt") == 0 || strcmp(name, "ge") == 0
        || strcmp(name, "to_json") == 0 || strcmp(name, "from_json") == 0;
}

/* The body and the canonical scheme are two elaborations of one
   annotation, so their rigid variables are different nodes with the
   same spellings. Rewrites the requirement into the scheme's nodes, so
   a use substitutes with the actuals the edge carries. */
static const ek_type *canonicalize(ck_ctx *c, const ek_type *t)
{
    const ek_type **from, **to;
    size_t i, j;

    if (!c->inst_decl || !c->inst_nvars || !c->inst_decl->nvars) {
        return t;
    }
    from = c->inst_vars;
    to = ek_arena_alloc(c->arena, c->inst_decl->nvars * sizeof *to);
    if (!to) {
        c->oom = 1;
        return t;
    }
    for (i = 0; i < c->inst_decl->nvars; i++) {
        const ek_type *want = c->inst_decl->vars[i];

        to[i] = NULL;
        for (j = 0; j < c->inst_nvars; j++) {
            const ek_type *have = from[j];

            if (have == want) {
                to[i] = want;
                break;
            }
            if (have && have->kind == EK_T_TVAR
                && want && want->kind == EK_T_TVAR
                && strcmp(have->u.tvar.name, want->u.tvar.name) == 0) {
                to[i] = want;
                break;
            }
        }
        if (!to[i]) {
            return t; /* the two elaborations do not pair: leave it */
        }
    }
    return ck_apply_binders(c, t, from, to, c->inst_nvars);
}

void ck_req_note(ck_ctx *c, const ek_node *at, const ek_type *r,
                 const char *service)
{
    ck_req q;

    if (c->oom || c->sweep_recheck || !at || !r) {
        return;
    }
    if (!type_has_var(r, 0)) {
        return; /* determined here: the service was decided in place */
    }
    memset(&q, 0, sizeof q);
    q.service = service;
    q.type = ck_prune(r);
    if (type_has_var(q.type, 1)) {
        /* In the canonical scheme's variables, so an edge substitutes
           them; a body's own variable is a different node. */
        q.type = canonicalize(c, q.type);
    }
    q.at = at;
    q.mod = c->cur;
    if (c->nreqs == c->creqs) {
        c->reqs = ck_grow(c, c->reqs, c->nreqs, &c->creqs, sizeof *c->reqs);
        if (c->oom) {
            return;
        }
    }
    c->reqs[c->nreqs++] = q;
    /* A scheme's own variable is decided at the scheme's uses; the
       record rides the scheme so every instantiation is checked. */
    if (c->inst_decl && type_has_var(r, 1)) {
        (void)req_add(c, (ck_scheme *)c->inst_decl, &q, 64);
    }
}

/* Decides one obligation over a type the body determined: the same
   derivation reading a concrete receiver gets, so the diagnostic is
   the one the use site would have produced. */
static void decide(ck_ctx *c, const ek_type *t, const char *service,
                   const ek_node *at)
{
    ek_reading rd = EK_RD_NONE;

    if (strcmp(service, "to_json") == 0 || strcmp(service, "from_json") == 0) {
        (void)ck_derive_json(c, at, t, service, &rd);
        return;
    }
    if (strcmp(service, "eq") == 0 || strcmp(service, "ne") == 0) {
        (void)ck_derive_eq(c, at, t, service, &rd, ck_std_type(c, "Bool"));
        return;
    }
    (void)ck_derive_lt(c, at, t, service, &rd, ck_std_type(c, "Bool"));
}

/* The obligations recorded in a body with no rigid variable: the
   module pass has settled the inference variables, so each is decided
   at its own use site now. */
static void decide_open(ck_ctx *c)
{
    size_t i;

    for (i = 0; i < c->nreqs && !c->oom; i++) {
        const ck_req *q = &c->reqs[i];
        const ek_type *t = ck_prune(q->type);

        if (type_has_var(t, 0)) {
            continue; /* still open, or a scheme's to decide */
        }
        c->cur = q->mod;
        decide(c, t, q->service, q->at);
    }
}

/* One substitution edge: the callee's recorded requirements ride the
   actuals of the use. A requirement that stays open propagates to the
   caller's scheme; a ground one is decided at the use site. */
static void propagate_edge(ck_ctx *c, ck_inst_edge *e, int *added)
{
    const ck_scheme *callee = e->callee;
    ck_reqs *rs;

    if (!callee || !callee->reqs) {
        return;
    }
    rs = callee->reqs;
    c->cur = e->mod;
    while (e->done < rs->n && !c->oom) {
        const ck_req *q = &rs->items[e->done++];
        const ek_type *t = ck_apply_binders(c, q->type, callee->vars,
                                            e->args, e->nargs);

        t = ck_prune(t);
        if (t && type_has_var(t, 0)) {
            if (e->caller && type_has_var(t, 1)) {
                ck_req next = *q;

                next.type = t;
                next.at = e->at;
                next.mod = e->mod;
                if (req_add(c, (ck_scheme *)e->caller, &next, 64)) {
                    *added = 1;
                }
            }
            continue;
        }
        if (t) {
            decide(c, t, q->service, e->at);
        }
    }
}

/* The schemes, to a fixpoint: each round substitutes every edge's open
   requirements with the actuals of the use. The recorded count per use
   keeps a repeat round from diagnosing the same edge twice. */
static void decide_schemes(ck_ctx *c)
{
    size_t i;
    unsigned round;

    for (round = 0; round < 32 && !c->oom; round++) {
        int added = 0;

        for (i = 0; i < c->ninst && !c->oom; i++) {
            propagate_edge(c, &c->inst_edges[i], &added);
        }
        if (!added) {
            break;
        }
    }
}

void ck_reqs_check(ck_ctx *c)
{
    const ek_surface *saved = c->cur;

    if (c->oom) {
        return;
    }
    decide_open(c);
    decide_schemes(c);
    c->cur = saved;
}
