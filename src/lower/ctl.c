#include <string.h>

#include "lower/internal.h"

/* The if form: the condition and the result are temps of the enclosing
   block, the branches are C blocks of their own, and the result temp
   carries the taken branch's value. */

/* One if branch: its body lowers into a fresh block whose bindings
   release before the brace closes, and the branch value moves into the
   result temp. */
static void emit_branch(lower_ctx *c, lower_scope *parent, const ek_node *body,
                        const char *r)
{
    lower_scope s;
    lower_val v;

    memset(&s, 0, sizeof s);
    s.parent = parent;
    v = lower_block_value(c, &s, body);
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", r, v.text);
    lower_release_scope(c, &s);
}

/* The if form: the condition and the result are temps of the enclosing
   block, the branches are C blocks, and the result temp carries the
   taken branch's value. */
void lower_if(lower_ctx *c, lower_scope *scope, const ek_node *e,
              lower_val *out)
{
    unsigned d = c->depth;
    lower_val cond = lower_expr(c, scope, e->u.ifexpr.cond);
    const char *ct = lower_as_temp(c, scope, &cond);
    const char *r = lower_temp(c, scope);

    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_unit();\n", r);
    lower_pad(c);
    lower_emitf(c, "if (%s.as.i) {\n", ct);
    c->depth = d + 1;
    emit_branch(c, scope, e->u.ifexpr.then, r);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "} else {\n");
    c->depth = d + 1;
    emit_branch(c, scope, e->u.ifexpr.els, r);
    c->depth = d;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", ct);
    out->text = r;
    out->is_temp = 1;
}

