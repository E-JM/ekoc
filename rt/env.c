#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The environment bundle: one read closure over the process
   environment. An unset name reads as None. Both results are built
   through the emitted unit's identities. */

static ek_val env_get(ek_val env, ek_val *args, size_t argc)
{
    const char *hit;

    (void)env;
    if (argc != 1 || !ek_val_is_ref(args[0])
        || args[0].as.ref->info->kind != EK_KIND_STR) {
        return ek_gen_option_none();
    }
    hit = getenv(ek_str_cstr(args[0].as.ref));
    if (hit == NULL) {
        return ek_gen_option_none();
    }
    {
        ek_val s = ek_val_ref(ek_str_from_cstr(hit));
        ek_val some = ek_gen_option_some(s);

        ek_release(s);
        return some;
    }
}

ek_val ek_cap_env_os(void)
{
    return ek_bundle1_new(ek_closure_new(env_get, ek_val_unit()));
}
