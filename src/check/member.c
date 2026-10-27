#include <string.h>

#include "check/internal.h"

/* Receiver stations. Resolve deferred the selection to check: with
   the receiver's static type known, a link is a record field, a
   declared namespace member, a derived service, or a current-module
   function taking the receiver first. Everything the placeholder
   bridge leaves unknown takes the opaque reading. Calls are typed in
   call.c. */

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

/* The declared namespace member of the type's head, found in the
   declaring module's surface. */
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

/* A current-module free function whose first parameter is the
   receiver's type. */
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

/* The field of a record type, NULL when the name is absent. */
static const ek_type *field_of(const ek_type *r, const char *name)
{
    size_t i;

    for (i = 0; i < r->u.record.count; i++) {
        if (strcmp(r->u.record.fields[i].name, name) == 0) {
            return r->u.record.fields[i].type;
        }
    }
    return NULL;
}

/* The two-receiver arrow of a derived service. */
static const ek_type *derived_fn(ck_ctx *c, const ek_type *r,
                                 const ek_type *bool_t)
{
    const ek_type **ps = ek_arena_alloc(c->arena, 2 * sizeof *ps);

    if (ps) {
        ps[0] = r;
        ps[1] = r;
        return ck_fn(c, ps, 2, bool_t, EK_ARROW_PURE);
    }
    c->oom = 1;
    return ck_ivar(c);
}

/* The derived eq member: E-no-eq where the derivation halts, the
   service where it does not. NULL falls through. */
