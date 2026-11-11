#include <string.h>

#include "check/internal.h"

/* The conversion member: on a newtype receiver, the payload type's own
   name selects the sole constructor's payload. The link reads as the
   pure one-parameter arrow from the newtype to the payload, so both
   the receiver form and the type-namespace form type as ordinary
   member calls; the lowerer dispatches on the reading for the payload
   read. The name is matched against the payload itself, never
   resolved, and the judgment is identity, not conformance: no
   adaptation enters it. */

/* The payload a sole constructor carries, instantiated at the receiver
   nominal's own arguments; NULL when the receiver's declaration is not
   the newtype form. */
static const ek_type *sole_payload(ck_ctx *c, const ek_type *recv)
{
    const ek_type *r = ck_prune(recv);
    const ck_decl *d;
    const ck_ctor *ct;

    if (!r || r->kind != EK_T_NOMINAL || !r->u.nominal.sym
        || r->u.nominal.sym->decl == NULL) {
        return NULL;
    }
    d = ck_decl_of(c, r->u.nominal.sym);
    if (!d || d->nctors != 1) {
        return NULL;
    }
    ct = d->ctors[0];
    if (!ct || !ct->payload) {
        return NULL; /* a zero payload selects nothing */
    }
    return ck_apply_binders(c, ct->payload, d->binders,
                            r->u.nominal.args, r->u.nominal.argc);
}

/* True when the payload names the member's spelling: the head symbol
   of a fully applied nominal payload, applied or not — an applied
   payload is still one named type, and the member is its name. */
static int names_payload(const ek_type *payload, const char *name)
{
    const ek_type *t = ck_prune(payload);

    return t && t->kind == EK_T_NOMINAL && t->u.nominal.sym
        && strcmp(t->u.nominal.sym->name, name) == 0;
}

const ek_type *ck_derive_convert(ck_ctx *c, const ek_type *r,
                                 const char *name, ek_reading *rd)
{
    const ek_type *payload = sole_payload(c, r);
    const ek_type **ps;

    if (!names_payload(payload, name)) {
        return NULL;
    }
    ps = ek_arena_alloc(c->arena, sizeof *ps);
    if (!ps) {
        c->oom = 1;
        return NULL;
    }
    ps[0] = ck_prune(r);
    *rd = EK_RD_CONVERT;
    return ck_fn(c, ps, 1, payload, EK_ARROW_PURE);
}
