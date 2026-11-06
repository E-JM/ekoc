#ifndef EK_LAYOUT_LAYOUT_H
#define EK_LAYOUT_LAYOUT_H

#include <stddef.h>

#include "lex/lex.h"
#include "support/arena.h"
#include "support/diag.h"

/* Token stream -> layout token stream. */

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
