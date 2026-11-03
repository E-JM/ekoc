#include <string.h>

#include "lower/internal.h"

/* The parameter-borrow analysis: whether a parameter may stay borrowed
   for the whole body. Two scans share one child walk, parameterized by
   a mode and the written name: the rebinds scan asks whether the body
   rebinds the parameter -- the rebind releases the local's own
   reference before the new value moves in -- and the mentions scan
   asks whether the body names the parameter anywhere, so the prologue
   knows whether the C parameter stays used. */

/* One scan's subject: the written parameter name and how deep the walk
   reads. The deep scan (the mentions) enters lambdas -- a capture
   reads the slot -- and reads binding and pattern spellings; the
   shallow scan (the rebinds) stops at a lambda, whose body owns its
   scopes and cannot rebind a capture, and reads rebind patterns
   wherever a rebind can sit, including inside an arm body opened by a
   rebind's own value. */
typedef struct {
    const char *name;
    int deep;
} param_scan;

/* True when the node itself settles the scan for that name. The deep
   scan hits a read of the alias, a binding or pattern that spells the
   name, and treats every binding as a mention, so a body that shadows
   the name without reading the slot still counts as a use. The shallow
   scan hits a rebind whose pattern names the parameter; an initial
   binding of the name is a fresh local, not a rebind. */
static int param_hit(const ek_node *e, const param_scan *s)
{
    const ek_node *pat;

    if (!s->deep) {
        pat = e->kind == EK_N_REBIND ? e->u.bind.pattern : NULL;
        return pat && pat->kind == EK_N_P_VAR && pat->u.path.count == 1
            && strcmp(pat->u.path.data[0], s->name) == 0;
    }
    if (e->kind == EK_N_NAME && e->u.path.count == 1
        && strcmp(e->u.path.data[0], s->name) == 0) {
        return 1;
    }
    if (e->kind == EK_N_P_VAR && e->u.path.count == 1
        && strcmp(e->u.path.data[0], s->name) == 0) {
        return 1;
    }
    if (e->kind != EK_N_LET && e->kind != EK_N_REBIND) {
        return 0;
    }
    pat = e->u.bind.pattern;
    return pat && pat->kind == EK_N_P_VAR && pat->u.path.count == 1
        && strcmp(pat->u.path.data[0], s->name) == 0;
}

static int scan_param(const ek_node *e, const param_scan *s);

/* The kid lists one kind carries. */
static int scan_kids(ek_node *const *data, size_t count,
                     const param_scan *s)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (scan_param(data[i], s)) {
            return 1;
        }
    }
    return 0;
}

/* The two-child operators: either side may carry the scan. */
static int scan_pair(const ek_node *a, const ek_node *b,
                     const param_scan *s)
{
    return scan_param(a, s) || scan_param(b, s);
}

/* The binding forms: the value carries the scan in both modes. A
   rebind's value is an expression and an expression can open a case
   whose arm bodies hold rebinds of their own, so the shallow scan
   reads the rebind pattern AND walks the value; it still stops at a
   lambda, whose body cannot rebind a capture. */
static int scan_bind(const ek_node *e, const param_scan *s)
{
    return scan_param(e->u.bind.value, s);
}

/* The interpolated expressions of one string literal. */
static int scan_segs(const ek_node *e, const param_scan *s)
{
    size_t i;

    for (i = 0; i < e->u.str.count; i++) {
        if (e->u.str.segs[i].expr
            && scan_param(e->u.str.segs[i].expr, s)) {
            return 1;
        }
    }
    return 0;
}

/* The arms of one case: the deep scan reads the alternative patterns,
   both scans read the bodies. */
static int scan_arms(const ek_node *e, const param_scan *s)
{
    size_t i, k;

    for (i = 0; i < e->u.caseexpr.arms.count; i++) {
        const ek_node *arm = e->u.caseexpr.arms.data[i];

        if (s->deep) {
            for (k = 0; k < arm->u.arm.patterns.count; k++) {
                if (scan_param(arm->u.arm.patterns.data[k], s)) {
                    return 1;
                }
            }
        }
        if (scan_param(arm->u.arm.body, s)) {
            return 1;
        }
    }
    return 0;
}

/* The child walk both scans share: the children each kind carries, the
   namespace's value among them. */
static int scan_param(const ek_node *e, const param_scan *s)
{
    if (!e) {
        return 0;
    }
    if (param_hit(e, s)) {
        return 1;
    }
    switch (e->kind) {
    case EK_N_MODULE:
    case EK_N_BLOCK:
    case EK_N_COMPOSITE:
        return scan_kids(e->u.kids.data, e->u.kids.count, s);
    case EK_N_LET:
    case EK_N_REBIND:
    case EK_N_INVERT:
    case EK_N_NAMESPACE:
        return scan_bind(e, s);
    case EK_N_LAMBDA:
        return s->deep && scan_pair(e->u.pair.a, e->u.pair.b, s);
    case EK_N_IF:
        return scan_param(e->u.ifexpr.cond, s)
            || scan_param(e->u.ifexpr.then, s)
            || scan_param(e->u.ifexpr.els, s);
    case EK_N_CASE:
        return scan_kids(e->u.caseexpr.scrutinees.data,
                         e->u.caseexpr.scrutinees.count, s)
            || scan_arms(e, s);
    case EK_N_PAREN:
    case EK_N_UNARY:
        return scan_param(e->u.wrap.inner, s);
    case EK_N_BINARY:
        return scan_pair(e->u.binary.lhs, e->u.binary.rhs, s);
    case EK_N_FIELD:
        return scan_param(e->u.field.recv, s);
    case EK_N_CALL:
        return scan_param(e->u.call.callee, s)
            || scan_kids(e->u.call.args.data, e->u.call.args.count, s);
    case EK_N_APPLY:
        return scan_pair(e->u.pair.a, e->u.pair.b, s);
    case EK_N_COMPOSITE_ITEM:
        return scan_param(e->u.pair.b, s)
            || (s->deep && scan_param(e->u.pair.a, s));
    case EK_N_STRING_LIT:
        return scan_segs(e, s);
    default:
        return 0;
    }
}

/* True when the body rebinds the parameter of that written name. */
int lower_param_rebinds(const ek_node *e, const char *name)
{
    const param_scan s = { name, 0 };

    return scan_param(e, &s);
}

/* True when the body names the parameter anywhere. */
int lower_param_mentions(const ek_node *e, const char *name)
{
    const param_scan s = { name, 1 };

    return scan_param(e, &s);
}
