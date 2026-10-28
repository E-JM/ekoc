#include "harness.h"

#include "support/xid.h"
#include "support/xid_tables.h"

/* Spot checks assert what the committed tables say. '_' is
   XID_Continue but not XID_Start: the lexer adds '_' as a first
   character on top of the raw predicates, so ek_xid_start stays 0 for
   it here. */

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
    /* raw property: continue yes, start no; the '_' start rule is
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

static void test_pascal_start(void)
{
    /* uppercase and titlecase letters only */
    EK_CHECK_U64(ek_xid_pascal_start('A'), 1);
    EK_CHECK_U64(ek_xid_pascal_start('Z'), 1);
    EK_CHECK_U64(ek_xid_pascal_start('a'), 0);
    EK_CHECK_U64(ek_xid_pascal_start('0'), 0);
    EK_CHECK_U64(ek_xid_pascal_start(0x5F), 0); /* '_' */
    EK_CHECK_U64(ek_xid_pascal_start(0x0398), 1); /* greek capital theta */
    EK_CHECK_U64(ek_xid_pascal_start(0x03BB), 0); /* greek small lambda */
    EK_CHECK_U64(ek_xid_pascal_start(0x0410), 1); /* cyrillic capital A */
    EK_CHECK_U64(ek_xid_pascal_start(0x0430), 0); /* cyrillic small a */
    EK_CHECK_U64(ek_xid_pascal_start(0x01C5), 1); /* latin capital D with
                                                     small z with caron,
                                                     category Lt */
    EK_CHECK_U64(ek_xid_pascal_start(0x4E00), 0); /* CJK ideograph, Lo */
    EK_CHECK_U64(ek_xid_pascal_start(0x00E9), 0); /* small e with acute */
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

static int ranges_sorted(const struct ek_xid_range *tab, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (tab[i].lo > tab[i].hi) {
            return 0;
        }
        if (i > 0 && tab[i - 1].hi >= tab[i].lo) {
            return 0;
        }
    }
    return 1;
}

static void test_table_invariants(void)
{
    /* the binary search only works on sorted, non-overlapping ranges */
    EK_CHECK(ranges_sorted(ek_xid_start_table, ek_xid_start_table_len));
    EK_CHECK(ranges_sorted(ek_xid_continue_table,
                           ek_xid_continue_table_len));
    EK_CHECK(ranges_sorted(ek_pascal_start_table,
                           ek_pascal_start_table_len));
}

static void test_binary_search_edges(void)
{
    /* first and last entries of each table and the code points just
       outside them */
    EK_CHECK_U64(ek_xid_start(0x0040), 0);
    EK_CHECK_U64(ek_xid_start(0x005A), 1);
    EK_CHECK_U64(ek_xid_start(0x005B), 0);
    EK_CHECK_U64(ek_xid_continue(0x002F), 0);
    EK_CHECK_U64(ek_xid_continue(0x0030), 1);
    EK_CHECK_U64(ek_xid_continue(0x0039), 1);
    EK_CHECK_U64(ek_xid_continue(0x003A), 0);

    EK_CHECK_U64(ek_xid_start(0x2FFFF), 0);
    EK_CHECK_U64(ek_xid_start(0x30000), 1);
    EK_CHECK_U64(ek_xid_start(0x3134A), 1);
    EK_CHECK_U64(ek_xid_start(0x3134B), 0);
    EK_CHECK_U64(ek_xid_start(0x3134F), 0);
    EK_CHECK_U64(ek_xid_start(0x31350), 1);
    EK_CHECK_U64(ek_xid_start(0x323AF), 1);
    EK_CHECK_U64(ek_xid_start(0x323B0), 0);
    EK_CHECK_U64(ek_xid_continue(0x323B0), 0);

    EK_CHECK_U64(ek_xid_continue(0xE00FF), 0);
    EK_CHECK_U64(ek_xid_continue(0xE0100), 1);
    EK_CHECK_U64(ek_xid_continue(0xE01EF), 1);
    EK_CHECK_U64(ek_xid_continue(0xE01F0), 0);
    EK_CHECK_U64(ek_xid_start(0xE0100), 0);

    EK_CHECK_U64(ek_xid_start(0xD7FF), 0);
    EK_CHECK_U64(ek_xid_continue(0xD7FF), 0);
    EK_CHECK_U64(ek_xid_start(0xE000), 0);
    EK_CHECK_U64(ek_xid_continue(0xE000), 0);
    EK_CHECK_U64(ek_xid_start(0xFFFE), 0);
    EK_CHECK_U64(ek_xid_start(0xFFFF), 0);
}

int main(void)
{
    test_ascii();
    test_underscore();
    test_greek();
    test_cjk();
    test_marks_and_punctuation();
    test_symbols_and_emoji();
    test_pascal_start();
    test_boundaries();
    test_table_invariants();
    test_binary_search_edges();
    return ek_test_report("support/xid");
}
