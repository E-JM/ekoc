#include <string.h>

#include "lex/internal.h"

static void unterminated(lex_ctx *c, int sline, int scol)
{
    ctx_error(c, "E-unterminated-string", sline, scol,
              "string literal is not terminated");
}

static int flush_literal(lex_ctx *c, gbuf *seg, gbuf *parts)
{
    ek_string_part p;

    if (seg->len == 0) {
        return 1;
    }
    memset(&p, 0, sizeof p);
    p.text = seg->data;
    p.text_len = seg->len;
    if (!gbuf_put(parts, &p, sizeof p)) {
        ctx_oom(c);
        return 0;
    }
    gbuf_init(seg, seg->arena); /* the spent block lives on in the arena */
    return 1;
}

/* Emits the STRING token when the closing quote was seen; otherwise
   the unterminated literal is diagnosed and no token is
   emitted. */
static int emit_string(lex_ctx *c, gbuf *out, gbuf *seg, gbuf *parts,
                       int closed, int sline, int scol)
{
    ek_token t;

    if (!closed) {
        unterminated(c, sline, scol);
        return 1;
    }
    if (!flush_literal(c, seg, parts)) {
        return 0;
    }
    memset(&t, 0, sizeof t);
    t.kind = EK_TOK_STRING;
    t.line = sline;
    t.col = scol;
    t.parts = (ek_string_part *)parts->data;
    t.part_count = parts->len / sizeof(ek_string_part);
    return emit_token(c, out, &t);
}

/* Closes the slot at its matching '}'. An empty run is E-empty-slot. */
static int close_slot(lex_ctx *c, gbuf *parts, gbuf *run, int bline,
                      int bcol)
{
    ek_string_part p;

    c->pos++;
    c->col++;
    if (run->len == 0) {
        ctx_error(c, "E-empty-slot", bline, bcol,
                  "interpolation slot is empty");
        return 1;
    }
    memset(&p, 0, sizeof p);
    p.tokens = (ek_token *)run->data;
    p.token_count = run->len / sizeof(ek_token);
    if (!gbuf_put(parts, &p, sizeof p)) {
        ctx_oom(c);
        return 0;
    }
    return 1;
}

/* Braces of a composite literal balance through the LBRACE/RBRACE
   token count. */
static void track_braces(const gbuf *run, int *depth)
{
    ek_token_kind k = ((const ek_token *)(run->data + run->len
                                          - sizeof(ek_token)))->kind;

    if (k == EK_TOK_LBRACE) {
        (*depth)++;
    } else if (k == EK_TOK_RBRACE) {
        (*depth)--;
    }
}

/* Slot trivia. Returns 1 when consumed, 0 when a token or the closing
   brace is due, -1 when a line terminator ends the string. A tab is
   whitespace between slot tokens, so it is E-tab. A slot's content is
   an expression: no comment form exists there, and a '/' lexes as the
   operator. */
static int slot_trivia(lex_ctx *c)
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
        return lex_consume_cr(c) ? -1 : 1;
    }
    if (b < 0 || b == '\n') {
        return -1;
    }
    return 0;
}

/* One interpolation slot, scanned to its matching '}'. Returns 1 when
   the slot closed at its matching brace, -1 when the string died
   inside the slot and the caller owes the E-unterminated-string
   diagnostic, 0 when the arena is exhausted. A surplus of braces
   always swallows the closing quote, so the failure surfaces as
   E-unterminated-string and needs no code of its own. */
static int scan_slot(lex_ctx *c, gbuf *parts)
{
    int bline = c->line, bcol = c->col;
    gbuf run;
    int depth = 0;

    gbuf_init(&run, c->arena);
    c->pos++; /* '{' */
    c->col++;
    for (;;) {
        int t = slot_trivia(c);

        if (t < 0) {
            return -1;
        }
        if (t > 0) {
            continue;
        }
        if (ctx_byte(c, 0) == '}' && depth == 0) {
            return close_slot(c, parts, &run, bline, bcol);
        }
        {
            size_t before = run.len;

            if (!lex_step(c, &run, lex_token)) {
                return 0;
            }
            if (run.len > before) {
                track_braces(&run, &depth);
            }
        }
    }
}

static int cook_scalar(lex_ctx *c, gbuf *seg)
{
    ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos, c->len - c->pos);

    if (d.len == 0) {
        return 0;
    }
    if (d.status != EK_UTF8_OK) {
        ctx_error(c, "E-encoding", c->line, c->col, "source is not valid UTF-8");
    } else if (d.cp == 0xFEFF) {
        ctx_error(c, "E-stray-bom", c->line, c->col,
                  "byte order mark outside the start of the file");
    } else if (!gbuf_put(seg, c->src + c->pos, d.len)) {
        ctx_oom(c);
        return 0;
    }
    c->pos += d.len;
    c->col++;
    return 1;
}

