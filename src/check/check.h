#ifndef EK_CHECK_CHECK_H
#define EK_CHECK_CHECK_H

#include <stddef.h>

#include "parse/parse.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/diag.h"

/* Resolved AST -> types. */

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
            const size_t *decl; /* the canonical index of each declared
                                   field, the order the display reads */
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
    EK_RD_CONVERT,      /* the payload type's name over a newtype */
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
