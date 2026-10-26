#include "common.h"

/* Token classes, the longest-match table of Clause 5.10.3 verbatim,
   punctuation, markers, and stream shape. */

static void test_punctuation(void)
{
    FX_KINDS("(", EK_TOK_LPAREN, EK_TOK_EOF);
    FX_KINDS(")", EK_TOK_RPAREN, EK_TOK_EOF);
    FX_KINDS("[", EK_TOK_LBRACKET, EK_TOK_EOF);
    FX_KINDS("]", EK_TOK_RBRACKET, EK_TOK_EOF);
    FX_KINDS("{", EK_TOK_LBRACE, EK_TOK_EOF);
    FX_KINDS("}", EK_TOK_RBRACE, EK_TOK_EOF);
    FX_KINDS(",", EK_TOK_COMMA, EK_TOK_EOF);
    FX_KINDS(":", EK_TOK_COLON, EK_TOK_EOF);
    FX_KINDS("=", EK_TOK_EQ, EK_TOK_EOF);
    FX_KINDS("|", EK_TOK_PIPE, EK_TOK_EOF);
    FX_KINDS(";", EK_TOK_SEMI, EK_TOK_EOF);
    FX_KINDS("x = 1", EK_TOK_IDENT, EK_TOK_EQ, EK_TOK_INT, EK_TOK_EOF);
    FX_KINDS("a, b: T", EK_TOK_IDENT, EK_TOK_COMMA, EK_TOK_IDENT,
             EK_TOK_COLON, EK_TOK_IDENT, EK_TOK_EOF);
}

/* Each row of 5.10.3 is one token and never splits. */
static void test_longest_match_table(void)
{
    FX_KINDS("!->", EK_TOK_BANGARROW, EK_TOK_EOF);
    FX_KINDS("?->", EK_TOK_QUESTIONARROW, EK_TOK_EOF);
    FX_KINDS("->", EK_TOK_ARROW, EK_TOK_EOF);
    FX_KINDS("<-", EK_TOK_LARROW, EK_TOK_EOF);
    FX_KINDS(":=", EK_TOK_COLONEQ, EK_TOK_EOF);
    FX_KINDS("..=", EK_TOK_DOTDOTEQ, EK_TOK_EOF);
    FX_KINDS("..", EK_TOK_DOTDOT, EK_TOK_EOF);
    FX_KINDS("==", EK_TOK_EQEQ, EK_TOK_EOF);
    FX_KINDS("!=", EK_TOK_BANGEQ, EK_TOK_EOF);
    FX_KINDS("<=", EK_TOK_LTEQ, EK_TOK_EOF);
    FX_KINDS(">=", EK_TOK_GTEQ, EK_TOK_EOF);
    FX_KINDS("&&", EK_TOK_ANDAND, EK_TOK_EOF);
    FX_KINDS("||", EK_TOK_OROR, EK_TOK_EOF);
    FX_KINDS("++", EK_TOK_PLUSPLUS, EK_TOK_EOF);
    FX_KINDS("a := b", EK_TOK_IDENT, EK_TOK_COLONEQ, EK_TOK_IDENT,
             EK_TOK_EOF);
    FX_KINDS("a ..= b", EK_TOK_IDENT, EK_TOK_DOTDOTEQ, EK_TOK_IDENT,
             EK_TOK_EOF);
    FX_KINDS("a ++ b", EK_TOK_IDENT, EK_TOK_PLUSPLUS, EK_TOK_IDENT,
             EK_TOK_EOF);
}

