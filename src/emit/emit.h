#ifndef EK_EMIT_EMIT_H
#define EK_EMIT_EMIT_H

#include "support/arena.h"
#include "support/diag.h"

/*
 * C source text -> executable.
 *
 * ek_emit_aot writes the translation unit to "<out_path>.c" (overwriting
 * any earlier file) and then invokes the system C compiler to link an
 * executable at out_path:
 *
 *     cc -std=c99 -O2 -I<tree_root>/rt <out_path>.c
 *        <tree_root>/build/libekort.a -lm -o <out_path>
 *
 * tree_root is the directory holding the compiler's own rt/ and build/
 * tree. The command line is built without quoting, so paths containing
 * spaces are not supported yet.
 *
 * The function never aborts. Every failure -- a missing translation
 * unit, a write failure, missing runtime bits, a compiler error -- is
 * diagnosed with code "E-emit" at out_path and leaves ok at 0. The
 * diagnostic message carries the compiler's output with runs of
 * whitespace collapsed to one space; an output too long for one message
 * keeps the first line and the tail.
 */
typedef struct {
    int ok; /* 1 when the executable was produced */
} ek_emit_result;

/* Emits and links one executable. c_text is a NUL-terminated C99
   translation unit; out_path and tree_root are borrowed and must
   outlive the call. Diagnostics go to diag. */
ek_emit_result ek_emit_aot(const char *c_text, const char *out_path,
                           const char *tree_root, ek_arena *arena,
                           ek_diag *diag);

#endif
