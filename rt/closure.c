#include "internal.h"

/* Closures pair a function with its captured environment. The
   environment is a reference like any other: retained at capture and
   released when the closure dies. Calling goes through the uniform
   ABI; a value that is not a closure panics. */

typedef struct {
    ek_obj obj;
    ek_closure_fn call;
    ek_val env;
} ek_closure;

/* Nonzero when v refers to a closure. */
static int is_closure(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref != NULL
        && v.as.ref->info->kind == EK_KIND_CLOSURE;
}

ek_val ek_closure_new(ek_closure_fn call, ek_val env)
{
    ek_closure *closure = ek_rt_alloc_zeroed(sizeof *closure);

    closure->obj.info = &ek_typeinfo_closure;
    closure->obj.rc = 1;
    closure->call = call;
    ek_retain(env);
    closure->env = env;
    return ek_val_ref(&closure->obj);
}

ek_val ek_closure_call(ek_val c, ek_val *args, size_t argc)
{
    const ek_closure *closure;

    if (!is_closure(c))
        ek_panic("P-panic", "call of a non-closure");
    closure = (const ek_closure *)c.as.ref;
    return closure->call(closure->env, args, argc);
}

ek_val ek_closure_env(ek_obj *obj)
{
    return ((const ek_closure *)obj)->env;
}

void ek_closure_drop(ek_obj *obj)
{
    ek_release(((ek_closure *)obj)->env);
}