static const ek_type *derive_eq(ck_ctx *c, const ek_node *at,
                                const ek_type *r, const char *name,
                                const ck_decl *d, ek_reading *rd,
                                const ek_type *bool_t)
{
    if (!d || strcmp(name, "eq") != 0) {
        return NULL;
    }
    if (ck_no_eq(c, r)) {
        /* The derivation halts here: equality over the type is
           E-no-eq and the member is absent. */
        ck_no_eq_error(c, at, r);
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    return derived_fn(c, r, bool_t);
}

/* The derived order member over a declared ADT. NULL falls through. */
static const ek_type *derive_lt(ck_ctx *c, const ek_type *r,
                                const char *name, const ck_decl *d,
                                ek_reading *rd, const ek_type *bool_t)
{
    if (!d || d->rhs != EK_TYPE_RHS_VARIANTS || strcmp(name, "lt") != 0) {
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    return derived_fn(c, r, bool_t);
}

/* The closest expressible arrow of the `with` service: the service's
   second parameter (the partial fields) has no type-syntax name, so
   the link row carries (t, t) -> t as a stand-in; the call itself is
   typed in call.c, which checks the literal against t directly. */
static const ek_type *with_fn(ck_ctx *c, const ek_type *r)
{
    const ek_type **ps = ek_arena_alloc(c->arena, 2 * sizeof *ps);

    if (ps) {
        ps[0] = r;
        ps[1] = r;
        return ck_fn(c, ps, 2, r, EK_ARROW_PURE);
    }
    c->oom = 1;
    return ck_ivar(c);
}

/* True when the nominal is a std placeholder: it has no declaration,
   so its shape is opaque and nothing is diagnosed about it. */
static int std_placeholder(const ek_type *r)
{
    return r->u.nominal.sym->decl == NULL;
}

/* The derived with member over a record receiver: an anonymous record
   or a record-shaped nominal. A receiver whose type is otherwise known
   is rejected here; a placeholder or undetermined receiver falls
   through to opacity. NULL falls through. */
static const ek_type *derive_with(ck_ctx *c, const ek_node *at,
                                  const ek_type *r, const char *name,
                                  ek_reading *rd)
{
    const ek_type *rec = r;

    if (strcmp(name, "with") != 0) {
        return NULL;
    }
    if (rec->kind == EK_T_NOMINAL) {
        if (std_placeholder(rec)) {
            return NULL;
        }
        rec = ck_decl_repr(c, rec);
        if (!rec || rec->kind != EK_T_RECORD) {
            ck_error(c, at, "E-type-mismatch",
                     "`with` constructs a record from a record, not %s "
                     "(Clause 9.8.6)", ck_type_name(c, r));
            return NULL;
        }
    } else if (rec->kind != EK_T_RECORD) {
        ck_error(c, at, "E-type-mismatch",
                 "`with` constructs a record from a record, not %s "
                 "(Clause 9.8.6)", ck_type_name(c, r));
        return NULL;
    }
    *rd = EK_RD_DERIVED;
    return with_fn(c, r);
}

/* The readings over a declared receiver: its representation's fields,
   its namespace, the derived services, and the extending module
   function. */
static const ek_type *nominal_link(ck_ctx *c, const ek_node *at,
                                   const ek_type *r, const char *name,
                                   ek_reading *rd, const ek_type *bool_t)
{
    const ek_type *repr = ck_decl_repr(c, r);
    const ek_type *t;
    const ek_symbol *m;

    if (repr && repr->kind == EK_T_RECORD) {
        t = field_of(repr, name);
        if (t) {
            *rd = EK_RD_FIELD;
            return t;
        }
    }
    /* The type's namespace (declared members first). */
    m = ns_member(c, r->u.nominal.sym, name);
    if (m) {
        *rd = EK_RD_NAMESPACE;
        return ck_type_of(c, m);
    }
    t = derive_eq(c, at, r, name, ck_decl_of(c, r->u.nominal.sym), rd,
                  bool_t);
    if (t) {
        return t;
    }
    t = derive_lt(c, r, name, ck_decl_of(c, r->u.nominal.sym), rd, bool_t);
    if (t) {
        return t;
    }
    t = derive_with(c, at, r, name, rd);
    if (t) {
        return t;
    }
    /* The current module extends the foreign type. */
    m = module_fn(c, name, r);
    if (m) {
        *rd = EK_RD_MODULE_FN;
        return ck_type_of(c, m);
    }
    return ck_ivar(c);
}

/* The reading of one member link at its receiver's type. Derived
   services: eq on every internal type whose derivation does not halt,
   lt on declared ADTs, with on record receivers. */
static const ek_type *link_type(ck_ctx *c, const ek_node *at,
                                const ek_type *recv, const char *name,
                                ek_reading *rd)
{
    const ek_type *r = ck_prune(recv);
    const ek_type *bool_t = ck_std_type(c, "Bool");

    *rd = EK_RD_EXTERNAL;
    /* A record field, on anonymous and record-shaped nominal receivers
       alike. */
    if (r && r->kind == EK_T_RECORD) {
        const ek_type *t = field_of(r, name);

        if (t) {
            *rd = EK_RD_FIELD;
            return t;
        }
        t = derive_with(c, at, r, name, rd);
        if (t) {
            return t;
        }
    }
    if (r && r->kind == EK_T_NOMINAL && r->u.nominal.sym) {
        return nominal_link(c, at, r, name, rd, bool_t);
    }
    if (r && r->kind == EK_T_FN) {
        /* A function type has fields only in a record that carries
           it; it has no namespace and no module reading. */
        ck_error(c, at, "E-unbound-member",
                 "the receiver type %s has no member `%s` "
                 "(Clause 7.4.3)", ck_type_name(c, r), name);
        return ck_ivar(c);
    }
    if (!r || r->kind == EK_T_IVAR || r->kind == EK_T_TVAR) {
        *rd = EK_RD_UNKNOWN;
    }
    return ck_ivar(c);
}

/* A bare x.f is a field access. */
const ek_type *ck_field(ck_ctx *c, const ek_node *e)
{
    const ek_type *recv = ck_expr(c, e->u.field.recv, NULL);
    ek_reading rd = EK_RD_NONE;

    return ck_link(c, e, recv, e->u.field.name, &rd);
}

/* One member link at its receiver's type. The caller supplies the
   spelling: a chain link carries it, and the free-call form of a
   service carries it on its name head. */
const ek_type *ck_link(ck_ctx *c, const ek_node *link, const ek_type *recv,
                       const char *name, ek_reading *rd)
{
    const ek_type *t = link_type(c, link, recv, name, rd);

    ck_row(c, link, t, *rd);
    return t;
}
