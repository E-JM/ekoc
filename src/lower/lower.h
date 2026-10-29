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
 * ek_lower is a pure entry point, (input, arena, diag) -> output; it
 * never aborts. The output is one NUL-terminated C99 translation unit
 * in the arena: the runtime include, the retention helper, a forward
 * declaration per emitted function, the function bodies, and the entry
 * shim when the root module declares `main`. Only top-level function
 * bindings transitively reachable from `main` are emitted; fn_count
 * counts them. A construct the lowerer does not cover is diagnosed
 * with code "E-lower" and skipped, so a failed run still yields the
 * text of the parts that lowered.
 */
typedef struct {
    char *text;      /* the translation unit, NUL-terminated, arena-owned */
    size_t len;      /* text length, terminator excluded */
    size_t fn_count; /* number of emitted functions */
} ek_lower_result;

/* Lowers the checked root module. Diagnostics go to diag; a run with
   diagnostics still returns the best-effort text. The result borrows
   the arena. */
ek_lower_result ek_lower(ek_resolve_result resolved, ek_check_result checked,
                         ek_arena *arena, ek_diag *diag);

#endif
