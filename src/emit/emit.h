#ifndef EK_EMIT_EMIT_H
#define EK_EMIT_EMIT_H

#include "support/arena.h"
#include "support/diag.h"

/* C source text -> executable. */
typedef struct {
    int ok; /* 1 when the executable was produced */
} ek_emit_result;

/* Emits and links one executable. c_text is a NUL-terminated C99
   translation unit; out_path and tree_root are borrowed and must
   outlive the call. Diagnostics go to diag. */
ek_emit_result ek_emit_aot(const char *c_text, const char *out_path,
                           const char *tree_root, const char *extra,
                           ek_arena *arena, ek_diag *diag);

#endif
