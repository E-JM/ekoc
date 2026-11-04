#include <string.h>

#include "check/internal.h"

/* Receiver stations. Resolve deferred the selection to check: with
   the receiver's static type known, a link is a record field, a
   declared namespace member, a derived service, or a current-module
   function taking the receiver first. Derived services are the
   equality pair over every type whose derivation does not halt, the
   ordering family over the value layer and declared ADTs, with over
   records, show and inspect over determined receivers, and the JSON
   pair over every type with a default form. A name no station answers
   is reported, except in a namespace the compiler still supplies
   members for, which keeps the opaque reading. Calls are typed in
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

/* A declared namespace member by owner spelling, for the structural
   list and dictionary namespaces: std declares their members in its
   source, and no nominal symbol carries the owner. */
static const ek_symbol *ns_member_owner(ck_ctx *c, const char *owner,
                                        const char *name)
{
    size_t k, i;

    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, "std") != 0) {
            continue;
        }
        for (i = 0; i < s->count; i++) {
            const ek_symbol *m = &s->syms[i];

            if (m->kind == EK_SYM_NAMESPACE && m->owner
                && strcmp(m->owner, owner) == 0
                && strcmp(m->name, name) == 0) {
                return m;
            }
        }
    }
    return NULL;
}

/* The declared namespace member of the type's head, found in the
   declaring module's surface. The nine capability types are std
   placeholder rows, so their declared members live in the capability
   module's surface instead (Clauses 7.9.1, 10.8): a miss in the
   declaring module falls back to it. */
