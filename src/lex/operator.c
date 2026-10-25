#include <string.h>

#include "lex/internal.h"

/* Longest match over the tables of Clause 5.10.3 and A.2.2. Returns 0
   when the byte cannot begin any operator or punctuation mark. */
int lex_operator(lex_ctx *c, gbuf *out)
{
    int b0 = ctx_byte(c, 0), b1 = ctx_byte(c, 1), b2 = ctx_byte(c, 2);
    ek_token_kind k;
    int n = 1;
    ek_token t;

    switch (b0) {
    case '+':
        k = EK_TOK_PLUS;
        if (b1 == '+') { k = EK_TOK_PLUSPLUS; n = 2; }
        break;
    case '-':
        k = EK_TOK_MINUS;
        if (b1 == '>') { k = EK_TOK_ARROW; n = 2; }
        break;
    case '*': k = EK_TOK_STAR; break;
    case '/': k = EK_TOK_SLASH; break;
    case '%': k = EK_TOK_PERCENT; break;
    case '!':
        if (b1 == '-' && b2 == '>') { k = EK_TOK_BANGARROW; n = 3; }
        else if (b1 == '=') { k = EK_TOK_BANGEQ; n = 2; }
        else { k = EK_TOK_BANG; }
        break;
    case '?':
        if (b1 == '-' && b2 == '>') { k = EK_TOK_QUESTIONARROW; n = 3; }
        else { k = EK_TOK_QUESTION; }
        break;
    case '<':
        if (b1 == '-') { k = EK_TOK_LARROW; n = 2; }
        else if (b1 == '=') { k = EK_TOK_LTEQ; n = 2; }
        else { k = EK_TOK_LT; }
        break;
    case '>':
        k = EK_TOK_GT;
        if (b1 == '=') { k = EK_TOK_GTEQ; n = 2; }
        break;
    case '=':
        k = EK_TOK_EQ;
        if (b1 == '=') { k = EK_TOK_EQEQ; n = 2; }
        break;
    case ':':
        k = EK_TOK_COLON;
        if (b1 == '=') { k = EK_TOK_COLONEQ; n = 2; }
        break;
    case '.':
        if (b1 == '.' && b2 == '=') { k = EK_TOK_DOTDOTEQ; n = 3; }
        else if (b1 == '.') { k = EK_TOK_DOTDOT; n = 2; }
        else { k = EK_TOK_DOT; }
        break;
    case '&':
        if (b1 != '&') {
            return 0; /* lone & is no token (Clause 5.11) */
        }
        k = EK_TOK_ANDAND;
        n = 2;
        break;
    case '|':
        k = EK_TOK_PIPE;
        if (b1 == '|') { k = EK_TOK_OROR; n = 2; }
        break;
    case '(': k = EK_TOK_LPAREN; break;
    case ')': k = EK_TOK_RPAREN; break;
    case '[': k = EK_TOK_LBRACKET; break;
    case ']': k = EK_TOK_RBRACKET; break;
    case '{': k = EK_TOK_LBRACE; break;
    case '}': k = EK_TOK_RBRACE; break;
    case ',': k = EK_TOK_COMMA; break;
    case ';': k = EK_TOK_SEMI; break;
    default: return 0;
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
