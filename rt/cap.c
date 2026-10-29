#include "internal.h"

#include <stdio.h>

/* The capability bundles the host's own surfaces are built over. A
   capability is a bundle of world-touching closures: the record holds
   one closure per session operation, and the compiler's injected
   unwrap and wrap are the identity between the capability value and
   its bundle, so the record here is layout-compatible with the
   compiler's one-field records and generated code reads the closures
   through its own layout. */

/* The session-write bundle of Out: the single write closure. The
   typeinfo's drop releases the closure with the record. */
typedef struct {
    ek_obj obj;
    ek_val write;
} ek_cap_out;

static void cap_out_drop(ek_obj *obj)
{
    ek_release(((ek_cap_out *)obj)->write);
}

static const ek_typeinfo ek_typeinfo_cap_out = { EK_KIND_STRUCT,
                                                 cap_out_drop };

/* The write closure of the standard output bundle: the argument's
   bytes, without the stored terminator, land on stdout; the stream
   keeps its default buffering. */
static ek_val out_write(ek_val env, ek_val *args, size_t argc)
{
    ek_obj *s;

    (void)env;
    if (argc != 1 || !ek_val_is_ref(args[0])
        || args[0].as.ref->info->kind != EK_KIND_STR) {
        return ek_val_unit();
    }
    s = args[0].as.ref;
    fwrite(ek_str_cstr(s), 1, ek_str_len(s), stdout);
    return ek_val_unit();
}

/* The Out bundle over the process's standard output. The caller owns
   the returned value; releasing it releases the write closure. */
ek_val ek_cap_out_stdout(void)
{
    ek_cap_out *out = ek_rt_alloc_zeroed(sizeof *out);

    out->obj.info = &ek_typeinfo_cap_out;
    out->obj.rc = 1;
    out->write = ek_closure_new(out_write, ek_val_unit());
    return ek_val_ref(&out->obj);
}
