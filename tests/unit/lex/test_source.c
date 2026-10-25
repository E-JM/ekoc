#include "common.h"

/* Source-level rules: encoding, BOM, CR, tabs, comments, EOF
   (Clauses 5.1, 5.2.3, 5.4, 5.11). */

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
        EK_CHECK(fx_has_error(&f, "E-utf8"));
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

    /* a tab inside a comment is ignored (Clause 5.2.3) */
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
    EK_CHECK_U64(fx_count(&f), 4);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "x // trailing");
    EK_CHECK_U64(fx_count(&f), 2); /* IDENT EOF, no trailing NEWLINE */
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "/* c */\nx");
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
    fx_end(&f);

    fx_begin(&f, "/**/");
    EK_CHECK(fx_has_error(&f, "E-block-comment"));
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
}

int main(void)
{
    test_bom();
    test_line_terminators();
    test_utf8_faults();
    test_tabs();
    test_comments();
    test_eof_shapes();
    return ek_test_report("lex/source");
}
