#include "lower/internal.h"

/* The key-domain stamping. lower_key_pair names the hook pair a
   construction site passes the runtime: the stamped family services
   for the record, tuple, and algebraic domains, the minted container
   services for a list or a dictionary standing as the key domain, and
   the runtime's default pair for the scalar and text slots.
   lower_key_hooks stamps those services onto the families that carry
   the domain's values, lazily at the dict and set sites that build
   over the domain. A family stamps once, so two instantiations of one
   generic family keep the first domain's hooks. The domains whose
   derivation halts answer zero, and the construction sites diagnose
   the refusal. */

/* Stamps the eq and hash of the applied type onto the family, once:
   the family carries its first stamping, and the typeinfo it names
   joins the unit through the family's own emission. */
static void stamp_family(lower_ctx *c, lower_gtype *g, const ek_type *t)
{
    if (!g || g->eq_name || g->hash_name) {
        return;
    }
    g->eq_name = lower_dsvc_request(c, NULL, DSV_EQ, t);
    g->hash_name = lower_dsvc_request(c, NULL, DSV_HASH, t);
    if (!g->constructed && !g->shared) {
        g->shared = 1; /* the typeinfo the hooks name joins the unit */
    }
}

/* Stamps the record, tuple, or ctype representation the applied type
   names, answering the family the stamping landed on. */
static lower_gtype *stamp_shape(lower_ctx *c, const ek_type *t)
{
    lower_gtype *g = NULL;
    const char **fnames;
    const ek_type **ftypes;
    size_t nf;

    if (lower_shape_of(c, NULL, t, &fnames, &ftypes, &nf, &g) && g) {
        stamp_family(c, g, t);
    }
    return g;
}

/* The stamped pair of the record, tuple, and nominal domains: the
   services read back off the family the stamping landed on -- every
   constructor of an algebraic type carries the same pair. Zero when
   the domain has no family, a capability or handle whose derivation
   halts. */
static int stamp_pair(lower_ctx *c, const ek_type *t, const char **keq,
                      const char **khash)
{
    lower_variant *vs;
    lower_gtype *g = NULL;
    size_t nv, i;

    if (t->kind == EK_T_NOMINAL) {
        if (lower_is_range(t)) {
            const ek_type *elem = t->u.nominal.argc == 1
                                      ? t->u.nominal.args[0] : NULL;

            g = elem ? lower_range_family(c, lower_scalar_kind(elem)) : NULL;
            if (g) {
                stamp_family(c, g, t);
            }
        } else if (lower_variants_of(c, NULL, t, &vs, &nv, 0)) {
            for (i = 0; i < nv; i++) {
                stamp_family(c, lower_gtype_of_stem(c, vs[i].stem), t);
            }
            g = lower_gtype_of_stem(c, vs[0].stem);
        } else if (t->u.nominal.sym && t->u.nominal.sym->decl) {
            g = stamp_shape(c, t);
        }
    } else {
        g = stamp_shape(c, t);
    }
    if (!g || !g->eq_name || !g->hash_name) {
        return 0;
    }
    *keq = g->eq_name;
    *khash = g->hash_name;
    return 1;
}

/* The minted pair of a container domain: the services that walk the
   container itself, named for its applied spelling. */
static int mint_pair(lower_ctx *c, const ek_type *t, const char **keq,
                     const char **khash)
{
    const char *q = lower_dsvc_request(c, NULL, DSV_EQ, t);
    const char *h = lower_dsvc_request(c, NULL, DSV_HASH, t);

    if (!q || !h) {
        return 0;
    }
    *keq = q;
    *khash = h;
    return 1;
}

/* The hook pair a construction site passes the runtime over the key
   domain: the family services the domain stamps for the record, tuple,
   and algebraic shapes, the minted services for a container standing
   as the domain -- a list, a dictionary, the runtime-backed set, or
   the runtime-backed vector -- and the runtime's default pair for the
   scalar and text slots. Zero when the domain keys nowhere -- the
   refusal the construction sites diagnose. The names stand at the
   default pair whatever the answer, so the drained unit still
   compiles. */
int lower_key_pair(lower_ctx *c, const ek_type *t, const char **keq,
                   const char **khash)
{
    *keq = "ek_key_eq";
    *khash = "ek_key_hash";
    if (!t) {
        return 1;
    }
    if (lower_scalar_kind(t) != EKSC_OTHER) {
        return 1;
    }
    switch (t->kind) {
    case EK_T_FN:
        return 0; /* the backstop: a checked construction refuses an
                     arrow domain before lower runs */
    case EK_T_NEVER:
        return 1; /* a Never key never answers: no value exists */
    case EK_T_LIST:
    case EK_T_DICT:
        return mint_pair(c, t, keq, khash);
    case EK_T_NOMINAL:
        if (lower_is_set(t) || lower_is_vector(t)) {
            return mint_pair(c, t, keq, khash);
        }
        return stamp_pair(c, t, keq, khash);
    case EK_T_RECORD:
    case EK_T_TUPLE:
        return stamp_pair(c, t, keq, khash);
    default:
        return 1;
    }
}

/* The key-domain stamping: whether the domain keys at all, stamping
   its pair onto the families that carry its values along the way. */
int lower_key_hooks(lower_ctx *c, const ek_type *t)
{
    const char *keq;
    const char *khash;

    return lower_key_pair(c, t, &keq, &khash);
}
