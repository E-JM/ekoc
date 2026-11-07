#include <string.h>

#include "lex/internal.h"

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

/* Gathers the content lines through the closing delimiter. Returns 1
   when closed, 0 on arena exhaustion, -1 when the literal was
   diagnosed inside and no token is owed. */
static int raw_scan_lines(lex_ctx *c, raw_line **lines, size_t *nlines,
                          size_t *cap, int sline, int scol)
{
    for (;;) {
        raw_line ln;

        if (ctx_byte(c, 0) < 0) {
            str_unterminated(c, sline, scol);
            return -1;
        }
        raw_scan_line(c, &ln, 1);
        if (*nlines == *cap) {
            *lines = raw_grow(c, *lines, cap, *nlines);
            if (!*lines) {
                return 0;
            }
        }
        (*lines)[(*nlines)++] = ln;
        if (!ln.blank && raw_is_closer(&ln)) {
            return 1;
        }
        if (raw_has_delimiter(&ln)) {
            ctx_error(c, "E-syntax", sline, scol,
                      "a raw string's content may not contain its "
                      "delimiter, and no escape exists");
            raw_skip(c);
            return -1;
        }
        if (!raw_newline(c)) {
            str_unterminated(c, sline, scol);
            return -1;
        }
    }
}

/* The common indent of the gathered lines: the closer rides as the
   last line, its indent counts toward the common one, and a blank
   line constrains nothing. */
static size_t raw_strip(const raw_line *lines, size_t nlines)
{
    size_t strip = lines[nlines - 1].indent;
    size_t i;

    for (i = 0; i + 1 < nlines; i++) {
        if (!lines[i].blank && lines[i].indent < strip) {
            strip = lines[i].indent;
        }
    }
    return strip;
}

/* Joins the content lines under the common indent; a blank line
   yields an empty line of content. */
static int raw_join(lex_ctx *c, gbuf *seg, const raw_line *lines,
                    size_t nlines, size_t strip)
{
    size_t i, ncontent = nlines - 1;

    for (i = 0; i < ncontent; i++) {
        size_t from = lines[i].indent < strip ? lines[i].indent : strip;
        size_t take = lines[i].blank ? 0 : lines[i].len - from;

        if (i > 0 && !gbuf_char(seg, '\n')) {
            ctx_oom(c);
            return 0;
        }
        if (take && !gbuf_put(seg, lines[i].text + from, take)) {
            ctx_oom(c);
            return 0;
        }
    }
    return 1;
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
    raw_line *lines = NULL;
    size_t nlines = 0, cap = 0;
    gbuf seg, parts;
    int r;

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
        str_unterminated(c, sline, scol);
        return 1;
    }
    gbuf_init(&seg, c->arena);
    gbuf_init(&parts, c->arena);
    r = raw_scan_lines(c, &lines, &nlines, &cap, sline, scol);
    if (r == 0) {
        return 0;
    }
    if (r < 0) {
        return 1;
    }
    if (!raw_join(c, &seg, lines, nlines, raw_strip(lines, nlines))) {
        return 0;
    }
    return str_emit(c, out, &seg, &parts, 1, sline, scol);
}
