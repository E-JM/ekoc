#include "common.h"

/* Source-level rules: encoding, BOM, CR, tabs, comments, EOF. */

static void test_bom(void)
{
    lex_fixture f;

    fx_begin(&f, "\xEF\xBB\xBFx");
    EK_CHECK_U64(fx_count(&f), 2);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_IDENT);
    EK_CHECK_U64(fx_tok(&f, 0).col, 1);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "x\xEF\xBB\xBF");
    EK_CHECK(fx_has_error(&f, "E-stray-bom"));
    fx_end(&f);

    fx_begin(&f, "\xEF\xBB\xBF\xEF\xBB\xBFx");
    EK_CHECK(fx_has_error(&f, "E-stray-bom"));
    fx_end(&f);

    fx_begin(&f, "\"a" "\xEF\xBB\xBF" "b\"");
    EK_CHECK(fx_has_error(&f, "E-stray-bom"));
    fx_end(&f);
}

static void test_line_terminators(void)
{
    lex_fixture f;

    fx_begin(&f, "a\r\nb");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count(&f), 4);
    if (fx_count(&f) == 4) {
        EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_NEWLINE);
        EK_CHECK_U64(fx_tok(&f, 2).kind, EK_TOK_IDENT);
        EK_CHECK_U64(fx_tok(&f, 2).line, 2);
    }
    fx_end(&f);

    fx_begin(&f, "a\rb");
    EK_CHECK(fx_has_error(&f, "E-stray-cr"));
    fx_end(&f);

    fx_begin(&f, "a\r");
    EK_CHECK(fx_has_error(&f, "E-stray-cr"));
    fx_end(&f);

    fx_begin(&f, "a\n\nb");
    EK_CHECK_U64(fx_errors(&f), 0);
    EK_CHECK_U64(fx_count(&f), 5); /* two NEWLINEs, no token between */
    fx_end(&f);
}

/* Invalid UTF-8 anywhere in the source is the encoding fault. The code
   spells in kebab-case, so the exact-string match also pins that the
   code carries no digits. */
static void test_utf8_faults(void)
{
    static const char *const bad[] = {
        "\xFF", "a" "\xFF" "b", "\xC3", "a" "\xC3", "\xE2\x82", "\xED\xA0\x80",
        "\x80", "\xF5\x88\x80\x80",
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        lex_fixture f;

        fx_begin(&f, bad[i]);
        EK_CHECK(fx_has_error(&f, "E-encoding"));
        fx_end(&f);
    }
}

static void test_tabs(void)
{
    lex_fixture f;

    fx_begin(&f, "\tx");
    EK_CHECK(fx_has_error(&f, "E-tab"));
    fx_end(&f);

    fx_begin(&f, "x\ty");
    EK_CHECK(fx_has_error(&f, "E-tab"));
    fx_end(&f);

    fx_begin(&f, "if\tx\n");
    EK_CHECK(fx_has_error(&f, "E-tab"));
    fx_end(&f);

    /* a tab inside a comment is ignored */
    fx_begin(&f, "// a\tb\nx");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    /* and inside literals it is a plain scalar */
    fx_begin(&f, "\"a\tb\"");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "'\t'");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

