#ifndef EK_LAYOUT_LAYOUT_H
#define EK_LAYOUT_LAYOUT_H

#include <stddef.h>

#include "lex/lex.h"
#include "support/arena.h"
#include "support/diag.h"

/*
 * Token stream -> layout token stream. Block structure is derived from
 * the column of each line's first token (the lexer counts columns in
 * scalar values and drops indentation spaces, so column - 1 is the
 * line's indentation), and the virtual NEWLINE, INDENT, and DEDENT
 * tokens the parser needs are inserted. Source tokens pass through in
 * order, wrapped as EK_LTOK_SOURCE; the input NEWLINE and EOF tokens
 * are consumed and re-encoded. A tab in indentation is diagnosed by
 * the lexer and only flows through here.
 *
 * The stream always ends with EK_LTOK_EOF. Encoding is terminator
 * style: every significant line at bracket depth zero ends with a
 * NEWLINE marker, except continuations, where no NEWLINE is emitted at
 * the end of the previous line and the lines splice. INDENT follows
 * the NEWLINE of the header line; a stack block's DEDENTs follow the
 * last member's NEWLINE, and a callback block's DEDENTs precede the
 * closing bracket that closes it.
 *
 * Marker positions: NEWLINE sits at the line terminator it encodes;
 * INDENT at the first token of the member opening the block; DEDENT at
 * the first token of the line that triggered it, or at the closing
 * bracket, or at EOF; EK_LTOK_EOF at the input EOF.
 *
 * Documentation tokens are trivia. A run of documentation lines is
 * held back until the next line that can carry a binding; the run is
 * then emitted after that line's INDENT/DEDENT markers and before its
 * source tokens, so the parser sees one document immediately before
 * the binding. The documentation lines contribute no NEWLINE of their
 * own. A blank line or an ordinary comment line ends an unattached
 * run, and a run with no binding line after it is dropped.
 */

typedef enum {
    EK_LTOK_SOURCE = 0, /* lexical token, carried in tok */
    EK_LTOK_NEWLINE,    /* virtual NEWLINE: ends a member line */
    EK_LTOK_INDENT,     /* virtual INDENT: opens a block */
    EK_LTOK_DEDENT,     /* virtual DEDENT: closes a block */
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
