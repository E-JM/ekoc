#include "common.h"

/* Identifiers, keywords, contextual words: Clauses 5.5, 5.6. */

static void expect_ident(const char *src, const char *name)
{
    lex_fixture f;

    fx_begin(&f, src);
    EK_CHECK_U64(fx_count(&f), 2);
    if (fx_count(&f) == 2) {
        ek_token t = fx_tok(&f, 0);
        EK_CHECK_U64(t.kind, EK_TOK_IDENT);
        EK_CHECK_STR(t.text, name);
    }
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

static void test_identifiers(void)
{
    expect_ident("person", "person");
    expect_ident("Person", "Person");
    expect_ident("_tmp", "_tmp");
    expect_ident("_", "_");
    expect_ident("x1", "x1");
    expect_ident("a_b_c", "a_b_c");
    expect_ident("type", "type");
    expect_ident("ctype", "ctype");
    expect_ident("test", "test");
    expect_ident("extern", "extern");
    expect_ident("Pure", "Pure");
    expect_ident("importx", "importx");
    expect_ident("ifx", "ifx");
    expect_ident("case1", "case1");
    expect_ident("elsey", "elsey");
}

static void test_non_ascii(void)
{
    expect_ident("\xCE\xBB", "\xCE\xBB"); /* lambda */
    expect_ident("\xCE\xBBx", "\xCE\xBBx");
    expect_ident("x\xCE\xBB", "x\xCE\xBB"); /* XID_Continue after start */
    expect_ident("\xE5\xA4\x89\xE6\x95\xB0", "\xE5\xA4\x89\xE6\x95\xB0");
    expect_ident("_\xCE\xBB", "_\xCE\xBB");
}

static void test_keywords(void)
{
    FX_KINDS("import", EK_TOK_KW_IMPORT, EK_TOK_EOF);
    FX_KINDS("case", EK_TOK_KW_CASE, EK_TOK_EOF);
    FX_KINDS("if", EK_TOK_KW_IF, EK_TOK_EOF);
    FX_KINDS("else", EK_TOK_KW_ELSE, EK_TOK_EOF);
    FX_KINDS("import geolib", EK_TOK_KW_IMPORT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("case n if n < 0 ->",
             EK_TOK_KW_CASE, EK_TOK_IDENT, EK_TOK_KW_IF, EK_TOK_IDENT,
             EK_TOK_LT, EK_TOK_INT, EK_TOK_ARROW, EK_TOK_EOF);
}

static void test_contextual_words(void)
{
    /* contextual words are ordinary identifiers even in their special
       spellings; only Clause 6 positions give them meaning */
    FX_KINDS("x: type = 1", EK_TOK_IDENT, EK_TOK_COLON, EK_TOK_IDENT,
             EK_TOK_EQ, EK_TOK_INT, EK_TOK_EOF);
    FX_KINDS("t: ctype = [v: UInt]",
             EK_TOK_IDENT, EK_TOK_COLON, EK_TOK_IDENT, EK_TOK_EQ,
             EK_TOK_LBRACKET, EK_TOK_IDENT, EK_TOK_COLON, EK_TOK_IDENT,
             EK_TOK_RBRACKET, EK_TOK_EOF);
    FX_KINDS("f: (Int) !-> Int = extern \"f\"",
             EK_TOK_IDENT, EK_TOK_COLON, EK_TOK_LPAREN, EK_TOK_IDENT,
             EK_TOK_RPAREN, EK_TOK_BANGARROW, EK_TOK_IDENT, EK_TOK_EQ,
             EK_TOK_IDENT, EK_TOK_STRING, EK_TOK_EOF);
}

static void test_boundaries(void)
{
    /* a combining mark continues but cannot start */
    expect_ident("a\xCC\x81", "a\xCC\x81");
    expect_ident("_\xCC\x81", "_\xCC\x81");
    {
        lex_fixture f;

        fx_begin(&f, "\xCC\x81x");
        EK_CHECK(fx_has_error(&f, "E-character"));
        fx_end(&f);
    }
    /* an emoji cannot start an identifier */
    {
        lex_fixture f;

        fx_begin(&f, "\xF0\x9F\x98\x80");
        EK_CHECK(fx_has_error(&f, "E-character"));
        fx_end(&f);
    }

    /* a stranded grouping underscore ends the literal; the underscore
       then lexes as the start of an identifier */
    {
        lex_fixture f;

        fx_begin(&f, "1_x");
        EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
        EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_INT);
        EK_CHECK_STR(fx_tok(&f, 1).text, "_x");
        fx_end(&f);
    }
}

static void test_case_sensitivity(void)
{
    lex_fixture f;

    FX_KINDS("Person person PERSON",
             EK_TOK_IDENT, EK_TOK_IDENT, EK_TOK_IDENT, EK_TOK_EOF);
    fx_begin(&f, "Person person PERSON");
    EK_CHECK_STR(fx_tok(&f, 0).text, "Person");
    EK_CHECK_STR(fx_tok(&f, 1).text, "person");
    EK_CHECK_STR(fx_tok(&f, 2).text, "PERSON");
    fx_end(&f);
}

int main(void)
{
    test_identifiers();
    test_non_ascii();
    test_keywords();
    test_contextual_words();
    test_boundaries();
    test_case_sensitivity();
    return ek_test_report("lex/idents");
}
