#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* The argument-vector bundle: all builds the argument list fresh on
   every call, the strings in order, without the program name. The
   copies live until the bundle is released: every list all builds
   carries its own strings, so the drop reclaims them with the
   record. */

typedef struct {
    ek_obj obj;
    char **items;
    size_t n;
} ek_cap_argv;

/* Releases the copied strings and the vector that orders them; the
   release path frees the record itself. */
static void args_drop(ek_obj *obj)
{
    ek_cap_argv *av = (ek_cap_argv *)obj;
    size_t i;

    for (i = 0; i < av->n; i++) {
        free(av->items[i]);
    }
    free(av->items);
}

static const ek_typeinfo ek_typeinfo_cap_argv = {
    EK_KIND_STRUCT, args_drop, NULL, NULL
};

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
        ek_val next = ek_cons(s, list);

        /* the new cell takes the previous head as its tail, so the
           birth count the builder held moves with it */
        ek_release(s);
        ek_release(list);
        list = next;
    }
    return list;
}

ek_val ek_cap_args(int argc, char **argv)
{
    ek_cap_argv *av = ek_rt_alloc_zeroed(sizeof *av);
    ek_val env;
    ek_val bundle;
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
    /* A bundle over the copied vector: the record moves into the
       closure's environment. */
    env = ek_val_ref(&av->obj);
    bundle = ek_bundle1_new(ek_closure_new(args_all, env));
    ek_release(env);
    return bundle;
}