static const ek_symbol *ns_member(ck_ctx *c, const ek_symbol *type,
                                  const char *name)
{
    size_t k, i, pass;

    if (!type || !type->module) {
        return NULL;
    }
    for (pass = 0; pass < 2; pass++) {
        const char *want = pass == 0 ? type->module : "cap";

        if (pass == 1 && (type->decl || strcmp(type->module, "std") != 0)) {
            /* The fallback answers for the capability placeholders
               alone; a declared type's namespace stays its own
               module's. */
            return NULL;
        }
        for (k = 0; k < c->in->module_count; k++) {
            const ek_surface *s = &c->in->modules[k];

            if (strcmp(s->module, want) != 0) {
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
    }
    return NULL;
}

/* A current-module free function whose first parameter is the
   receiver's type. */
static const ek_symbol *module_fn(ck_ctx *c, const char *name,
                                  const ek_type *recv)
{
    size_t i;

    for (i = 0; i < c->cur->count; i++) {
        const ek_symbol *m = &c->cur->syms[i];
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

/* The services the checker types by derivation. A name in this set is
   never the opaque bridge: it derives, or the link reports why it
   cannot. */
int ck_service_name(const char *name)
{
    static const char *names[] = {"eq",   "ne",   "lt",   "le",
                                  "gt",   "ge",   "show", "inspect",
                                  "with", "to_json", "from_json"};
    size_t i;

    for (i = 0; i < sizeof names / sizeof *names; i++) {
        if (strcmp(names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The readings over a declared receiver: its representation's fields,
   its namespace, the derived services, and the extending module
   function. */
static const ek_type *nominal_link(ck_ctx *c, const ek_node *at,
                                   const ek_type *r, const char *name,
                                   const ek_symbol *mod, ek_reading *rd,
                                   const ek_type *bool_t)
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
    {
        const ek_type *srecv = ck_service_receiver(c, r);

        t = ck_derive_eq(c, at, srecv, name, rd, bool_t);
        if (t) {
            return t;
        }
        t = ck_derive_lt(c, at, srecv, name, rd, bool_t);
        if (t) {
            return t;
        }
        t = ck_derive_display(c, srecv, name, rd);
        if (t) {
            return t;
        }
        t = ck_derive_panic(c, r, name, rd);
        if (t) {
            return t;
        }
        t = ck_derive_json(c, at, srecv, name, rd);
        if (t) {
            return t;
        }
    }
    t = ck_derive_with(c, at, r, name, rd);
    if (t) {
        return t;
    }
    {
        /* A compiler-provided row of the namespace: the default of a
           name no declaration owns. */
        int owned = 0;
        const ek_type *row = ck_ns_row(c, r, name, &owned);

        if (row) {
            *rd = EK_RD_NAMESPACE;
            return row;
        }
    }
    /* The current module extends the foreign type. */
    m = module_fn(c, name, r);
    if (m) {
        *rd = EK_RD_MODULE_FN;
        return ck_type_of(c, m);
    }
    return ck_no_member(c, at, r, name, mod);
}

/* The reading of one member link at its receiver's type. Derived
   services: the equality pair on every internal type whose derivation
   does not halt, the ordering family on the value layer and declared
   ADTs, with on record receivers, show, inspect, and the JSON pair on
   the receivers each service covers. */
static const ek_type *link_type(ck_ctx *c, const ek_node *at,
                                const ek_type *recv, const char *name,
                                const ek_symbol *mod, ek_reading *rd)
{
    const ek_type *r = ck_prune(recv);
    const ek_type *bool_t = ck_std_type(c, "Bool");

    *rd = EK_RD_EXTERNAL;
    /* A bottom receiver never evaluates, so the access does not
       return: every member of `Never` is `Never`. */
    if (r && r->kind == EK_T_NEVER) {
        *rd = EK_RD_NONE;
        return ck_never(c);
    }
    /* A record field, on anonymous and record-shaped nominal receivers
       alike; the structural composites take the equality, display, and
       JSON derivations. */
    if (r && (r->kind == EK_T_RECORD || r->kind == EK_T_TUPLE
              || r->kind == EK_T_LIST || r->kind == EK_T_DICT
              || r->kind == EK_T_UNIT)) {
        const ek_type *t = r->kind == EK_T_RECORD ? field_of(r, name)
                                                  : NULL;

        if (t) {
            *rd = EK_RD_FIELD;
            return t;
        }
        if (r->kind == EK_T_LIST || r->kind == EK_T_DICT) {
            const ek_symbol *m = ns_member_owner(
                c, r->kind == EK_T_LIST ? "List" : "Dict", name);

            if (m) {
                *rd = EK_RD_NAMESPACE;
                return ck_type_of(c, m);
            }
        }
        t = ck_derive_eq(c, at, r, name, rd, bool_t);
        if (t) {
            return t;
        }
        t = ck_derive_lt(c, at, r, name, rd, bool_t);
        if (t) {
            return t;
        }
        t = ck_derive_with(c, at, r, name, rd);
        if (t) {
            return t;
        }
        t = ck_derive_display(c, r, name, rd);
        if (t) {
            return t;
        }
        t = ck_derive_json(c, at, r, name, rd);
        if (t) {
            return t;
        }
        {
            int owned = 0;
            const ek_type *row = ck_ns_row(c, r, name, &owned);

            if (row) {
                *rd = EK_RD_NAMESPACE;
                return row;
            }
        }
        return ck_no_member(c, at, r, name, mod);
    }
    if (r && r->kind == EK_T_NOMINAL && r->u.nominal.sym) {
        return nominal_link(c, at, r, name, mod, rd, bool_t);
    }
    if (r && r->kind == EK_T_FN) {
        /* A function type has no fields, no namespace, and no module
           reading: the derived services and their halts. */
        const ek_type *t = ck_derive_eq(c, at, r, name, rd, bool_t);

        if (t) {
            return t;
        }
        t = ck_derive_lt(c, at, r, name, rd, bool_t);
        if (t) {
            return t;
        }
        t = ck_derive_display(c, r, name, rd);
        if (t) {
            return t;
        }
        t = ck_derive_json(c, at, r, name, rd);
        if (t) {
            return t;
        }
        t = ck_derive_with(c, at, r, name, rd);
        if (t) {
            return t;
        }
        return ck_no_member(c, at, r, name, mod);
    }
    if (!r || r->kind == EK_T_IVAR || r->kind == EK_T_TVAR) {
        *rd = EK_RD_UNKNOWN;
    }
    return ck_opaque(c);
}

/* The field of a record type, direct or through a record-shaped
   nominal's representation. */
const ek_type *ck_field_of(ck_ctx *c, const ek_type *r, const char *name)
{
    const ek_type *p = ck_prune(r);

    if (!p) {
        return NULL;
    }
    if (p->kind == EK_T_RECORD) {
        return field_of(p, name);
    }
    if (p->kind == EK_T_NOMINAL && p->u.nominal.sym) {
        p = ck_decl_repr(c, p);
        if (p && p->kind == EK_T_RECORD) {
            return field_of(p, name);
        }
    }
    return NULL;
}

/* A bare x.f is a field access. A head-qualified chain resolve decided
   names a module function, a namespace member, or an extern; its
   scheme is the chain's type, so the member reads as a value wherever
   the chain does. A receiver-form module path keeps the field walk:
   check owes it the receiver-type decision a field of the same
   spelling can win. */
const ek_type *ck_field(ck_ctx *c, const ek_node *e)
{
    const ek_ref *ref = NULL;
    const ek_type *recv;
    ek_reading rd = EK_RD_NONE;

    if (ck_ref_at(c, e, 0, &ref) && ref->station == EK_ST_QUALIFIED
        && ref->symbol
        && (ref->kind == EK_SYM_FUNC || ref->kind == EK_SYM_NAMESPACE
            || ref->kind == EK_SYM_EXTERN)) {
        (void)ck_expr(c, e->u.field.recv, NULL);
        return ck_type_of(c, ref->symbol);
    }
    recv = ck_expr(c, e->u.field.recv, NULL);
    return ck_link(c, e, recv, e->u.field.name, &rd);
}

/* One member link at its receiver's type. The caller supplies the
   spelling: a chain link carries it, and the free-call form of a
   service carries it on its name head. */
const ek_type *ck_link(ck_ctx *c, const ek_node *link, const ek_type *recv,
                       const char *name, ek_reading *rd)
{
    return ck_link_mod(c, link, recv, name, NULL, rd);
}

/* One member link at its receiver's type, with the module a chain
   named when no station answers: the miss reports against it. */
const ek_type *ck_link_mod(ck_ctx *c, const ek_node *link,
                                const ek_type *recv, const char *name,
                                const ek_symbol *mod, ek_reading *rd)
{
    const ek_type *t = link_type(c, link, recv, name, mod, rd);

    ck_row(c, link, t, *rd);
    return t;
}
