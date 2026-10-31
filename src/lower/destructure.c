#include <string.h>

#include "lower/internal.h"

/* The flat tuple destructuring of an initial binding: a paren pattern
   of names and wildcards reads the components out of the generated
   tuple struct as owned copies, and the value temp dies once the
   reads stand. The row type names the layout; an opaque one keeps the
   general diagnostic. */

/* The tuple representation the value's row names: the anonymous
   family of a tuple row, the declared family of a tuple-shaped
   nominal; NULL when the row names neither. */
static lower_gtype *tuple_repr_of(lower_ctx *c, const ek_type *t)
{
    if (t && t->kind == EK_T_TUPLE) {
        return lower_anon_tuple(c, t->u.tuple.count);
    }
    if (t && t->kind == EK_T_NOMINAL && t->u.nominal.sym) {
        lower_gtype *g = lower_decl_repr(c, t->u.nominal.sym);

        return g && g->shape == GT_TUP ? g : NULL;
    }
    return NULL;
}

/* True when every component of a paren pattern is a name or the
   wildcard: the flat destructuring the binding supports. */
static int flat_tuple_pat(const ek_node *pat)
{
    size_t i;

    if (!pat || pat->kind != EK_N_P_PAREN || pat->u.kids.count == 0) {
        return 0;
    }
    for (i = 0; i < pat->u.kids.count; i++) {
        const ek_node *k = pat->u.kids.data[i];

        if (k->kind != EK_N_P_VAR && k->kind != EK_N_P_WILDCARD) {
            return 0;
        }
    }
    return 1;
}

/* A tuple destructuring: the right-hand side lowers to a temp, every
   component reads out as an owned copy through the generated struct,
   and the temp dies. The row type names the layout; an opaque one
   keeps the general diagnostic. */
int lower_tuple_let(lower_ctx *c, lower_scope *scope,
                          const ek_node *m, const ek_node *pat)
{
    const ek_type *vt = lower_row_type(c, m->u.bind.value);
    lower_gtype *g = tuple_repr_of(c, vt);
    size_t n, i;
    lower_val v;
    const char *t;

    if (!flat_tuple_pat(pat) || !g || !vt
        || pat->u.kids.count != g->nf) {
        return 0;
    }
    n = g->nf;
    v = lower_expr(c, scope, m->u.bind.value);
    t = lower_as_temp(c, scope, &v);
    for (i = 0; i < n; i++) {
        const ek_node *k = pat->u.kids.data[i];

        if (k->kind != EK_N_P_VAR) {
            continue; /* the wildcard component dies with the tuple */
        }
        {
            const char *name = k->u.path.data[0];
            const char *cname = lower_c_name(c, scope, name);

            lower_pad(c);
            lower_emitf(c, "ek_val %s = ek_keep(((ekS_%s *)%s.as.ref)"
                           "->f_%zu);\n", cname, g->stem, t, i);
            lower_scope_add(c, scope, name, cname, 0, NULL);
        }
    }
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", t);
    return 1;
}

