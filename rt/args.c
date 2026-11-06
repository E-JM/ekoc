#include "internal.h"

#include <string.h>

/* The argument-vector bundle: all builds the argument list fresh on
   every call, the strings in order, without the program name. The
   copied vector lives for the process; leaks are permitted. */

typedef struct {
    ek_obj obj;
    char **items;
    size_t n;
} ek_cap_argv;

static const ek_typeinfo ek_typeinfo_cap_argv = { EK_KIND_STRUCT, NULL, NULL, NULL };

static ek_val args_all(ek_val env, ek_val *args, size_t argc)
{
    ek_cap_argv *av;
    ek_val list = ek_nil_val();
    size_t i;

    (void)args;
    if (argc != 0 || !ek_val_is_ref(env)
        || env.as.ref->info != &ek_typeinfo_cap_argv) {
        return list;
    }
    av = (ek_cap_argv *)env.as.ref;
    for (i = av->n; i-- > 0;) {
        ek_val s = ek_val_ref(ek_str_from_cstr(av->items[i]));

        list = ek_cons(s, list);
        ek_release(s);
    }
    return list;
}

static ek_val argv_env(ek_cap_argv *av)
{
    return ek_closure_new(args_all, ek_val_ref((ek_obj *)av));
}

ek_val ek_cap_args(int argc, char **argv)
{
    ek_cap_argv *av = ek_rt_alloc_zeroed(sizeof *av);
    size_t i;

    av->obj.info = &ek_typeinfo_cap_argv;
    av->obj.rc = 1;
    av->n = argc > 0 ? (size_t)argc : 0;
    av->items = ek_rt_alloc_zeroed(av->n * sizeof *av->items);
    for (i = 0; i < av->n; i++) {
        size_t len = strlen(argv[i]);
        char *copy = ek_rt_alloc_zeroed(len + 1);

        memcpy(copy, argv[i], len);
        av->items[i] = copy;
    }
    return ek_bundle1_new(argv_env(av));
}
