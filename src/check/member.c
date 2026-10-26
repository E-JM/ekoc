#include <string.h>

#include "check/internal.h"

/* Receiver stations (Clause 7.4.3). Resolve deferred the selection of
   stations 1-3 to this stage: with the receiver's static type known, a
   link is a record field, a declared namespace member, a derived
   service, or a current-module function taking the receiver first.
   Everything the placeholder bridge leaves unknown takes the opaque
   reading. Calls are typed in call.c. */

/* The first reference of a node, optionally of one kind. */
int ck_ref_at(const ck_ctx *c, const ek_node *n, ek_sym_kind kind,
              const ek_ref **out)
{
    size_t lo = 0, hi = c->in->ref_count;

    if (!n) {
        return 0;
    }
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        if ((size_t)c->in->refs[mid].node < (size_t)n) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    for (; lo < c->in->ref_count && c->in->refs[lo].node == n; lo++) {
        if (!kind || c->in->refs[lo].kind == kind) {
            *out = &c->in->refs[lo];
            return 1;
        }
    }
    return 0;
}

/* Station 2: the declared namespace member of the type's head, found
   in the declaring module's surface (Clause 7.4.5). */
static const ek_symbol *ns_member(ck_ctx *c, const ek_symbol *type,
                                  const char *name)
{
    size_t k, i;

    if (!type || !type->module) {
        return NULL;
    }
    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, type->module) != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_NAMESPACE && m->owner
                && strcmp(m->owner, type->name) == 0
                && strcmp(m->name, name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* Station 3: a current-module free function whose first parameter is
   the receiver's type (Clause 7.4.3). */
static const ek_symbol *module_fn(ck_ctx *c, const char *name,
                                  const ek_type *recv)
{
    size_t i;

    for (i = 0; i < c->root->count; i++) {
        const ek_symbol *m = &c->root->syms[i];
        const ek_type *ft;

        if (m->kind != EK_SYM_FUNC || strcmp(m->name, name) != 0) {
            continue;
        }
        ft = ck_type_of(c, m);
        if (ft && ft->kind == EK_T_FN && ft->u.fn.count >= 1
            && (ck_conforms(c, recv, ft->u.fn.params[0], 1) == CK_OK
                || ck_conforms(c, ft->u.fn.params[0], recv, 1) == CK_OK)) {
            return m;
        }
    }
    return NULL;
}

/* The reading of one member link at its receiver's type (Clause
   7.4.3). Derived services of Clause 9.8.7: eq on every internal type
   whose derivation does not halt (9.8.4), lt on declared ADTs
   (9.8.5). */
static const ek_type *link_type(ck_ctx *c, const ek_node *at,
                                const ek_type *recv, const char *name,
                                ek_reading *rd)
{
    const ek_type *r = ck_prune(recv);
    const ek_type *bool_t = ck_std_type(c, "Bool");

    *rd = EK_RD_EXTERNAL;
    /* Station 1: a record field, on anonymous and record-shaped
       nominal receivers alike (Clause 7.4.3). */
    if (r && r->kind == EK_T_RECORD) {
        size_t i;

        for (i = 0; i < r->u.record.count; i++) {
            if (strcmp(r->u.record.fields[i].name, name) == 0) {
                *rd = EK_RD_FIELD;
                return r->u.record.fields[i].type;
            }
        }
    }
    if (r && r->kind == EK_T_NOMINAL && r->u.nominal.sym) {
        const ek_type *repr = ck_decl_repr(c, r);
        const ek_symbol *m;
        const ck_decl *d;

        if (repr && repr->kind == EK_T_RECORD) {
            size_t i;

            for (i = 0; i < repr->u.record.count; i++) {
                if (strcmp(repr->u.record.fields[i].name, name) == 0) {
                    *rd = EK_RD_FIELD;
                    return repr->u.record.fields[i].type;
                }
            }
        }
        /* Station 2: the type's namespace (declared members first). */
        m = ns_member(c, r->u.nominal.sym, name);
        if (m) {
            *rd = EK_RD_NAMESPACE;
            return ck_type_of(c, m);
        }
        d = ck_decl_of(c, r->u.nominal.sym);
        if (d && strcmp(name, "eq") == 0) {
            if (ck_no_eq(c, r)) {
                /* The derivation halts here (Clause 9.8.4): equality
                   over the type is E-no-eq and the member is absent. */
                ck_no_eq_error(c, at, r);
            } else {
                const ek_type **ps = ek_arena_alloc(c->arena,
                                                    2 * sizeof *ps);
                const ek_type *a = r, *b = r;

                if (ps) {
                    ps[0] = a;
                    ps[1] = b;
                    *rd = EK_RD_DERIVED;
                    return ck_fn(c, ps, 2, bool_t, EK_ARROW_PURE);
                }
                c->oom = 1;
                return ck_ivar(c);
            }
        }
        if (d && d->rhs == EK_TYPE_RHS_VARIANTS && strcmp(name, "lt") == 0) {
            const ek_type **ps = ek_arena_alloc(c->arena, 2 * sizeof *ps);
            const ek_type *a = r, *b = r;

            if (ps) {
                ps[0] = a;
                ps[1] = b;
                *rd = EK_RD_DERIVED;
                return ck_fn(c, ps, 2, bool_t, EK_ARROW_PURE);
            }
            c->oom = 1;
            return ck_ivar(c);
        }
        /* Station 3: the current module extends the foreign type. */
        m = module_fn(c, name, r);
        if (m) {
            *rd = EK_RD_MODULE_FN;
            return ck_type_of(c, m);
        }
    }
    if (!r || r->kind == EK_T_IVAR || r->kind == EK_T_TVAR) {
        *rd = EK_RD_UNKNOWN;
    }
    return ck_ivar(c);
}

/* A bare x.f is a field access (Clause 7.4.3). */
const ek_type *ck_field(ck_ctx *c, const ek_node *e)
{
    const ek_type *recv = ck_expr(c, e->u.field.recv, NULL);
    ek_reading rd = EK_RD_NONE;
    const ek_type *t = link_type(c, e, recv, e->u.field.name, &rd);

    ck_row(c, e, t, rd);
    return t;
}

/* The last link of a receiver chain, typed for the caller in call.c. */
const ek_type *ck_link(ck_ctx *c, const ek_node *link, const ek_type *recv,
                       ek_reading *rd)
{
    const ek_type *t = link_type(c, link, recv, link->u.field.name, rd);

    ck_row(c, link, t, *rd);
    return t;
}
