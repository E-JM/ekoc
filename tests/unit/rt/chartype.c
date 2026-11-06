#include "harness.h"

#include <stdint.h>

#include "ekort.h"

/* Char classification: the ASCII letter and digit predicates, the
   Unicode White_Space property, and the XID properties the lexer's
   identifier rule builds on. */

static int alpha(uint32_t cp)
{
    return ek_char_is_alpha(ek_val_char(cp));
}

static int digit(uint32_t cp)
{
    return ek_char_is_digit(ek_val_char(cp));
}

static int alnum(uint32_t cp)
{
    return ek_char_is_alnum(ek_val_char(cp));
}

static int space(uint32_t cp)
{
    return ek_char_is_space(ek_val_char(cp));
}

static int upper(uint32_t cp)
{
    return ek_char_is_upper(ek_val_char(cp));
}

static int lower(uint32_t cp)
{
    return ek_char_is_lower(ek_val_char(cp));
}

static int xid_start(uint32_t cp)
{
    return ek_char_is_xid_start(ek_val_char(cp));
}

static int xid_continue(uint32_t cp)
{
    return ek_char_is_xid_continue(ek_val_char(cp));
}

static void ascii_letters_and_digits(void)
{
    EK_CHECK(upper('A') && upper('Z') && !upper('a') && !upper('0'));
    EK_CHECK(lower('a') && lower('z') && !lower('A') && !lower('0'));
    EK_CHECK(alpha('a') && alpha('Z') && !alpha('0') && !alpha('_'));
    EK_CHECK(digit('0') && digit('9') && !digit('a') && !digit('/'));
    EK_CHECK(alnum('a') && alnum('Z') && alnum('5'));
    EK_CHECK(!alnum('_') && !alnum('-') && !alnum(' '));
    EK_CHECK(!alpha(0xE9) && !upper(0xC9)); /* Latin-1 stays out */
}

static void white_space_is_the_unicode_property(void)
{
    EK_CHECK(space(' ') && space('\t') && space('\n') && space('\r'));
    EK_CHECK(space(0x0B) && space(0x0C));
    EK_CHECK(space(0x85) && space(0xA0) && space(0x1680));
    EK_CHECK(space(0x2000) && space(0x200A) && !space(0x200B));
    EK_CHECK(space(0x2028) && space(0x2029));
    EK_CHECK(space(0x202F) && space(0x205F) && space(0x3000));
    EK_CHECK(!space(0x3001) && !space('x'));
}

static void xid_is_the_unicode_property(void)
{
    EK_CHECK(xid_start('a') && xid_start('Z') && xid_start(0x3BB));
    EK_CHECK(xid_start(0x4E00)); /* Han */
    EK_CHECK(!xid_start('_') && !xid_start('0') && !xid_start('-'));
    EK_CHECK(xid_continue('_') && xid_continue('0') && xid_continue(0x3BB));
    EK_CHECK(xid_continue(0x0301)); /* a combining mark */
    EK_CHECK(!xid_continue('-') && !xid_continue(' '));
}

static void a_value_that_is_not_a_char_is_false(void)
{
    EK_CHECK(!ek_char_is_alpha(ek_val_int('a')));
    EK_CHECK(!ek_char_is_digit(ek_val_unit()));
    EK_CHECK(!ek_char_is_alnum(ek_val_int(1)));
    EK_CHECK(!ek_char_is_space(ek_val_int(' ')));
    EK_CHECK(!ek_char_is_upper(ek_val_int('A')));
    EK_CHECK(!ek_char_is_lower(ek_val_int('a')));
    EK_CHECK(!ek_char_is_xid_start(ek_val_int('a')));
    EK_CHECK(!ek_char_is_xid_continue(ek_val_int('a')));
}

/* The lexer's identifier rule: XID plus a leading underscore, which
   the raw property leaves out. */
static void underscore_is_not_an_xid_start(void)
{
    EK_CHECK(!xid_start('_'));
    EK_CHECK(xid_continue('_'));
}

int main(void)
{
    ascii_letters_and_digits();
    white_space_is_the_unicode_property();
    xid_is_the_unicode_property();
    a_value_that_is_not_a_char_is_false();
    underscore_is_not_an_xid_start();
    return ek_test_report("rt/chartype");
}
