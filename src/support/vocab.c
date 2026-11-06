#include "support/vocab.h"

#include <string.h>

const char *const ek_capability_names[] = {
    "Out", "In", "Fs", "Env", "Args", "Clock", "Rand", "Proc", "Conc"
};
const size_t ek_capability_name_count =
    sizeof ek_capability_names / sizeof *ek_capability_names;

const char *const ek_handle_names[] = { "File", "Scope", "Task" };
const size_t ek_handle_name_count =
    sizeof ek_handle_names / sizeof *ek_handle_names;

static int in_table(const char *const *names, size_t n, const char *name)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (strcmp(names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

int ek_is_capability_name(const char *name)
{
    return name && in_table(ek_capability_names, ek_capability_name_count,
                            name);
}

int ek_is_handle_name(const char *name)
{
    return name && in_table(ek_handle_names, ek_handle_name_count, name);
}
