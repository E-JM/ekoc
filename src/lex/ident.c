#include <string.h>

#include "lex/internal.h"

static ek_token_kind ident_kind(const char *text)
{
    if (strcmp(text, "import") == 0) {
        return EK_TOK_KW_IMPORT;
    }
    if (strcmp(text, "case") == 0) {
        return EK_TOK_KW_CASE;
    }
    if (strcmp(text, "if") == 0) {
        return EK_TOK_KW_IF;
    }
    if (strcmp(text, "else") == 0) {
        return EK_TOK_KW_ELSE;
    }
    return EK_TOK_IDENT;
}

/* XID_Start or '_' first, then XID_Continue; '_' already
   carries XID_Continue in the tables. Contextual words stay ordinary
   identifiers. */
int lex_identifier(lex_ctx *c, gbuf *out)
{
    size_t start = c->pos;
    int sline = c->line, scol = c->col;
    const char *text;
    ek_token t;

    for (;;) {
        ek_utf8_decoded d = ek_utf8_decode(c->src + c->pos, c->len - c->pos);

        if (d.status != EK_UTF8_OK || d.len == 0 || !ek_xid_continue(d.cp)) {
            break;
        }
        c->pos += d.len;
        c->col++;
    }
    text = ctx_slice(c, start, c->pos);
    if (!text) {
        return 0;
    }
    memset(&t, 0, sizeof t);
    t.kind = ident_kind(text);
    t.line = sline;
    t.col = scol;
    t.text = text;
    t.len = c->pos - start;
    return emit_token(c, out, &t);
}
