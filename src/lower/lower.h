#ifndef EK_LOWER_LOWER_H
#define EK_LOWER_LOWER_H

#include <stddef.h>

#include "check/check.h"
#include "resolve/resolve.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * Checked AST -> C source text.
 *
 * ek_lower_with is a pure entry point, (input, mode, arena, diag) ->
 * output; it never aborts. The output is one NUL-terminated C99
 * translation unit in the arena: the runtime include, the retention
 * helper, the generated struct layer of the constructed and read
 * declared types, a forward declaration per emitted function, the
 * function bodies, and the entry shim of the mode. Only top-level
 * bindings transitively reachable from the mode's roots are emitted;
 * fn_count counts them. A construct the lowerer does not cover is
 * diagnosed with code "E-lower" and skipped, so a failed run still
 * yields the text of the parts that lowered.
 */
typedef struct {
    char *text;      /* the translation unit, NUL-terminated, arena-owned */
    size_t len;      /* text length, terminator excluded */
    size_t fn_count; /* number of emitted functions */
} ek_lower_result;

/* What the unit is for; the choice fixes the roots and the entry
   shim. */
typedef enum {
    EK_LOWER_PROGRAM, /* one program: the roots start at the root
                         module's `main`; the shim calls it */
    EK_LOWER_TESTS    /* one test unit: the roots are the root module's
                         test bindings; the shim dispatches one test
                         per invocation by name */
} ek_lower_mode;

/* Lowers the checked root module for the mode. Diagnostics go to
   diag; a run with diagnostics still returns the best-effort text.
   The result borrows the arena. */
ek_lower_result ek_lower_with(ek_resolve_result resolved,
                              ek_check_result checked, ek_lower_mode mode,
                              ek_arena *arena, ek_diag *diag);

/* Lowers the checked root module as a program. Diagnostics go to
   diag; a run with diagnostics still returns the best-effort text.
   The result borrows the arena. */
ek_lower_result ek_lower(ek_resolve_result resolved, ek_check_result checked,
                         ek_arena *arena, ek_diag *diag);

#endif
