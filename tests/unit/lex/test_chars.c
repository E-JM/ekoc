#include "common.h"

/* Character literals: raw scalars, escapes, the one-scalar rule. */

static void expect_char(const char *src, uint32_t scalar)
{
    lex_fixture f;

    fx_begin(&f, src);
    EK_CHECK_U64(fx_count(&f), 2);
    if (fx_count(&f) == 2) {
        ek_token t = fx_tok(&f, 0);
        EK_CHECK_U64(t.kind, EK_TOK_CHAR);
        EK_CHECK_U64(t.scalar, scalar);
    }
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

static void test_single_scalars(void)
{
    expect_char("'a'", 'a');
    expect_char("'Z'", 'Z');
    expect_char("'0'", '0');
    expect_char("' '", ' ');
    expect_char("'\xCE\xBB'", 0x03BB); /* lambda */
    expect_char("'\xC3\xA9'", 0x00E9); /* e-acute */
    expect_char("'\xE4\xB8\x80'", 0x4E00); /* CJK */
}

static void test_escapes(void)
{
    expect_char("'\\n'", 0x0A);
    expect_char("'\\t'", 0x09);
    expect_char("'\\r'", 0x0D);
    expect_char("'\\\\'", 0x5C);
    expect_char("'\\\"'", 0x22);
    expect_char("'\\''", 0x27);
    expect_char("'\\{'", 0x7B); /* the string escapes all apply here */
    expect_char("'\\u{41}'", 0x41);
    expect_char("'\\u{1F600}'", 0x1F600);
    expect_char("'\\u{10FFFF}'", 0x10FFFF);
}

static void test_invalid_escapes(void)
{
    static const char *const bad[] = {
        "'\\q'", "'\\x'", "'\\u{}'", "'\\u{D800}'",
        "'\\u{110000}'", "'\\u{1234567}'", "'\\u41'",
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        lex_fixture f;

        fx_begin(&f, bad[i]);
        EK_CHECK(fx_has_error(&f, "E-escape"));
        fx_end(&f);
    }
}

static void test_one_scalar_rule(void)
{
    lex_fixture f;

    fx_begin(&f, "''");
    EK_CHECK(fx_has_error(&f, "E-char-literal"));
    fx_end(&f);

    fx_begin(&f, "'ab'");
    EK_CHECK(fx_has_error(&f, "E-char-literal"));
    EK_CHECK_U64(fx_count(&f), 1); /* no CHAR token; only EOF */
    fx_end(&f);

    fx_begin(&f, "'\xCE\xBB\xCE\xBB'");
    EK_CHECK(fx_has_error(&f, "E-char-literal"));
    fx_end(&f);

    fx_begin(&f, "'\\n\\n'");
    EK_CHECK(fx_has_error(&f, "E-char-literal"));
    fx_end(&f);

    /* raw and escaped scalars count alike */
    fx_begin(&f, "'a\\n'");
    EK_CHECK(fx_has_error(&f, "E-char-literal"));
    fx_end(&f);

    fx_begin(&f, "'\\u{41}x'");
    EK_CHECK(fx_has_error(&f, "E-char-literal"));
    fx_end(&f);
}

static void test_unterminated(void)
{
    lex_fixture f;

    fx_begin(&f, "'a");
    EK_CHECK(fx_has_error(&f, "E-unterminated-char"));
    fx_end(&f);

    fx_begin(&f, "'a\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-char"));
    fx_end(&f);

    fx_begin(&f, "'a\r\n");
    EK_CHECK(fx_has_error(&f, "E-unterminated-char"));
    fx_end(&f);

    fx_begin(&f, "'");
    EK_CHECK(fx_has_error(&f, "E-unterminated-char"));
    fx_end(&f);

    fx_begin(&f, "'a\\");
    EK_CHECK(fx_has_error(&f, "E-unterminated-char"));
    fx_end(&f);
}

/* An escape that runs into the line terminator is the whole defect:
   the E-escape span ends the literal, so no unterminated-char fault
   follows it. */
static void test_unterminated_escape_is_one_defect(void)
{
    lex_fixture f;

    fx_begin(&f, "'\\u{12\n");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK(fx_has_error(&f, "E-escape"));
    fx_end(&f);

    fx_begin(&f, "'\\u{12");
    EK_CHECK_U64(fx_errors(&f), 1);
    EK_CHECK(fx_has_error(&f, "E-escape"));
    fx_end(&f);
}

static void test_context(void)
{
    lex_fixture f;

    /* an adjacent apostrophe does not glue to a character literal */
    FX_KINDS("'a' ++ 'b'", EK_TOK_CHAR, EK_TOK_PLUSPLUS, EK_TOK_CHAR,
             EK_TOK_EOF);

    /* a raw tab is a scalar; only \t is the spelled form */
    expect_char("'\t'", 0x09);

    fx_begin(&f, "'a\rb'\nx");
    EK_CHECK(fx_has_error(&f, "E-stray-cr"));
    fx_end(&f);
}

int main(void)
{
    test_single_scalars();
    test_escapes();
    test_invalid_escapes();
    test_one_scalar_rule();
    test_unterminated();
    test_unterminated_escape_is_one_defect();
    test_context();
    return ek_test_report("lex/chars");
}
