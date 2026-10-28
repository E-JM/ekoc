#ifndef EK_LEX_INTERNAL_H
#define EK_LEX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "lex/lex.h"
#include "support/arena.h"
#include "support/diag.h"
#include "support/utf8.h"
#include "support/xid.h"

/* Guard against stack exhaustion on pathologically nested slots. */
#define EK_LEX_MAX_SLOT_DEPTH 100

/* Growable byte buffer on the arena; abandoned blocks are reclaimed
   wholesale when the arena is freed. */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
    ek_arena *arena;
} gbuf;

void gbuf_init(gbuf *b, ek_arena *arena);
int gbuf_put(gbuf *b, const void *bytes, size_t n);
int gbuf_char(gbuf *b, char c);

typedef struct lex_ctx {
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int col;
    const char *file;
    ek_arena *arena;
    ek_diag *diag;
    int depth; /* open interpolation slots, for EK_LEX_MAX_SLOT_DEPTH */
    int oom;   /* stop scanning after the first allocation failure */
} lex_ctx;

/* lex.c */
typedef int (*ek_scan_fn)(lex_ctx *c, gbuf *out);
int ctx_byte(const lex_ctx *c, size_t off);
void ctx_error(lex_ctx *c, const char *code, int line, int col,
               const char *fmt, ...);
void ctx_oom(lex_ctx *c);
char *ctx_slice(lex_ctx *c, size_t start, size_t end);
void lex_skip_line_comment(lex_ctx *c);
int lex_consume_cr(lex_ctx *c);
int lex_token(lex_ctx *c, gbuf *out);
int lex_step(lex_ctx *c, gbuf *out, ek_scan_fn scan);
int emit_token(lex_ctx *c, gbuf *out, const ek_token *t);

/* escape.c */
typedef enum { ESC_OK = 0, ESC_ERROR, ESC_END, ESC_STOP } esc_result;
/* ESC_END means the backslash sits at end of line or end of file; the
   caller reports its own unterminated-literal error. ESC_STOP means an
   escape ran into the line terminator or end of input and is already
   diagnosed, so the literal ends there owing no further diagnostic. */
esc_result scan_escape(lex_ctx *c, int in_char, uint32_t *out);

/* ident.c */
int lex_identifier(lex_ctx *c, gbuf *out);

/* number.c */
int lex_number(lex_ctx *c, gbuf *out);

/* operator.c */
int lex_operator(lex_ctx *c, gbuf *out);

/* charlit.c */
int lex_char_literal(lex_ctx *c, gbuf *out);

/* string.c */
int lex_string_literal(lex_ctx *c, gbuf *out);

#endif
