#ifndef EK_CHECK_CHECK_H
#define EK_CHECK_CHECK_H

#include <stddef.h>

#include "parse/parse.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * Resolved AST -> types. ek_check is a pure entry point, (input,
 * arena, diag) -> output; it never aborts.
 *
 * The input is the resolve result. ek_check checks the compilation
 * ROOT's module body against the closure's interfaces; the other
 * modules of the closure are checked when they are roots of their own
 * ek_check calls. Imported surfaces supply the signatures references
 * need; the AST behind an imported signature (symbol->type,
 * symbol->decl) stays alive in the arena and is readable.
 *
 * PLACEHOLDER STD BRIDGE (resolve.h): a reference resolve answered
 * with EK_SYM_EXTERNAL has an unknown-but-fixed type. Each external
 * reference gets a fresh inference variable at its use site (fresh per
 * reference, so occurrence-wise monomorphic); unification binds these
 * variables freely, and no diagnostic ever arises from their opacity.
 * A member link whose reading fails on a known receiver type (the
 * unknown std namespace members) takes the same opaque reading.
 *
 * Output: one row per typed AST node, sorted by node address, so lower
 * can binary search by node. `type` is a closed snapshot: bound
 * inference variables are substituted away; a remaining EK_T_IVAR is
 * one the bridge leaves undetermined. `reading` is the selected
 * receiver reading for a member link, EK_RD_NONE otherwise.
 *
 * `bit` is the node's world classification. A CALL row carries the bit
 * of the call itself: pure through `->`, world-touching through
 * `!->`, and through `?->` the instantiation from the feeder
 * arguments; inside the body of a declaration whose signature contains
 * the open bit, a call through a `?` arrow carries that bit. A LAMBDA
 * row carries the bit derived from its body. An annotated binding row
 * (LET/REBIND/NAMESPACE over a function annotation) carries the
 * declared arrow's bit, open included. Every other row carries the OR
 * of the call bits in its subtree, lambda interiors excluded: only
 * calls contribute, so a reference to a function value is pure. PURE
 * where nothing contributes.
 *
 * Test bindings have no arrow and nothing to reconcile; their rows
 * carry derived bits all the same, and no bit diagnostic fires inside
 * them.
 *
 * OPACITY RULE for the placeholder-std bridge: a call's bit is fixed
 * only through known arrows ("through `->` it is pure; through `!->`
 * it is world-touching"); an unknown callee is not covered. A call
 * through an unknown callee contributes world-touching when its
 * receiver has a capability type other than `Conc`, which sets no bit,
 * and the member is not one of the pure derivation combinators
 * (Out.null/tee, In.empty/from_lines, Fs.null/root, Env/Args/
 * Clock.frozen, Rand.with_seed, Task.join/cancel), or when an argument
 * is a lambda whose body is world-touching (the open-bit
 * instantiation applied at opaque dispatchers); it contributes pure
 * otherwise. No diagnostic ever arises from the opacity itself: an
 * unknown callee is never rejected, it only feeds the OR that an
 * annotated binding reconciles against.
 */

typedef enum {
    EK_T_UNIT,    /* `_` */
    EK_T_NEVER,   /* `Never`, the bottom type */
    EK_T_NOMINAL, /* declared type, fully applied */
    EK_T_LIST,    /* `[]t` */
    EK_T_DICT,    /* `[k]v` */
    EK_T_TUPLE,   /* `[a, b, ...]` */
    EK_T_RECORD,  /* `[f: a, ...]` */
    EK_T_FN,      /* `(params) arrow result` */
    EK_T_TVAR,    /* rigid variable of an annotation */
    EK_T_IVAR     /* inference variable (unification) */
} ek_type_kind;

typedef struct ek_type ek_type;

typedef struct {
    const char *name;
    const ek_type *type;
} ek_tfield;

struct ek_type {
    ek_type_kind kind;
    union {
        struct {
            const ek_symbol *sym; /* the declared type's symbol; a std
                                     placeholder symbol marks opacity */
            const ek_type **args; /* one per binder, in order */
            size_t argc;
        } nominal;
        struct { const ek_type *elem; } list;
        struct { const ek_type *key; const ek_type *val; } dict;
        struct { const ek_type **items; size_t count; } tuple;
        struct {
            const ek_tfield *fields; /* sorted by name: field order is
                                        not part of equality */
            size_t count;
        } record;
        struct {
            const ek_type **params;
            size_t count;
            const ek_type *result;
            ek_arrow arrow;
        } fn;
        struct { const char *name; } tvar; /* identity is the node */
        struct { unsigned id; const ek_type *bound; } ivar;
    } u;
};

/* The receiver-position reading selected for a member link. */
typedef enum {
    EK_RD_NONE = 0,     /* not a member link */
    EK_RD_FIELD,        /* record field */
    EK_RD_NAMESPACE,    /* declared type namespace member */
    EK_RD_MODULE_FN,    /* current-module free function */
    EK_RD_DERIVED,      /* derived service */
    EK_RD_EXTERNAL,     /* the placeholder-std opaque reading */
    EK_RD_UNKNOWN       /* receiver type undetermined (opacity) */
} ek_reading;

/* The world-bit slots. EK_BIT_UNSET remains only where a failed run
   produced no row content. */
typedef enum {
    EK_BIT_UNSET = -1,
    EK_BIT_PURE = 0,
    EK_BIT_WORLD = 1,
    EK_BIT_OPEN = 2
} ek_bit;

typedef struct {
    const ek_node *node;
    const ek_type *type;   /* NULL where the node carries no type */
    ek_reading reading;    /* selected reading of a member link */
    ek_bit bit;            /* the node's world bit */
} ek_ck_row;

typedef struct {
    ek_ck_row *rows; /* sorted by node address */
    size_t count;
} ek_check_result;

/* Checks the root module of the resolve result. Diagnostics go to
   diag; a failed check still returns the partial table. The result
   borrows the arena. */
ek_check_result ek_check(ek_resolve_result in, ek_arena *arena,
                         ek_diag *diag);

#endif
