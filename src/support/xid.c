#include "support/xid.h"

#include "support/xid_tables.h"

static int table_has(const struct ek_xid_range *table, size_t len, uint32_t cp)
{
    size_t lo = 0;
    size_t hi = len;

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

int ek_xid_start(uint32_t cp)
{
    return table_has(ek_xid_start_table, ek_xid_start_table_len, cp);
}

int ek_xid_continue(uint32_t cp)
{
    return table_has(ek_xid_continue_table, ek_xid_continue_table_len, cp);
}

int ek_xid_pascal_start(uint32_t cp)
{
    return table_has(ek_pascal_start_table, ek_pascal_start_table_len, cp);
}
