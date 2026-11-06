#ifndef EK_SUPPORT_VOCAB_H
#define EK_SUPPORT_VOCAB_H

#include <stddef.h>

/* The vocabulary type names the compiler itself supplies: the nine
   capability types and the three handle types. The resolver, the
   checker, and the lowerer all classify with this one table, so a name
   added here is a capability or a handle in every stage at once. */
extern const char *const ek_capability_names[];
extern const size_t ek_capability_name_count;
extern const char *const ek_handle_names[];
extern const size_t ek_handle_name_count;

int ek_is_capability_name(const char *name);
int ek_is_handle_name(const char *name);

#endif
