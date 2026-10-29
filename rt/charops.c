#include "ekort.h"

/* Char's code-point bridge: a Char is one Unicode scalar value, and
   the Int slot carries the code. A tag outside the bridge's own slot
   is unreachable from typed code and yields the neutral result. */

ek_val ek_char_to_code(ek_val c)
{
    if (c.tag != EK_VAL_CHAR)
        return ek_val_unit();
    return ek_val_int((int64_t)c.as.c);
}

int ek_try_char_from_code(ek_val n, uint32_t *out)
{
    if (n.tag != EK_VAL_INT)
        return 0;
    if (n.as.i < 0 || n.as.i > 0x10FFFF)
        return 0;
    if (n.as.i >= 0xD800 && n.as.i <= 0xDFFF)
        return 0;
    *out = (uint32_t)n.as.i;
    return 1;
}
