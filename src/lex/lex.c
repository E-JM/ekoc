#include <stdarg.h>
#include <string.h>

#include "lex/internal.h"

void gbuf_init(gbuf *b, ek_arena *arena)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->arena = arena;
}

static int gbuf_reserve(gbuf *b, size_t need)
{
    size_t cap;
    char *p;

    if (b->len + need <= b->cap) {
        return 1;
    }
    cap = b->cap ? b->cap : 64;
    while (cap < b->len + need) {
        cap *= 2;
    }
    p = ek_arena_alloc(b->arena, cap);
    if (!p) {
        return 0;
    }
    if (b->len > 0) {
        memcpy(p, b->data, b->len);
    }
    b->data = p;
    b->cap = cap;
    return 1;
}

int gbuf_put(gbuf *b, const void *bytes, size_t n)
{
    if (!gbuf_reserve(b, n)) {
        return 0;
    }
    memcpy(b->data + b->len, bytes, n);
    b->len += n;
    return 1;
}

int gbuf_char(gbuf *b, char c)
{
    return gbuf_put(b, &c, 1);
}

int ctx_byte(const lex_ctx *c, size_t off)
{
    if (c->pos + off >= c->len) {
        return -1;
    }
    return (unsigned char)c->src[c->pos + off];
}

void ctx_error(lex_ctx *c, const char *code, int line, int col,
               const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    ek_diag_emitv(c->diag, EK_SEV_ERROR, code, c->file, line, col, fmt, ap);
    va_end(ap);
}

void ctx_oom(lex_ctx *c)
{
    if (!c->oom) {
        c->oom = 1;
        ek_diag_emit(c->diag, EK_SEV_ERROR, NULL, c->file, c->line, c->col,
                     "out of memory while lexing; results are partial");
    }
}

char *ctx_slice(lex_ctx *c, size_t start, size_t end)
{
    char *text = ek_arena_alloc(c->arena, end - start + 1);

    if (!text) {
        ctx_oom(c);
        return NULL;
    }
    memcpy(text, c->src + start, end - start);
    text[end - start] = '\0';
    return text;
}

/* Skips one scalar, reporting encoding faults; comments scan through
   here too, so faults inside them are diagnosed as well. Returns 0 at
   end of input. */
static int ctx_skip_checked(lex_ctx *c)
{
    ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos, c->len - c->pos);

    if (d.len == 0) {
        return 0;
    }
    if (d.status != EK_UTF8_OK) {
        ctx_error(c, "E-encoding", c->line, c->col, "source is not valid UTF-8");
    } else if (d.cp == 0xFEFF && c->pos != 0) {
        ctx_error(c, "E-stray-bom", c->line, c->col,
                  "byte order mark outside the start of the file");
    }
    c->pos += d.len;
    c->col++;
    return 1;
}

void lex_skip_line_comment(lex_ctx *c)
{
    c->pos += 2;
    c->col += 2;
    while (ctx_byte(c, 0) >= 0) {
        int b = ctx_byte(c, 0);

        if (b == '\n' || b == '\r') {
            return;
        }
        ctx_skip_checked(c);
    }
}

/* One documentation comment: the line after `///` becomes a DOC token
   and the line break itself still reaches the layout stage. The text
   keeps the comment's bytes, minus the three slashes and one leading
   space. */
static int lex_doc_comment(lex_ctx *c, gbuf *out)
{
    size_t start;
    ek_token t;
    char *text;

    memset(&t, 0, sizeof t);
    t.kind = EK_TOK_DOC;
    t.line = c->line;
    t.col = c->col;
    c->pos += 3;
    c->col += 3;
    start = c->pos;
    if (ctx_byte(c, 0) == ' ') {
        start++;
        c->pos++;
        c->col++;
    }
    while (ctx_byte(c, 0) >= 0) {
        int b = ctx_byte(c, 0);

        if (b == '\n' || b == '\r') {
            break;
        }
        ctx_skip_checked(c);
    }
    text = ctx_slice(c, start, c->pos);
    if (!text) {
        return 0;
    }
    t.text = text;
    t.len = c->pos - start;
    return emit_token(c, out, &t);
}

/* Consumes one '\r'. Returns 1 when a line terminator
   follows and the literal scanning the CR must end, 0 for a stray CR,
   diagnosed here. */
int lex_consume_cr(lex_ctx *c)
{
    int line = c->line, col = c->col;

    c->pos++;
    if (ctx_byte(c, 0) == '\n') {
        return 1;
    }
    ctx_error(c, "E-stray-cr", line, col,
              "carriage return outside a line terminator");
    return 0;
}

int emit_token(lex_ctx *c, gbuf *out, const ek_token *t)
{
    if (!gbuf_put(out, t, sizeof *t)) {
        ctx_oom(c);
        return 0;
    }
    return 1;
}

/* One dispatch step of the main loop, with the progress guard. A
   scanner that consumes nothing would spin the loop forever, so a stall
   costs one diagnostic and one skipped byte. The scanner is injected
   so the unit suite can feed the guard a stalling scanner. */
int lex_step(lex_ctx *c, gbuf *out, ek_scan_fn scan)
{
    size_t before = c->pos;

    if (!scan(c, out)) {
        return 0;
    }
    if (c->pos == before) {
        ctx_error(c, "E-lex-stall", c->line, c->col,
                  "scanner consumed nothing at this position");
        c->pos++;
        c->col++;
    }
    return 1;
}

