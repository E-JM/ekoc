#include <string.h>

#include "lex/internal.h"

/* Compound spellings, matched before their single-byte prefixes.
   Returns the spelling length, 0 when b0 begins none. */
static int match_compound(int b0, int b1, int b2, ek_token_kind *k)
{
    if (b0 == '!' && b1 == '-' && b2 == '>') { *k = EK_TOK_BANGARROW; return 3; }
    if (b0 == '?' && b1 == '-' && b2 == '>') { *k = EK_TOK_QUESTIONARROW; return 3; }
    if (b0 == '.' && b1 == '.' && b2 == '=') { *k = EK_TOK_DOTDOTEQ; return 3; }
    if (b0 == '-' && b1 == '>') { *k = EK_TOK_ARROW; return 2; }
    if (b0 == '<' && b1 == '-') { *k = EK_TOK_LARROW; return 2; }
    if (b0 == '+' && b1 == '+') { *k = EK_TOK_PLUSPLUS; return 2; }
    if (b0 == '=' && b1 == '=') { *k = EK_TOK_EQEQ; return 2; }
    if (b0 == '!' && b1 == '=') { *k = EK_TOK_BANGEQ; return 2; }
    if (b0 == '<' && b1 == '=') { *k = EK_TOK_LTEQ; return 2; }
    if (b0 == '>' && b1 == '=') { *k = EK_TOK_GTEQ; return 2; }
    if (b0 == '&' && b1 == '&') { *k = EK_TOK_ANDAND; return 2; }
    if (b0 == '|' && b1 == '|') { *k = EK_TOK_OROR; return 2; }
    if (b0 == ':' && b1 == '=') { *k = EK_TOK_COLONEQ; return 2; }
    if (b0 == '.' && b1 == '.') { *k = EK_TOK_DOTDOT; return 2; }
    return 0;
}

/* The single-byte operators and punctuation. A lone '&' is
   no token. Returns the spelling length, 0 when the byte
   cannot begin any operator or punctuation mark. */
static int match_single(int b, ek_token_kind *k)
{
    switch (b) {
    case '+': *k = EK_TOK_PLUS; return 1;
    case '-': *k = EK_TOK_MINUS; return 1;
    case '*': *k = EK_TOK_STAR; return 1;
    case '/': *k = EK_TOK_SLASH; return 1;
    case '%': *k = EK_TOK_PERCENT; return 1;
    case '!': *k = EK_TOK_BANG; return 1;
    case '?': *k = EK_TOK_QUESTION; return 1;
    case '<': *k = EK_TOK_LT; return 1;
    case '>': *k = EK_TOK_GT; return 1;
    case '=': *k = EK_TOK_EQ; return 1;
    case ':': *k = EK_TOK_COLON; return 1;
    case '.': *k = EK_TOK_DOT; return 1;
    case '|': *k = EK_TOK_PIPE; return 1;
    case '(': *k = EK_TOK_LPAREN; return 1;
    case ')': *k = EK_TOK_RPAREN; return 1;
    case '[': *k = EK_TOK_LBRACKET; return 1;
    case ']': *k = EK_TOK_RBRACKET; return 1;
    case '{': *k = EK_TOK_LBRACE; return 1;
    case '}': *k = EK_TOK_RBRACE; return 1;
    case ',': *k = EK_TOK_COMMA; return 1;
    case ';': *k = EK_TOK_SEMI; return 1;
    default: return 0;
    }
}

int lex_operator(lex_ctx *c, gbuf *out)
{
    ek_token_kind k;
    int n;
    ek_token t;

    n = match_compound(ctx_byte(c, 0), ctx_byte(c, 1), ctx_byte(c, 2), &k);
    if (n == 0) {
        n = match_single(ctx_byte(c, 0), &k);
    }
    if (n == 0) {
        return 0;
    }
    memset(&t, 0, sizeof t);
    t.kind = k;
    t.line = c->line;
    t.col = c->col;
    if (!emit_token(c, out, &t)) {
        return 0;
    }
    c->pos += (size_t)n;
    c->col += n;
    return 1;
}
