#include <string.h>

#include "check/internal.h"

/* The reading of one member link at its receiver's type. Each arm
   walks the stations one receiver kind carries: a structural
   composite's fields and namespaces, a declared type's representation,
   namespace, and services, a function's derivations, and the current
   module's extension over all of them. ck_link_mod dispatches by the
   receiver's kind; the derived services cover every internal type
   whose derivation does not halt. */

/* The field of a record type, NULL when the name is absent. */
const ek_type *field_of(const ek_type *r, const char *name)
{
    size_t i;

    for (i = 0; i < r->u.record.count; i++) {
        if (strcmp(r->u.record.fields[i].name, name) == 0) {
            return r->u.record.fields[i].type;
        }
    }
    return NULL;
}

/* The namespace symbol `owner` declares as `name` in module `want`'s
   surface: std declares the structural list and dictionary members in
   its source, and a type's namespace lives in its declaring module. */
const ek_symbol *ns_scan(ck_ctx *c, const char *want,
                         const char *owner, const char *name)
{
    size_t k, i;

    for (k = 0; k < c->in->module_count; k++) {
        const ek_surface *s = &c->in->modules[k];

        if (strcmp(s->module, want) != 0) {
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
    const ek_symbol *m;

    if (!type || !type->module) {
        return NULL;
    }
    m = ns_scan(c, type->module, type->name, name);
    /* The fallback answers for the capability placeholders alone; a
       declared type's namespace stays its own module's. */
    if (!m && !type->decl && strcmp(type->module, "std") == 0) {
        m = ns_scan(c, "cap", type->name, name);
    }
    return m;
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

/* The derived services over one receiver, in answer order: the
   equality pair, the ordering family, show and inspect, the JSON
   pair, and with. Each derivation answers only its own names, so the
   chain falls through a miss. */
static const ek_type *derive_chain(ck_ctx *c, const ek_node *at,
                                   const ek_type *r, const char *name,
                                   ek_reading *rd, const ek_type *bool_t)
{
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
    return ck_derive_with(c, at, r, name, rd);
}

/* The current module's extension of the receiver: a free function
   taking it first, else the reported miss. */
static const ek_type *module_tail(ck_ctx *c, const ek_node *at,
                                  const ek_type *r, const char *name,
                                  const ek_symbol *mod, ek_reading *rd)
{
    const ek_type *t = ck_type_of(c, module_fn(c, name, r));

    if (t && t->kind == EK_T_FN) {
        *rd = EK_RD_MODULE_FN;
        return t;
    }
    return ck_no_member(c, at, r, name, mod);
}

/* The readings over a structural composite: an anonymous record's
   fields, the structural namespaces, the derived services, a
   compiler namespace row, and the module extension. */
static const ek_type *composite_link(ck_ctx *c, const ek_node *at,
                                     const ek_type *r, const char *name,
                                     const ek_symbol *mod, ek_reading *rd,
                                     const ek_type *bool_t)
{
    const ek_type *t = r->kind == EK_T_RECORD ? field_of(r, name) : NULL;

    if (t) {
        *rd = EK_RD_FIELD;
        return t;
    }
    if (r->kind == EK_T_LIST || r->kind == EK_T_DICT) {
        /* std declares the structural namespaces in its source, and
           no nominal symbol carries the owner. */
        const ek_symbol *m = ns_scan(c, "std",
                                     r->kind == EK_T_LIST ? "List" : "Dict",
                                     name);

        if (m) {
            *rd = EK_RD_NAMESPACE;
            return ck_type_of(c, m);
        }
    }
    t = derive_chain(c, at, r, name, rd, bool_t);
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
    return module_tail(c, at, r, name, mod, rd);
}

/* The derived services of a declared receiver: written at the service
   receiver the unapplied heads take, panic at the receiver itself. */
static const ek_type *nominal_service(ck_ctx *c, const ek_node *at,
                                      const ek_type *r, const char *name,
                                      ek_reading *rd, const ek_type *bool_t)
{
    const ek_type *srecv = ck_service_receiver(c, r);
    const ek_type *t;

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
    return ck_derive_json(c, at, srecv, name, rd);
}

/* The readings over a declared receiver: its representation's fields,
   its namespace, the conversion member, the derived services, with, a
   compiler namespace row, and the extending module function. */
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
    t = ck_derive_convert(c, r, name, rd);
    if (t) {
        return t;
    }
    t = nominal_service(c, at, r, name, rd, bool_t);
    if (t) {
        return t;
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
    return module_tail(c, at, r, name, mod, rd);
}

/* The readings over a function receiver: no fields and no namespace,
   the derived services and their halts, then the extension. */
static const ek_type *fn_link(ck_ctx *c, const ek_node *at,
                              const ek_type *r, const char *name,
                              const ek_symbol *mod, ek_reading *rd,
                              const ek_type *bool_t)
{
    const ek_type *t = derive_chain(c, at, r, name, rd, bool_t);

    if (t) {
        return t;
    }
    return module_tail(c, at, r, name, mod, rd);
}

/* An open receiver: the obligations the instantiation owes, the
   `with` rejection, and the unknown reading. */
static const ek_type *open_link(ck_ctx *c, const ek_node *at,
                                const ek_type *r, const char *name,
                                ek_reading *rd)
{
    if (r && (r->kind == EK_T_IVAR || r->kind == EK_T_TVAR)) {
        if (ck_service_halts(name)) {
            /* The service is resolved at instantiation; its halt is
               owed there, not answerable over an open receiver. */
            ck_req_note(c, at, r, name);
        }
        if (strcmp(name, "with") == 0) {
            /* `with` constructs a record of the receiver's type: a
               type variable names no record, whatever it later
               instantiates to. */
            ck_error(c, at, "E-no-member",
                     "the receiver type %s is not a record; `with` needs "
                     "a concrete record or ctype (Clause 9.8.6)",
                     ck_type_name(c, r));
        }
    }
    if (!r || r->kind == EK_T_IVAR || r->kind == EK_T_TVAR) {
        *rd = EK_RD_UNKNOWN;
    }
    return ck_opaque(c);
}

/* The reading of one member link at its receiver's type, by the
   receiver's kind. */
const ek_type *link_type(ck_ctx *c, const ek_node *at, const ek_type *recv,
                         const char *name, const ek_symbol *mod,
                         ek_reading *rd)
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
        return composite_link(c, at, r, name, mod, rd, bool_t);
    }
    if (r && r->kind == EK_T_NOMINAL && r->u.nominal.sym) {
        return nominal_link(c, at, r, name, mod, rd, bool_t);
    }
    if (r && r->kind == EK_T_FN) {
        return fn_link(c, at, r, name, mod, rd, bool_t);
    }
    return open_link(c, at, r, name, rd);
}
