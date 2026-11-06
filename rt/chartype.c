#include "ekort.h"
#include "xid_tables.h"

/* Char classification: the ASCII letter and digit predicates, the
   Unicode White_Space property, and the Unicode XID properties the
   lexer's identifier rule is built on. A value that is not a Char
   classifies as false, a tag the typed surface never produces. */

static int code_of(ek_val c, uint32_t *out)
{
    if (c.tag != EK_VAL_CHAR)
        return 0;
    *out = c.as.c;
    return 1;
}

static int ascii_range(uint32_t cp, uint32_t lo, uint32_t hi)
{
    return cp >= lo && cp <= hi;
}

int ek_char_is_upper(ek_val c)
{
    uint32_t cp;

    return code_of(c, &cp) && ascii_range(cp, 'A', 'Z');
}

int ek_char_is_lower(ek_val c)
{
    uint32_t cp;

    return code_of(c, &cp) && ascii_range(cp, 'a', 'z');
}

int ek_char_is_digit(ek_val c)
{
    uint32_t cp;

    return code_of(c, &cp) && ascii_range(cp, '0', '9');
}

int ek_char_is_alpha(ek_val c)
{
    uint32_t cp;

    return code_of(c, &cp)
        && (ascii_range(cp, 'A', 'Z') || ascii_range(cp, 'a', 'z'));
}

int ek_char_is_alnum(ek_val c)
{
    return ek_char_is_alpha(c) || ek_char_is_digit(c);
}

/* The Unicode White_Space property, the set String.trim removes and
   the one the specification names. */
int ek_char_is_space(ek_val c)
{
    uint32_t cp;

    if (!code_of(c, &cp))
        return 0;
    return cp == 0x09 || cp == 0x0A || cp == 0x0B || cp == 0x0C
        || cp == 0x0D || cp == 0x20 || cp == 0x85 || cp == 0xA0
        || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
        || cp == 0x2028 || cp == 0x2029 || cp == 0x202F
        || cp == 0x205F || cp == 0x3000;
}

static int table_has(const struct ek_xid_range *table, size_t len,
                     uint32_t cp)
{
    size_t lo = 0, hi = len;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        if (cp < table[mid].lo) {
            hi = mid;
        } else if (cp > table[mid].hi) {
            lo = mid + 1;
        } else {
            return 1;
        }
    }
    return 0;
}

int ek_char_is_xid_start(ek_val c)
{
    uint32_t cp;

    return code_of(c, &cp)
        && table_has(ek_xid_start_table, ek_xid_start_table_len, cp);
}

int ek_char_is_xid_continue(ek_val c)
{
    uint32_t cp;

    return code_of(c, &cp)
        && table_has(ek_xid_continue_table, ek_xid_continue_table_len, cp);
}
