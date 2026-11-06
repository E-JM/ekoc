#ifndef EK_RT_XID_TABLES_H
#define EK_RT_XID_TABLES_H

/* Private to rt: the generated Unicode XID range tables the Char
   predicates classify with. */

#include <stddef.h>
#include <stdint.h>

struct ek_xid_range {
    uint32_t lo;
    uint32_t hi; /* inclusive */
};

extern const struct ek_xid_range ek_xid_start_table[];
extern const size_t ek_xid_start_table_len;
extern const struct ek_xid_range ek_xid_continue_table[];
extern const size_t ek_xid_continue_table_len;

#endif
