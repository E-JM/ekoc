#ifndef EK_LEX_LEX_H
#define EK_LEX_LEX_H

#include <stddef.h>
#include <stdint.h>

#include "support/arena.h"
#include "support/diag.h"

/* Source bytes -> token stream. The stream ends with EOF; line
   terminators survive as NEWLINE tokens for the layout stage. A `///`
   line yields one DOC token before its NEWLINE; plain `//` comments
   are skipped. */

typedef enum {
    EK_TOK_IDENT = 0,
    EK_TOK_INT,
    EK_TOK_FLOAT,
    EK_TOK_CHAR,
    EK_TOK_STRING,
    EK_TOK_KW_IMPORT,
    EK_TOK_KW_CASE,
    EK_TOK_KW_IF,
    EK_TOK_KW_ELSE,
    /* operators */
    EK_TOK_PLUS,
    EK_TOK_MINUS,
    EK_TOK_STAR,
    EK_TOK_SLASH,
    EK_TOK_PERCENT,
    EK_TOK_PLUSPLUS,
    EK_TOK_EQEQ,
    EK_TOK_BANGEQ,
    EK_TOK_LT,
    EK_TOK_LTEQ,
    EK_TOK_GT,
    EK_TOK_GTEQ,
    EK_TOK_ANDAND,
    EK_TOK_OROR,
    EK_TOK_BANG,
    EK_TOK_QUESTION,
    EK_TOK_ARROW,        /* ->  */
    EK_TOK_LARROW,       /* <-  */
    EK_TOK_BANGARROW,    /* !-> */
    EK_TOK_QUESTIONARROW,/* ?-> */
    EK_TOK_DOTDOT,
    EK_TOK_DOTDOTEQ,
    /* punctuation */
    EK_TOK_LPAREN,
    EK_TOK_RPAREN,
    EK_TOK_LBRACKET,
    EK_TOK_RBRACKET,
    EK_TOK_LBRACE,
    EK_TOK_RBRACE,
    EK_TOK_COMMA,
    EK_TOK_COLON,
    EK_TOK_DOT,
    EK_TOK_EQ,
    EK_TOK_COLONEQ,
    EK_TOK_PIPE,
    EK_TOK_SEMI,
    EK_TOK_DOC,     /* documentation comment: text is the line after /// */
    EK_TOK_NEWLINE,
    EK_TOK_EOF
} ek_token_kind;

/* One member of a string literal: a cooked literal segment or the token
   run of one interpolation slot, in source order. Exactly one of text
   and tokens is non-NULL. */
typedef struct ek_string_part {
    const char *text;        /* cooked UTF-8 bytes of a literal segment */
    size_t text_len;
    struct ek_token *tokens; /* token run of one slot; no NEWLINE, no EOF */
    size_t token_count;
} ek_string_part;

typedef struct ek_token {
    ek_token_kind kind;
    int line; /* 1-based */
    int col;  /* 1-based, counted in Unicode scalar values */
    /* EK_TOK_IDENT, EK_TOK_INT, EK_TOK_FLOAT: raw source spelling,
       NUL-terminated, arena-owned.
       EK_TOK_DOC: the comment's line after `///`, one leading space
       removed, NUL-terminated and arena-owned. */
    const char *text;
    size_t len;
    /* EK_TOK_INT: the mathematical value. magnitude is the value modulo
       2^64; overflow is 1 when the value does not fit in 64 bits, so
       the check stage must recover it from text. base is 2, 10 or 16.
       E-literal-overflow is never diagnosed here. */
    unsigned long long magnitude;
    int overflow;
    int base;
    double fvalue;   /* EK_TOK_FLOAT: value of the cleaned spelling */
    uint32_t scalar; /* EK_TOK_CHAR: the one cooked scalar value */
    /* EK_TOK_STRING: the parts; NULL and 0 for the empty string */
    ek_string_part *parts;
    size_t part_count;
} ek_token;

typedef struct {
    ek_token *tokens; /* arena-owned; NULL only on allocation failure */
    size_t count;     /* includes the trailing EK_TOK_EOF */
} ek_lex_result;

/* Lexes length bytes of UTF-8 source. Errors go to diag, never abort;
   lexing continues past recoverable errors. file names the source in
   diagnostics and is borrowed. */
ek_lex_result ek_lex(const char *source, size_t length, const char *file,
                     ek_arena *arena, ek_diag *diag);

#endif