static void test_comments(void)
{
    lex_fixture f;

    fx_begin(&f, "// note\nx");
    EK_CHECK_U64(fx_count(&f), 3); /* NEWLINE IDENT EOF */
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_NEWLINE);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_IDENT);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "/// doc\n/// more\nx");
    EK_CHECK_U64(fx_count(&f), 6); /* DOC NL DOC NL IDENT EOF */
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_DOC);
    EK_CHECK_STR(fx_tok(&f, 0).text, "doc");
    EK_CHECK_U64(fx_tok(&f, 2).kind, EK_TOK_DOC);
    EK_CHECK_STR(fx_tok(&f, 2).text, "more");
    EK_CHECK_U64(fx_tok(&f, 4).kind, EK_TOK_IDENT);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "x // trailing");
    EK_CHECK_U64(fx_count(&f), 2); /* IDENT EOF, no trailing NEWLINE */
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    /* a block-comment spelling is one consumed span: the body never
       reaches the token stream */
    fx_begin(&f, "/* c */\nx");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
    EK_CHECK_U64(fx_count(&f), 3); /* NEWLINE IDENT EOF */
    fx_end(&f);

    fx_begin(&f, "/**/");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
    EK_CHECK_U64(fx_count(&f), 1); /* EOF only */
    fx_end(&f);

    /* the span closes at the marker on the same line */
    fx_begin(&f, "a /* x */ b");
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
    EK_CHECK_U64(fx_count(&f), 3); /* a b EOF */
    fx_end(&f);

    /* without the marker the span runs to the line terminator */
    fx_begin(&f, "a /* x\nb");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
    EK_CHECK_U64(fx_count(&f), 4); /* a NEWLINE b EOF */
    fx_end(&f);

    FX_KINDS("a/b", EK_TOK_IDENT, EK_TOK_SLASH, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("x / y", EK_TOK_IDENT, EK_TOK_SLASH, EK_TOK_IDENT, EK_TOK_EOF);
}

static void test_eof_shapes(void)
{
    lex_fixture f;

    fx_begin(&f, "\n");
    EK_CHECK_U64(fx_count(&f), 2);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_NEWLINE);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_EOF);
    EK_CHECK_U64(fx_tok(&f, 1).line, 2);
    EK_CHECK_U64(fx_tok(&f, 1).col, 1);
    fx_end(&f);

    fx_begin(&f, "x");
    EK_CHECK_U64(fx_count(&f), 2);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_EOF);
    EK_CHECK_U64(fx_tok(&f, 1).line, 1);
    EK_CHECK_U64(fx_tok(&f, 1).col, 2);
    fx_end(&f);

    /* input that is only comments */
    fx_begin(&f, "// nothing here");
    EK_CHECK_U64(fx_count(&f), 1);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_EOF);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "// one\n// two\n");
    EK_CHECK_U64(fx_count(&f), 3);
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_NEWLINE);
    EK_CHECK_U64(fx_tok(&f, 1).kind, EK_TOK_NEWLINE);
    EK_CHECK_U64(fx_tok(&f, 2).kind, EK_TOK_EOF);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    /* a lone CR is a fault even as the whole input */
    fx_begin(&f, "\r");
    EK_CHECK(fx_has_error(&f, "E-stray-cr"));
    fx_end(&f);
}

/* Comment content is scanned like any other source text, so a
   BOM after a comment is outside the start of the file. */
static void test_comment_content_faults(void)
{
    lex_fixture f;

    fx_begin(&f, "// c\n\xEF\xBB\xBFx");
    EK_CHECK(fx_has_error(&f, "E-stray-bom"));
    fx_end(&f);

    fx_begin(&f, "// \xFF\nx");
    EK_CHECK(fx_has_error(&f, "E-encoding"));
    fx_end(&f);

    fx_begin(&f, "/*\n");
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
    EK_CHECK_U64(fx_count(&f), 2); /* NEWLINE EOF, body consumed */
    fx_end(&f);
}

/* A NUL byte is an ordinary byte that cannot begin a token; it ends
   neither the scan nor a token (fx_begin cannot carry it, so this
   calls ek_lex with an explicit length). */
static void test_embedded_nul(void)
{
    lex_fixture f;

    f.arena = ek_arena_create();
    f.diag = ek_diag_create();
    f.r = ek_lex("a\0b", 3, "test.eko", f.arena, f.diag);
    EK_CHECK_U64(fx_count(&f), 3); /* IDENT IDENT EOF */
    EK_CHECK(fx_has_error(&f, "E-character"));
    fx_end(&f);
}

int main(void)
{
    test_bom();
    test_line_terminators();
    test_utf8_faults();
    test_tabs();
    test_comments();
    test_eof_shapes();
    test_comment_content_faults();
    test_embedded_nul();
    return ek_test_report("lex/source");
}