static void test_prefix_splitting(void)
{
    FX_KINDS("!", EK_TOK_BANG, EK_TOK_EOF);
    FX_KINDS("?", EK_TOK_QUESTION, EK_TOK_EOF);
    FX_KINDS("!x", EK_TOK_BANG, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("x?", EK_TOK_IDENT, EK_TOK_QUESTION, EK_TOK_EOF);
    FX_KINDS("a !== b", EK_TOK_IDENT, EK_TOK_BANGEQ, EK_TOK_EQ,
             EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("a === b", EK_TOK_IDENT, EK_TOK_EQEQ, EK_TOK_EQ, EK_TOK_IDENT,
             EK_TOK_EOF);
    FX_KINDS("- -1", EK_TOK_MINUS, EK_TOK_MINUS, EK_TOK_INT, EK_TOK_EOF);
    FX_KINDS("<<", EK_TOK_LT, EK_TOK_LT, EK_TOK_EOF);
    FX_KINDS(">>", EK_TOK_GT, EK_TOK_GT, EK_TOK_EOF);
    FX_KINDS("! ->", EK_TOK_BANG, EK_TOK_ARROW, EK_TOK_EOF);
    FX_KINDS("? ->", EK_TOK_QUESTION, EK_TOK_ARROW, EK_TOK_EOF);
    FX_KINDS(". .", EK_TOK_DOT, EK_TOK_DOT, EK_TOK_EOF);
    FX_KINDS("...", EK_TOK_DOTDOT, EK_TOK_DOT, EK_TOK_EOF);
    FX_KINDS("..x", EK_TOK_DOTDOT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS(": :", EK_TOK_COLON, EK_TOK_COLON, EK_TOK_EOF);
}

/* 5.10.3: the dot rule and the worked examples. */
static void test_dot_rules(void)
{
    FX_KINDS("1..2", EK_TOK_INT, EK_TOK_DOTDOT, EK_TOK_INT, EK_TOK_EOF);
    FX_KINDS("1..=2", EK_TOK_INT, EK_TOK_DOTDOTEQ, EK_TOK_INT, EK_TOK_EOF);
    FX_KINDS("255.to_hex()", EK_TOK_INT, EK_TOK_DOT, EK_TOK_IDENT,
             EK_TOK_LPAREN, EK_TOK_RPAREN, EK_TOK_EOF);
    FX_KINDS("1.5", EK_TOK_FLOAT, EK_TOK_EOF);
    FX_KINDS("1.5.2", EK_TOK_FLOAT, EK_TOK_DOT, EK_TOK_INT, EK_TOK_EOF);
}

/* 5.11: bitwise spellings are not tokens. */
static void test_excluded_symbols(void)
{
    lex_fixture f;

    fx_begin(&f, "&");
    EK_CHECK_U64(fx_count(&f), 1); /* only EOF */
    EK_CHECK(fx_has_error(&f, "E-character"));
    fx_end(&f);

    fx_begin(&f, "a & b");
    EK_CHECK(fx_has_error(&f, "E-character"));
    fx_end(&f);

    fx_begin(&f, "a ^ b");
    EK_CHECK(fx_has_error(&f, "E-character"));
    fx_end(&f);

    fx_begin(&f, "a ~ b");
    EK_CHECK(fx_has_error(&f, "E-character"));
    fx_end(&f);

    fx_begin(&f, "@ $ `");
    EK_CHECK(fx_has_error(&f, "E-character"));
    fx_end(&f);

    FX_KINDS("a && b || c", EK_TOK_IDENT, EK_TOK_ANDAND, EK_TOK_IDENT,
             EK_TOK_OROR, EK_TOK_IDENT, EK_TOK_EOF);
}

/* Compound spellings stop where their table stops; the remainder
   lexes as the shorter tokens (Clause 5.10.3 longest match). */
static void test_compound_boundaries(void)
{
    FX_KINDS("!!->", EK_TOK_BANG, EK_TOK_BANGARROW, EK_TOK_EOF);
    FX_KINDS("-->", EK_TOK_MINUS, EK_TOK_ARROW, EK_TOK_EOF);
    FX_KINDS("->>", EK_TOK_ARROW, EK_TOK_GT, EK_TOK_EOF);
    FX_KINDS("<->", EK_TOK_LARROW, EK_TOK_GT, EK_TOK_EOF);
    FX_KINDS("!-", EK_TOK_BANG, EK_TOK_MINUS, EK_TOK_EOF);
    FX_KINDS("!!", EK_TOK_BANG, EK_TOK_BANG, EK_TOK_EOF);
    FX_KINDS("??", EK_TOK_QUESTION, EK_TOK_QUESTION, EK_TOK_EOF);
    FX_KINDS("\?\?->", EK_TOK_QUESTION, EK_TOK_QUESTIONARROW, EK_TOK_EOF);
    FX_KINDS("<=>", EK_TOK_LTEQ, EK_TOK_GT, EK_TOK_EOF);
    FX_KINDS(":==", EK_TOK_COLONEQ, EK_TOK_EQ, EK_TOK_EOF);
    FX_KINDS("!==", EK_TOK_BANGEQ, EK_TOK_EQ, EK_TOK_EOF);
    FX_KINDS("..==", EK_TOK_DOTDOTEQ, EK_TOK_EQ, EK_TOK_EOF);
    FX_KINDS("|||", EK_TOK_OROR, EK_TOK_PIPE, EK_TOK_EOF);
}

static void test_stream_shape(void)
{
    lex_fixture f;

    fx_begin(&f, "");
    EK_CHECK_U64(fx_count(&f), 1);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_EOF);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    FX_KINDS("x;\ny", EK_TOK_IDENT, EK_TOK_SEMI, EK_TOK_NEWLINE,
             EK_TOK_IDENT, EK_TOK_EOF);

    fx_begin(&f, "x\ny");
    EK_CHECK_U64(fx_tok(&f, 0).line, 1);
    EK_CHECK_U64(fx_tok(&f, 0).col, 1);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_NEWLINE);
    EK_CHECK_U64(fx_tok(&f, 1).line, 1);
    EK_CHECK_U64(fx_tok(&f, 1).col, 2);
    EK_CHECK_U64(fx_tok(&f, 2).line, 2);
    EK_CHECK_U64(fx_tok(&f, 2).col, 1);
    EK_CHECK_U64(fx_tok(&f, 3).kind, EK_TOK_EOF);
    EK_CHECK_U64(fx_tok(&f, 3).line, 2);
    EK_CHECK_U64(fx_tok(&f, 3).col, 2);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    /* a scalar counts one column, whatever its byte length */
    fx_begin(&f, "\xCE\xBB x");
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_IDENT);
    EK_CHECK_U64(fx_tok(&f, 0).col, 1);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_IDENT);
    EK_CHECK_U64(fx_tok(&f, 1).col, 3);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

int main(void)
{
    test_punctuation();
    test_longest_match_table();
    test_prefix_splitting();
    test_compound_boundaries();
    test_dot_rules();
    test_excluded_symbols();
    test_stream_shape();
    return ek_test_report("lex/tokens");
}
