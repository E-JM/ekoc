#include <string.h>

#include "lower/internal.h"

/* The one-directional shape adaptation of one value: a nominal record
   flowing into an anonymous record expectation rebuilds field by field
   -- the shared anonymous family never meets a nominal object, because
   the two layouts order one field set differently. The destination
   family births from owned reads of the source family's slots. The
   value comes back unchanged when the pair is not that shape. */

/* The source's field names cover the destination's set. */
static int names_cover(const ek_type *repr, const lower_gtype *g)
{
    size_t i, k;

    for (i = 0; i < g->nf; i++) {
        int found = 0;

        for (k = 0; k < repr->u.record.count; k++) {
            if (repr->u.record.fields[k].name
                && strcmp(repr->u.record.fields[k].name, g->fnames[i])
                       == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

/* The destination family of an adapting pair: the anonymous record of
   the expectation, when the source's declared record repr covers its
   field set. NULL keeps the value unchanged. */
static lower_gtype *adapt_target(lower_ctx *c, const ek_type *from,
                                 const ek_type *to)
{
    const ek_type *repr;
    lower_gtype *g;

    if (!from || from->kind != EK_T_NOMINAL || !to
        || to->kind != EK_T_RECORD || !to->u.record.count) {
        return NULL;
    }
    repr = lower_repr_type(c, from);
    if (!repr || repr->kind != EK_T_RECORD
        || repr->u.record.count != to->u.record.count) {
        return NULL;
    }
    g = lower_anon_record(c, to);
    if (!g || g->shape != GT_REC || !g->fnames || !names_cover(repr, g)) {
        return NULL;
    }
    return g;
}

/* The rebuild: each destination field keeps its source slot's value,
   the helper births the fresh record, and the keeps and the source
   value die once it stands. */
static lower_val emit_adapt(lower_ctx *c, lower_scope *scope,
                            const ek_node *at, const ek_type *from,
                            lower_gtype *g, lower_val v)
{
    const char *t = lower_as_temp(c, scope, &v);
    const char **argv;
    size_t cap = 0, i;

    argv = lower_grow(c, NULL, &cap, g->nf, sizeof *argv);
    if (!argv) {
        c->oom = 1;
        return lower_val_unit();
    }
    for (i = 0; i < g->nf; i++) {
        const char *slot = lower_field_slot(c, at, from, t, g->fnames[i]);

        argv[i] = lower_temp(c, scope);
        lower_pad(c);
        lower_emitf(c, "ek_val %s = ek_keep(%s);\n", argv[i],
                    slot ? slot : "ek_val_unit()");
    }
    v = (lower_val){ lower_temp(c, scope), 1 };
    lower_pad(c);
    lower_emitf(c, "ek_val %s = %s(", v.text, g->helper);
    for (i = 0; i < g->nf; i++) {
        lower_emitf(c, "%s%s", i ? ", " : "", argv[i]);
    }
    lower_emitf(c, ");\n");
    for (i = g->nf; i > 0; i--) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", argv[i - 1]);
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return v;
}

lower_val lower_shape_adapt(lower_ctx *c, lower_scope *scope, lower_val v,
                            const ek_node *at, const ek_type *from,
                            const ek_type *to)
{
    lower_gtype *g = adapt_target(c, from, to);

    if (!g) {
        return v;
    }
    g->constructed = 1;
    return emit_adapt(c, scope, at, from, g, v);
}
