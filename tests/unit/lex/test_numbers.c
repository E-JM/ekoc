#include "common.h"

#include <math.h>

/* Numeric literals: Clause 5.7 and Annex A.2.4/A.2.5. */

static void expect_float(const char *src, double v)
{
    lex_fixture f;

    fx_begin(&f, src);
    EK_CHECK_U64(fx_count(&f), 2);
    if (fx_count(&f) == 2) {
        ek_token t = fx_tok(&f, 0);
        EK_CHECK_U64(t.kind, EK_TOK_FLOAT);
        if (t.kind == EK_TOK_FLOAT && fabs(t.fvalue - v) > 1e-9) {
            EK_CHECK(t.fvalue == v);
        }
    }
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

static void test_integers(void)
{
    lex_fixture f;

    fx_begin(&f, "255");
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_INT);
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 255);
    EK_CHECK_U64(fx_tok(&f, 0).overflow, 0);
    EK_CHECK_U64(fx_tok(&f, 0).base, 10);
    EK_CHECK_STR(fx_tok(&f, 0).text, "255");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "0xFF");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 255);
    EK_CHECK_U64(fx_tok(&f, 0).base, 16);
    EK_CHECK_STR(fx_tok(&f, 0).text, "0xFF");
    fx_end(&f);

    fx_begin(&f, "0xff");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 255);
    fx_end(&f);

    fx_begin(&f, "0x1FEED");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 0x1FEED);
    fx_end(&f);

    fx_begin(&f, "0b1010");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 10);
    EK_CHECK_U64(fx_tok(&f, 0).base, 2);
    fx_end(&f);

    fx_begin(&f, "0");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 0);
    EK_CHECK_U64(fx_tok(&f, 0).base, 10);
    fx_end(&f);

    FX_KINDS("0xdead_beef", EK_TOK_INT, EK_TOK_EOF);
    fx_begin(&f, "0xdead_beef");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 0xdeadbeef);
    fx_end(&f);

    fx_begin(&f, "1_000_000");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 1000000);
    EK_CHECK_STR(fx_tok(&f, 0).text, "1_000_000");
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

/* The value is carried without silent wrap: magnitudes hold the value
   modulo 2^64 and overflow marks anything past that. */
static void test_overflow_carried(void)
{
    lex_fixture f;

    fx_begin(&f, "18446744073709551615");
    EK_CHECK_U64(fx_tok(&f, 0).overflow, 0);
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 18446744073709551615ULL);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "18446744073709551616");
    EK_CHECK_U64(fx_tok(&f, 0).overflow, 1);
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 0);
    EK_CHECK_U64(fx_errors(&f), 0); /* not diagnosable at lex, Clause 5.7.4 */
    fx_end(&f);

    fx_begin(&f, "99999999999999999999999999");
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_INT);
    EK_CHECK_U64(fx_tok(&f, 0).overflow, 1);
    EK_CHECK_STR(fx_tok(&f, 0).text, "99999999999999999999999999");
    fx_end(&f);

    fx_begin(&f, "0xFFFFFFFFFFFFFFFFF");
    EK_CHECK_U64(fx_tok(&f, 0).overflow, 1);
    fx_end(&f);
}

static void test_floats(void)
{
    expect_float("1.5", 1.5);
    expect_float("1e3", 1000.0);
    expect_float("1.5e3", 1500.0);
    expect_float("1.5e-3", 0.0015);
    expect_float("1.5e+3", 1500.0);
    expect_float("3.14159", 3.14159);
    expect_float("1_000.000_1", 1000.0001);

    FX_KINDS("1e3", EK_TOK_FLOAT, EK_TOK_EOF);
    FX_KINDS("2.0", EK_TOK_FLOAT, EK_TOK_EOF);
    FX_KINDS("-0.5", EK_TOK_MINUS, EK_TOK_FLOAT, EK_TOK_EOF);
}

static void test_dot_and_exponent_backtracking(void)
{
    /* a '.' joins only between digits; an 'e' only before exponent digits */
    FX_KINDS("1.", EK_TOK_INT, EK_TOK_DOT, EK_TOK_EOF);
    FX_KINDS(".5", EK_TOK_DOT, EK_TOK_INT, EK_TOK_EOF);
    FX_KINDS("1.e3", EK_TOK_INT, EK_TOK_DOT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("1e", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("1e+", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_PLUS, EK_TOK_EOF);
    FX_KINDS("1e-", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_MINUS, EK_TOK_EOF);
    /* the exponent admits no grouping, A.2.5 */
    FX_KINDS("1e1_0", EK_TOK_FLOAT, EK_TOK_IDENT, EK_TOK_EOF);
    /* a trailing 'e' that starts no exponent stays an identifier */
    FX_KINDS("1.5e", EK_TOK_FLOAT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("1.5e-", EK_TOK_FLOAT, EK_TOK_IDENT, EK_TOK_MINUS, EK_TOK_EOF);
}

static void test_no_suffixes(void)
{
    lex_fixture f;

    /* suffix spellings do not exist (Clause 5.11): longest match ends
       the literal and the letters lex as an identifier; A.3 rejects the
       rest. */
    FX_KINDS("1u8", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("1.0f", EK_TOK_FLOAT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("1e3x", EK_TOK_FLOAT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("1abc", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);

    /* incomplete prefixes backtrack the same way, Clause A.2.4 */
    FX_KINDS("0x", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("0b", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("0b2", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("0xg", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("0X", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("0x1g", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    FX_KINDS("0x1x", EK_TOK_INT, EK_TOK_IDENT, EK_TOK_EOF);
    /* hex literals have no fraction: the dot rule of 5.7.2 reads the
       digits, so the '.' is the member-call dot */
    FX_KINDS("0x0.5", EK_TOK_INT, EK_TOK_DOT, EK_TOK_INT, EK_TOK_EOF);

    fx_begin(&f, "0x");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 0);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);

    fx_begin(&f, "0x1g");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 1);
    EK_CHECK(fx_tok(&f, 1).text != NULL);
    fx_end(&f);
}
static void test_grouping_violations(void)
{
    lex_fixture f;

    fx_begin(&f, "1_");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_INT);
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 1);
    fx_end(&f);

    fx_begin(&f, "1__0");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 1);
    fx_end(&f);

    fx_begin(&f, "1_000_");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 1000);
    fx_end(&f);

    fx_begin(&f, "0x_FF");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    fx_end(&f);

    fx_begin(&f, "0b_1");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    fx_end(&f);

    fx_begin(&f, "1_000.000_");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    EK_CHECK_U64(fx_tok(&f, 0).kind, EK_TOK_FLOAT);
    EK_CHECK_STR(fx_tok(&f, 0).text, "1_000.000");
    fx_end(&f);

    fx_begin(&f, "0x_1");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    fx_end(&f);

    /* '0' alone takes no prefix, so '_' starts an identifier after the
       diagnosed stranded underscore */
    fx_begin(&f, "0_x");
    EK_CHECK(fx_has_error(&f, "E-digit-grouping"));
    fx_end(&f);

    fx_begin(&f, "0b1_0");
    EK_CHECK_U64(fx_tok(&f, 0).magnitude, 2);
    EK_CHECK_U64(fx_tok(&f, 0).base, 2);
    EK_CHECK_U64(fx_errors(&f), 0);
    fx_end(&f);
}

int main(void)
{
    test_integers();
    test_overflow_carried();
    test_floats();
    test_dot_and_exponent_backtracking();
    test_no_suffixes();
    test_grouping_violations();
    return ek_test_report("lex/numbers");
}
