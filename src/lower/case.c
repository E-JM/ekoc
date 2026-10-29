#include <string.h>

#include "lower/internal.h"

/* The case form: the scrutinees evaluate once into borrowed temps,
   the arms run in order as a C do-block whose break carries the first
   match out, and a cascade that matches nothing panics — unreachable
   whenever check accepted the coverage. Arm bindings own keeps of the
   destructured slots and release with the arm; the scrutinee temps
   release once at the join. */

/* The arm body: its value moves into the case result, then the
   body's own bindings release. */
static void emit_arm_body(lower_ctx *c, lower_scope *arm_scope,
                          const ek_node *body, const char *r, int tail)
{
    lower_scope bs;
    lower_val v;
    int saved = c->in_tail;

    memset(&bs, 0, sizeof bs);
    bs.parent = arm_scope;
    c->in_tail = tail;
    v = lower_block_value(c, &bs, body);
    c->in_tail = saved;
    if (!v.text) {
        v = lower_val_unit(); /* the body tail jumped to retry */
    }
    lower_pad(c);
    lower_emitf(c, "%s = %s;\n", r, v.text);
    lower_release_scope(c, &bs);
}

/* The guarded body: only a guard that held leaves the cascade; a
   failed guard releases the bindings and the cascade goes on. */
static void emit_guarded_body(lower_ctx *c, lower_scope *arm_scope,
                              const ek_node *arm, const char *r, int tail,
                              size_t clean_base)
{
    const char *g = lower_temp(c, arm_scope);
    lower_val gv = lower_sub(c, arm_scope, arm->u.arm.guard);
    const char *gt = lower_as_temp(c, arm_scope, &gv);

    lower_pad(c);
    lower_emitf(c, "int %s = %s.as.i;\n", g, gt);
    lower_pad(c);
    lower_emitf(c, "ek_release(%s);\n", gt);
    lower_pad(c);
    lower_emitf(c, "if (%s) {\n", g);
    c->depth++;
    emit_arm_body(c, arm_scope, arm->u.arm.body, r, tail);
    lower_release_scope(c, arm_scope);
    lower_clean_release(c, clean_base);
    c->nclean = clean_base;
    lower_pad(c);
    lower_emitf(c, "break;\n");
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_release_scope(c, arm_scope);
}

/* One arm: the test cascade over the scrutinee temps, the guard, and
   the body. The arm's temps — the literal hoists, the list-walk
   cursors, and the pattern bindings — release on every exit. */
static void emit_arm(lower_ctx *c, lower_scope *outer, const ek_node *arm,
                     const char **s, size_t nsc, const char *r, int tail)
{
    lower_scope arm_scope;
    lower_pat pat;
    size_t clean_base = c->nclean;
    size_t k;

    memset(&arm_scope, 0, sizeof arm_scope);
    arm_scope.parent = outer;
    lower_pat_init(c, &pat, &arm_scope);
    lower_pad(c);
    lower_emitf(c, "{\n");
    c->depth++;
    lower_pad(c);
    lower_emitf(c, "int %s = 1;\n", pat.m);
    for (k = 0; nsc && k < arm->u.arm.patterns.count; k++) {
        size_t col = k < nsc ? k : nsc - 1;

        lower_pat_walk(c, &pat, arm->u.arm.patterns.data[k], s[col]);
    }
    lower_pat_emit(c, &pat);
    lower_pad(c);
    lower_emitf(c, "if (%s) {\n", pat.m);
    c->depth++;
    lower_pat_assign(c, &pat);
    if (arm->u.arm.guard) {
        emit_guarded_body(c, &arm_scope, arm, r, tail, clean_base);
    } else {
        emit_arm_body(c, &arm_scope, arm->u.arm.body, r, tail);
        lower_release_scope(c, &arm_scope);
        lower_clean_release(c, clean_base);
        c->nclean = clean_base;
        lower_pad(c);
        lower_emitf(c, "break;\n");
    }
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_clean_release(c, clean_base);
    c->nclean = clean_base;
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "}\n");
    lower_pat_done(c, &pat);
}

/* The case form over its scrutinees. */
lower_val lower_case(lower_ctx *c, lower_scope *scope, const ek_node *e)
{
    size_t nsc = e->u.caseexpr.scrutinees.count;
    const char **s = NULL;
    size_t cap = 0;
    size_t clean_base = c->nclean;
    const char *r;
    size_t i;
    int saved = c->in_tail;

    if (nsc) {
        s = lower_grow(c, NULL, &cap, nsc, sizeof *s);
        if (!s) {
            return lower_val_unit();
        }
    }
    for (i = 0; i < nsc; i++) {
        lower_val v = lower_sub(c, scope, e->u.caseexpr.scrutinees.data[i]);

        s[i] = lower_as_temp(c, scope, &v);
        lower_clean_push(c, s[i]);
    }
    r = lower_temp(c, scope);
    lower_pad(c);
    lower_emitf(c, "ek_val %s = ek_val_unit();\n", r);
    lower_pad(c);
    lower_emitf(c, "do {\n");
    c->depth++;
    for (i = 0; i < e->u.caseexpr.arms.count; i++) {
        emit_arm(c, scope, e->u.caseexpr.arms.data[i], s, nsc, r, saved);
    }
    lower_pad(c);
    lower_emitf(c, "ek_panic(\"P-panic\", \"no matching case arm\");\n");
    c->depth--;
    lower_pad(c);
    lower_emitf(c, "} while (0);\n");
    for (i = nsc; i-- > 0;) {
        lower_pad(c);
        lower_emitf(c, "ek_release(%s);\n", s[i]);
    }
    c->nclean = clean_base;
    return (lower_val){ r, 1 };
}