/* '\\' at the current position. Returns 1 to continue, 0 on arena
   exhaustion, -1 when the literal ends here unconsumed and the caller
   owes the unterminated diagnostic, -2 when the escape ran into the
   line terminator or end of input and is diagnosed, so the literal
   ends here with nothing further to report. */
static int string_escape(lex_ctx *c, gbuf *seg)
{
    uint32_t cp;
    esc_result r = scan_escape(c, 0, &cp);
    char buf[4];
    size_t n;

    if (r == ESC_END) {
        return -1;
    }
    if (r == ESC_STOP) {
        return -2;
    }
    if (r == ESC_OK) {
        n = ek_utf8_encode(cp, buf);
        if (!gbuf_put(seg, buf, n)) {
            ctx_oom(c);
            return 0;
        }
    }
    return 1;
}

/* '{' at the current position: flush, honor the depth bound, scan one
   slot. Returns 1 to continue, 0 on arena exhaustion, -1 when the
   string died in the slot. */
static int open_slot(lex_ctx *c, gbuf *seg, gbuf *parts)
{
    int r;

    if (!flush_literal(c, seg, parts)) {
        return 0;
    }
    if (c->depth >= EK_LEX_MAX_SLOT_DEPTH) {
        ctx_error(c, "E-slot-depth", c->line, c->col,
                  "interpolation slots are nested too deeply");
        c->pos++;
        c->col++;
        return 1;
    }
    c->depth++;
    r = scan_slot(c, parts);
    c->depth--;
    if (r == 0) {
        return 0;
    }
    return r < 0 ? -1 : 1;
}

/* Puts the scan back at the first bracket of the literal's span, so
   the recovered stream keeps a closer that sits past the defect. A
   span already diagnosed at or past the bracket stays put: rewinding
   would scan that material again and report its faults twice. */
static void recover_at_bracket(lex_ctx *c, int have_bound, size_t pos,
                               int col)
{
    if (have_bound && !(c->saw_fault && c->last_fault_pos >= pos)) {
        c->pos = pos;
        c->col = col;
    }
}

/* One content line of a raw string: where it starts, how long it is,
   its leading-space count, and whether it holds anything at all. */
typedef struct {
    const char *text;
    size_t len;
    size_t indent;
    int blank;
} raw_line;

/* Doubles the line array; arenas never free, so growth copies. */
static raw_line *raw_grow(lex_ctx *c, raw_line *old, size_t *cap, size_t n)
{
    size_t want = *cap ? *cap * 2 : 8;
    raw_line *grown = ek_arena_alloc(c->arena, want * sizeof *grown);

    if (!grown) {
        ctx_oom(c);
        return NULL;
    }
    if (n) {
        memcpy(grown, old, n * sizeof *grown);
    }
    *cap = want;
    return grown;
}

/* Consumes one line terminator. Returns 1 when a line break was
   consumed, 0 at the end of input. */
static int raw_newline(lex_ctx *c)
{
    if (ctx_byte(c, 0) == '\r') {
        c->pos++;
    }
    if (ctx_byte(c, 0) != '\n') {
        return 0;
    }
    c->pos++;
    c->line++;
    c->col = 1;
    return 1;
}

/* Reads one line without consuming its terminator. A leading tab is
   indentation, so it is E-tab; interior bytes are content. A skipped
   line carries no content, so it reports nothing. */
static void raw_scan_line(lex_ctx *c, raw_line *ln, int report_tab)
{
    int b = ctx_byte(c, 0);

    ln->text = c->src + c->pos;
    ln->len = 0;
    ln->indent = 0;
    ln->blank = 1;
    while (b >= 0 && b != '\n' && b != '\r') {
        if (ln->blank && b == ' ') {
            ln->indent++;
        } else if (ln->blank && b == '\t' && report_tab) {
            ctx_error(c, "E-tab", c->line, c->col,
                      "tab is not valid whitespace");
        } else {
            ln->blank = 0;
        }
        c->pos++;
        c->col++;
        ln->len++;
        b = ctx_byte(c, 0);
    }
}

/* Whether a line is the closing delimiter alone: optional spaces, the
   three quotes, and nothing else. */
static int raw_is_closer(const raw_line *ln)
{
    return ln->len - ln->indent == 3 && ln->text[ln->indent] == '"'
        && ln->text[ln->indent + 1] == '"'
        && ln->text[ln->indent + 2] == '"';
}

/* Consumes the lines of a literal already diagnosed, through its closing
   delimiter or the end of input, so the rejection is one diagnostic. */
static void raw_skip(lex_ctx *c)
{
    for (;;) {
        raw_line ln;

        if (ctx_byte(c, 0) < 0) {
            return;
        }
        raw_scan_line(c, &ln, 0);
        if (!ln.blank && raw_is_closer(&ln)) {
            return;
        }
        if (!raw_newline(c)) {
            return;
        }
    }
}

