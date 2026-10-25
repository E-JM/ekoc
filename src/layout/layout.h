#ifndef EK_LAYOUT_LAYOUT_H
#define EK_LAYOUT_LAYOUT_H

#include <stddef.h>

#include "lex/lex.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * Stage 3 (Annex A.4): token stream -> layout token stream.
 * The algorithm derives block structure from the column of each line's
 * first token (Clause 5.2; the lexer counts columns in scalar values and
 * drops indentation spaces, so column - 1 is the line's indentation) and
 * inserts the virtual NEWLINE, INDENT, and DEDENT tokens the syntactic
 * grammar of Annex A.3 consumes. Source tokens pass through in order,
 * wrapped as EK_LTOK_SOURCE; the input NEWLINE and EOF tokens are
 * consumed and re-encoded. A tab in indentation is diagnosed by the
 * lexer (E-tab, Clause 5.2.3) and only flows through here.
 *
 * The stream always ends with EK_LTOK_EOF. Encoding: the encoding is
 * terminator-style — every significant line at bracket depth zero ends
 * with a NEWLINE marker (Arm, VariantLine, and ImportLine carry a
 * trailing NEWLINE in Annex A.3), except continuations, where no
 * NEWLINE is emitted at the end of the previous line (A.4 rules 6 and
 * 7) and the lines splice. INDENT follows the NEWLINE of the header
 * line (Suite := ':' NEWLINE INDENT ...); DEDENTs follow the NEWLINE of
 * the last member and precede the closing bracket that closes a
 * callback block (A.4 rule 4).
 *
 * Marker positions: NEWLINE sits at the line terminator it encodes;
 * INDENT at the first token of the member opening the block; DEDENT at
 * the first token of the line that triggered it, or at the closing
 * bracket, or at EOF; EK_LTOK_EOF at the input EOF.
 */

typedef enum {
    EK_LTOK_SOURCE = 0, /* lexical token, carried in tok */
    EK_LTOK_NEWLINE,    /* A.4 NEWLINE: ends a member line */
    EK_LTOK_INDENT,     /* A.4 INDENT: opens a block */
    EK_LTOK_DEDENT,     /* A.4 DEDENT: closes a block */
    EK_LTOK_EOF
} ek_layout_kind;

typedef struct {
    ek_layout_kind kind;
    int line; /* 1-based */
    int col;  /* 1-based, counted in Unicode scalar values */
    ek_token tok; /* kind == EK_LTOK_SOURCE: the lexical token */
} ek_layout_token;

typedef struct {
    ek_layout_token *tokens; /* arena-owned; NULL only on allocation failure */
    size_t count;            /* includes the trailing EK_LTOK_EOF */
} ek_layout_result;

/* Lays out count tokens from ek_lex. Errors go to diag, never abort;
   layout continues past recoverable errors and always produces a stream
   ending in EK_LTOK_EOF. file names the source in diagnostics and is
   borrowed. */
ek_layout_result ek_layout(const ek_token *tokens, size_t count, const char *file,
                           ek_arena *arena, ek_diag *diag);

#endif
