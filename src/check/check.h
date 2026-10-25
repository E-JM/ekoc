#ifndef EK_CHECK_CHECK_H
#define EK_CHECK_CHECK_H

#include <stddef.h>

#include "parse/parse.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * Stage 6 (Clauses 7.5-7.8, plus the 9.8/5.7 parts of the M6a negative
 * list): resolved AST -> types. ek_check is a pure entry point,
 * (input, arena, diag) -> output; it never aborts (ARCHITECTURE rule 4).
 *
 * The input is the resolve result. ek_check checks the compilation
 * ROOT's module body against the closure's interfaces (Clause 7.3.2);
 * the other modules of the closure are checked when they are roots of
 * their own ek_check calls, exactly as the conformance suite runs
 * them. Imported surfaces supply the signatures references need; the
 * AST behind an imported signature (symbol->type, symbol->decl) stays
 * alive in the arena and is readable.
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
 * (M7) can binary search by node. `type` is a closed snapshot: bound
 * inference variables are substituted away; a remaining EK_T_IVAR is
 * one the ruling above leaves undetermined. `reading` is the selected
 * station of Clause 7.4.3 for member links, EK_RD_NONE otherwise.
 * `bit` is reserved for M6b (Clause 7.9 world bits) and is always
 * EK_BIT_UNSET in this milestone.
 */

typedef enum {
    EK_T_UNIT,    /* `_` (Clause 7.5.4) */
    EK_T_NEVER,   /* `Never`, the bottom type */
    EK_T_NOMINAL, /* declared type, fully applied */
    EK_T_LIST,    /* `[]t` */
    EK_T_DICT,    /* `[k]v` */
    EK_T_TUPLE,   /* `[a, b, ...]` */
    EK_T_RECORD,  /* `[f: a, ...]` */
    EK_T_FN,      /* `(params) arrow result` */
    EK_T_TVAR,    /* rigid variable of an annotation (Clause 7.6.1) */
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
            const ek_tfield *fields; /* sorted by name (7.5.7: field
                                        order is not part of equality) */
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
    EK_RD_FIELD,        /* station 1: record field */
    EK_RD_NAMESPACE,    /* station 2: declared type namespace member */
    EK_RD_MODULE_FN,    /* station 3: current-module free function */
    EK_RD_DERIVED,      /* station 2: derived service (Clause 9.8.7) */
    EK_RD_EXTERNAL,     /* the placeholder-std opaque reading */
    EK_RD_UNKNOWN       /* receiver type undetermined (opacity) */
} ek_reading;

/* The world-bit slots of Clause 7.9. M6b fills them; M6a leaves every
   row at EK_BIT_UNSET. */
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
    ek_bit bit;            /* reserved for M6b; EK_BIT_UNSET here */
} ek_ck_row;

typedef struct {
    ek_ck_row *rows; /* sorted by node address */
    size_t count;
} ek_check_result;

/* Checks the root module of the resolve result (Clause 7.5-7.8).
   Diagnostics go to diag; a failed check still returns the partial
   table. The result borrows the arena. */
ek_check_result ek_check(ek_resolve_result in, ek_arena *arena,
                         ek_diag *diag);

#endif