/* One decoded scalar at the token position: BOM, identifier start, or
   a character that cannot begin a token. */
static int lex_unicode(lex_ctx *c, gbuf *out)
{
    ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos, c->len - c->pos);

    if (d.len == 0) {
        return 1; /* unreachable: callers check for end of input */
    }
    if (d.status != EK_UTF8_OK) {
        ctx_error(c, "E-encoding", c->line, c->col, "source is not valid UTF-8");
        c->pos += d.len;
        c->col++;
        return 1;
    }
    if (d.cp == 0xFEFF) {
        if (c->pos != 0) {
            ctx_error(c, "E-stray-bom", c->line, c->col,
                      "byte order mark outside the start of the file");
        }
        c->pos += d.len;
        return 1; /* an ignored mark takes no column */
    }
    if (d.cp < 0x80) {
        if (lex_operator(c, out)) {
            return 1;
        }
        ctx_error(c, "E-character", c->line, c->col,
                  "character cannot begin a token");
        c->pos++;
        c->col++;
        return 1;
    }
    if (ek_xid_start(d.cp)) {
        return lex_identifier(c, out);
    }
    ctx_error(c, "E-character", c->line, c->col,
              "character cannot begin a token");
    c->pos += d.len;
    c->col++;
    return 1;
}

/* Lexes one token from a non-whitespace position; also handles the
   characters that cannot begin any token. Returns 0 only when the
   arena is exhausted. */
int lex_token(lex_ctx *c, gbuf *out)
{
    int b = ctx_byte(c, 0);

    if ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || b == '_') {
        return lex_identifier(c, out);
    }
    if (b >= '0' && b <= '9') {
        return lex_number(c, out);
    }
    if (b == '"') {
        return lex_string_literal(c, out);
    }
    if (b == '\'') {
        return lex_char_literal(c, out);
    }
    if (b < 0) {
        return 1; /* unreachable: callers check for end of input */
    }
    return lex_unicode(c, out);
}

/* The comment spellings at a '/'. Returns 1 when consumed, 0 when the
   byte is no comment. */
static int scan_comment(lex_ctx *c, gbuf *out)
{
    int b = ctx_byte(c, 0);

    if (b != '/') {
        return 0;
    }
    if (b == '/' && ctx_byte(c, 1) == '/' && ctx_byte(c, 2) == '/') {
        return lex_doc_comment(c, out);
    }
    if (ctx_byte(c, 1) == '/') {
        lex_skip_line_comment(c);
        return 1;
    }
    if (ctx_byte(c, 1) == '*') {
        ctx_error(c, "E-block-comment", c->line, c->col,
                  "block comments do not exist; use //");
        c->pos += 2;
        c->col += 2;
        return 1;
    }
    return 0;
}

/* Whitespace, line terminators, and comments: bytes the token stream
   drops or reshapes. Returns 1 when the byte was consumed as trivia,
   0 when a token must be lexed, -1 when the arena is exhausted. */
static int scan_trivia(lex_ctx *c, gbuf *out)
{
    int b = ctx_byte(c, 0);

    if (b == ' ') {
        c->pos++;
        c->col++;
        return 1;
    }
    if (b == '\t') {
        ctx_error(c, "E-tab", c->line, c->col,
                  "tab is not valid whitespace");
        c->pos++;
        c->col++;
        return 1;
    }
    if (b == '\r') {
        lex_consume_cr(c);
        return 1; /* the ignored half of a terminator takes no column */
    }
    if (b == '\n') {
        ek_token t;

        memset(&t, 0, sizeof t);
        t.kind = EK_TOK_NEWLINE;
        t.line = c->line;
        t.col = c->col;
        if (!emit_token(c, out, &t)) {
            return -1;
        }
        c->pos++;
        c->line++;
        c->col = 1;
        return 1;
    }
    return scan_comment(c, out);
}

static void scan_all(lex_ctx *c, gbuf *out)
{
    for (;;) {
        int b;
        int trivia;

        if (c->oom) {
            return;
        }
        b = ctx_byte(c, 0);
        if (b < 0) {
            break;
        }
        trivia = scan_trivia(c, out);
        if (trivia != 0) {
            if (trivia < 0) {
                return;
            }
            continue;
        }
        if (!lex_step(c, out, lex_token)) {
            return;
        }
    }
    {
        ek_token t;

        memset(&t, 0, sizeof t);
        t.kind = EK_TOK_EOF;
        t.line = c->line;
        t.col = c->col;
        emit_token(c, out, &t);
    }
}

ek_lex_result ek_lex(const char *source, size_t length, const char *file,
                     ek_arena *arena, ek_diag *diag)
{
    lex_ctx ctx;
    gbuf out;
    ek_lex_result r;

    if (!source) {
        length = 0;
    }
    memset(&ctx, 0, sizeof ctx);
    ctx.src = source;
    ctx.len = length;
    ctx.line = 1;
    ctx.col = 1;
    ctx.file = file;
    ctx.arena = arena;
    ctx.diag = diag;
    gbuf_init(&out, arena);
    scan_all(&ctx, &out);
    r.tokens = (ek_token *)out.data;
    r.count = out.len / sizeof(ek_token);
    return r;
}