/* Whether the line carries the delimiter inside its content. */
static int raw_has_delimiter(const raw_line *ln)
{
    size_t i;

    for (i = ln->indent; i + 3 <= ln->len; i++) {
        if (ln->text[i] == '"' && ln->text[i + 1] == '"'
            && ln->text[i + 2] == '"') {
            return 1;
        }
    }
    return 0;
}

/* The raw string: three quotes, a line break, content lines, and a
   closing delimiter alone on its line. No escape and no interpolation
   exist inside, the common indent of the content lines and the closing
   line is stripped, and the closing line's newline is not content. The
   whole literal is one token, so the lines inside never reach the
   layout. */
int lex_raw_string(lex_ctx *c, gbuf *out)
{
    int sline = c->line, scol = c->col;
    gbuf seg, parts;
    raw_line *lines = NULL;
    size_t nlines = 0, cap = 0, i, strip, ncontent;
    int closed = 0;

    c->pos += 3;
    c->col += 3;
    if (!raw_newline(c)) {
        if (ctx_byte(c, 0) >= 0) {
            ctx_error(c, "E-syntax", sline, scol,
                      "the opening delimiter of a raw string is followed "
                      "by a line break");
            raw_skip(c);
            return 1;
        }
        unterminated(c, sline, scol);
        return 1;
    }
    for (;;) {
        raw_line ln;

        if (ctx_byte(c, 0) < 0) {
            break;
        }
        raw_scan_line(c, &ln, 1);
        if (nlines == cap) {
            lines = raw_grow(c, lines, &cap, nlines);
            if (!lines) {
                return 0;
            }
        }
        lines[nlines++] = ln;
        if (!ln.blank && raw_is_closer(&ln)) {
            closed = 1;
            break;
        }
        if (raw_has_delimiter(&ln)) {
            ctx_error(c, "E-syntax", sline, scol,
                      "a raw string's content may not contain its "
                      "delimiter, and no escape exists");
            raw_skip(c);
            return 1;
        }
        if (!raw_newline(c)) {
            break;
        }
    }
    if (!closed) {
        unterminated(c, sline, scol);
        return 1;
    }
    /* The closer rides as the last line: its indent counts toward the
       common one, and the content is every line before it. A blank
       line constrains nothing and yields an empty line of content. */
    strip = lines[nlines - 1].indent;
    for (i = 0; i + 1 < nlines; i++) {
        if (!lines[i].blank && lines[i].indent < strip) {
            strip = lines[i].indent;
        }
    }
    gbuf_init(&seg, c->arena);
    gbuf_init(&parts, c->arena);
    ncontent = nlines - 1;
    for (i = 0; i < ncontent; i++) {
        size_t from = lines[i].indent < strip ? lines[i].indent : strip;
        size_t take = lines[i].blank ? 0 : lines[i].len - from;

        if (i > 0 && !gbuf_char(&seg, '\n')) {
            ctx_oom(c);
            return 0;
        }
        if (take && !gbuf_put(&seg, lines[i].text + from, take)) {
            ctx_oom(c);
            return 0;
        }
    }
    return emit_string(c, out, &seg, &parts, 1, sline, scol);
}

int lex_string_literal(lex_ctx *c, gbuf *out)
{
    int sline = c->line, scol = c->col;
    gbuf seg, parts;
    int closed = 0;
    size_t bound_pos = 0;
    int bound_col = 0, have_bound = 0;

    gbuf_init(&seg, c->arena);
    gbuf_init(&parts, c->arena);
    c->pos++; /* opening quote */
    c->col++;
    for (;;) {
        int b = ctx_byte(c, 0);
        int r;

        if (b < 0 || b == '\n') {
            break;
        }
        if (b == '"') {
            c->pos++;
            c->col++;
            closed = 1;
            break;
        }
        if (!have_bound && lex_is_bracket(b)) {
            have_bound = 1;
            bound_pos = c->pos;
            bound_col = c->col;
        }
        if (b == '\r') {
            r = lex_consume_cr(c) ? -1 : 1;
        } else if (b == '\\') {
            r = string_escape(c, &seg);
        } else if (b == '{') {
            r = open_slot(c, &seg, &parts);
        } else if (b == '}') {
            if (!gbuf_char(&seg, '}')) {
                ctx_oom(c);
                return 0;
            }
            c->pos++;
            c->col++;
            continue;
        } else {
            r = cook_scalar(c, &seg) ? 1 : 0;
        }
        if (r < 0) {
            if (r == -1) {
                recover_at_bracket(c, have_bound, bound_pos, bound_col);
                unterminated(c, sline, scol);
            }
            return 1; /* -2: the escape already diagnosed the span */
        }
        if (r == 0) {
            return 0;
        }
    }
    if (!closed) {
        recover_at_bracket(c, have_bound, bound_pos, bound_col);
    }
    return emit_string(c, out, &seg, &parts, closed, sline, scol);
}