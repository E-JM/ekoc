#ifndef EK_LAYOUT_INTERNAL_H
#define EK_LAYOUT_INTERNAL_H

#include "layout/layout.h"

/* One physical line. Blank and ordinary comment-only lines keep their
   terminator but carry no tokens: the lexer drops their text, so a
   token-free line is insignificant. A documentation line carries DOC
   tokens only; it is trivia too. */
typedef struct {
    const ek_token *tok; /* tokens of the line, terminator excluded */
    size_t count;
    const ek_token *nl; /* terminating NEWLINE; NULL when the file ends the line */
} line_run;

/* One open callback block. Callback blocks live inside brackets and
   are closed by the call's closing bracket or by a line at or left of
   the header column. */
typedef struct {
    int header_col;   /* column of the header line's first token */
    int header_depth; /* bracket depth at the header line's end */
    int member_col;   /* column fixed by the first member; 0 = none yet */
    int opened;       /* NEWLINE INDENT emitted, so a DEDENT is owed */
} cb_block;

typedef struct {
    ek_layout_token *data;
    size_t len;
    size_t cap;
} tok_buf;

typedef struct {
    const char *file;
    ek_arena *arena;
    ek_diag *diag;
    int oom;
    tok_buf out;
    const ek_token *eof; /* input EOF token, for end-of-file positions */
    /* main indentation stack; bottom entry 0 is never popped */
    int *cols;
    int *hdr; /* header column of each open block; -1 when unknown */
    size_t col_count;
    size_t col_cap;
    int expect_block; /* depth-zero line ended in a block terminator */
    int header_col;    /* column of the line that set expect_block */
    int last_if_col;   /* column of the last if line; -1 when none */
    int case_pending;  /* a case scrutinee is still open */
    int seen;          /* a significant line was processed */
    int stray_base;    /* block column below an ignored indent; -1 when none */
    const line_run *prev; /* previous significant line */
    /* Documents waiting for the next binding line. */
    const ek_token **docs;
    size_t doc_count;
    size_t doc_cap;
    int after_break; /* the previous line ended at a member boundary */
    int chain_col;    /* column of the chain's first '.'; -1 when none */
    int depth;        /* open ( [ { at the current point */
    cb_block *cb;
    size_t cb_count;
    size_t cb_cap;
} layout_ctx;

/* layout.c: emission and arena growth. Emission is best effort; a host
   allocation failure costs one diagnostic and the rest is dropped. */
void lo_oom(layout_ctx *c);
void lo_emit(layout_ctx *c, ek_layout_kind kind, int line, int col);
void lo_emit_source(layout_ctx *c, const ek_token *t);
void lo_push_col(layout_ctx *c, int col, int hdr);
void lo_push_cb(layout_ctx *c, int header_col, int header_depth);
void lo_push_doc(layout_ctx *c, const ek_token *t);
void lo_clear_docs(layout_ctx *c);
void lo_flush_docs(layout_ctx *c);

/* process.c: the layout state machine over the line table. */
void lo_process(layout_ctx *c, const line_run *lines, size_t count);

#endif
