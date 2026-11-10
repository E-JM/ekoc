#include <string.h>

#include "check/internal.h"

/* Receiver stations. Resolve deferred the selection to check: with
   the receiver's static type known, a link is a record field, a
   declared namespace member, a derived service, or a current-module
   function taking the receiver first. A name no station answers is
   reported, except in a namespace the compiler still supplies members
   for, which keeps the opaque reading. The readings live in link.c;
   calls are typed in call.c. */

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
        && ref->symbol) {
        if (ref->kind == EK_SYM_FUNC || ref->kind == EK_SYM_NAMESPACE
            || ref->kind == EK_SYM_EXTERN) {
            (void)ck_expr(c, e->u.field.recv, NULL);
            return ck_type_of(c, ref->symbol);
        }
        if (ref->kind == EK_SYM_CONSTRUCTOR) {
            /* A qualified constructor read: a zero payload is the
               value of its type, a payload constructor reads only
               under application. */
            const ck_ctor *ct = ck_ctor_of(c, ref->symbol);

            (void)ck_expr(c, e->u.field.recv, NULL);
            if (ct && ct->payload) {
                ck_error(c, e, "E-ctor-value",
                         "the constructor `%s` takes a payload and is "
                         "not a value; apply it to its payload "
                         "(Clause 6.5.2)", ref->symbol->name);
                return ck_opaque(c);
            }
            if (ct && ct->owner) {
                return ck_nominal(c, ct->owner, NULL, 0);
            }
            return ck_opaque(c);
        }
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
