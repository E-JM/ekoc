#include "harness.h"

#include "support/xid.h"

/* Spot checks assert what the committed tables say, which is what the
   host Python's str.isidentifier() derives (Unicode 16.0.0 at
   generation time). '_' is XID_Continue but not XID_Start: clause 5.5.1
   adds '_' as a first character at the lex layer, so the raw predicate
   here stays 0 for it. */

static void test_ascii(void)
{
    EK_CHECK_U64(ek_xid_start('a'), 1);
    EK_CHECK_U64(ek_xid_continue('a'), 1);
    EK_CHECK_U64(ek_xid_start('z'), 1);
    EK_CHECK_U64(ek_xid_start('A'), 1);
    EK_CHECK_U64(ek_xid_start('5'), 0);
    EK_CHECK_U64(ek_xid_continue('5'), 1);
    EK_CHECK_U64(ek_xid_start(' '), 0);
    EK_CHECK_U64(ek_xid_continue(' '), 0);
    EK_CHECK_U64(ek_xid_start('-'), 0);
    EK_CHECK_U64(ek_xid_continue('-'), 0);
}

static void test_underscore(void)
{
    /* raw property: continue yes, start no; 5.5.1's '_' start rule is
       the lexer's business */
    EK_CHECK_U64(ek_xid_start(0x5F), 0);
    EK_CHECK_U64(ek_xid_continue(0x5F), 1);
}

static void test_greek(void)
{
    EK_CHECK_U64(ek_xid_start(0x03BB), 1); /* lambda */
    EK_CHECK_U64(ek_xid_continue(0x03BB), 1);
    EK_CHECK_U64(ek_xid_start(0x0398), 1); /* theta, uppercase */
    EK_CHECK_U64(ek_xid_continue(0x0398), 1);
}

static void test_cjk(void)
{
    EK_CHECK_U64(ek_xid_start(0x4E00), 1); /* CJK ideograph, one */
    EK_CHECK_U64(ek_xid_continue(0x4E00), 1);
    EK_CHECK_U64(ek_xid_start(0x9FFF), 1);
    EK_CHECK_U64(ek_xid_continue(0x9FFF), 1);
}

static void test_marks_and_punctuation(void)
{
    /* combining marks continue words but cannot start them */
    EK_CHECK_U64(ek_xid_start(0x0301), 0); /* combining acute */
    EK_CHECK_U64(ek_xid_continue(0x0301), 1);
    /* other_id_continue punctuation */
    EK_CHECK_U64(ek_xid_start(0x00B7), 0); /* middle dot */
    EK_CHECK_U64(ek_xid_continue(0x00B7), 1);
    EK_CHECK_U64(ek_xid_start(0x0387), 0); /* greek ano teleia */
    EK_CHECK_U64(ek_xid_continue(0x0387), 1);
    /* other_id_start letterlikes */
    EK_CHECK_U64(ek_xid_start(0x2118), 1); /* script capital P */
    EK_CHECK_U64(ek_xid_continue(0x2118), 1);
}

static void test_symbols_and_emoji(void)
{
    EK_CHECK_U64(ek_xid_start(0x1F600), 0); /* grinning face */
    EK_CHECK_U64(ek_xid_continue(0x1F600), 0);
    EK_CHECK_U64(ek_xid_start(0x00A9), 0); /* copyright */
    EK_CHECK_U64(ek_xid_continue(0x00A9), 0);
    EK_CHECK_U64(ek_xid_start(0x2192), 0); /* rightwards arrow */
    EK_CHECK_U64(ek_xid_continue(0x2192), 0);
}

static void test_boundaries(void)
{
    EK_CHECK_U64(ek_xid_start(0x0000), 0);
    EK_CHECK_U64(ek_xid_continue(0x0000), 0);
    EK_CHECK_U64(ek_xid_start(0xD800), 0); /* surrogate */
    EK_CHECK_U64(ek_xid_continue(0xD800), 0);
    EK_CHECK_U64(ek_xid_start(0x10FFFF), 0);
    EK_CHECK_U64(ek_xid_continue(0x10FFFF), 0);
    /* edges of the ascii range in the tables */
    EK_CHECK_U64(ek_xid_start('Z' + 1), 0); /* '[' */
    EK_CHECK_U64(ek_xid_continue('Z' + 1), 0);
    EK_CHECK_U64(ek_xid_start('`' - 1), 0); /* '^'; 0x5F '_' asserted above */
    EK_CHECK_U64(ek_xid_start('z' + 1), 0); /* '{' */
    EK_CHECK_U64(ek_xid_continue('z' + 1), 0);
}

int main(void)
{
    test_ascii();
    test_underscore();
    test_greek();
    test_cjk();
    test_marks_and_punctuation();
    test_symbols_and_emoji();
    test_boundaries();
    return ek_test_report("support/xid");
}
