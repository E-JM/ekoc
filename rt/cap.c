#include "internal.h"

#include <stdio.h>

/* The capability bundles the host's own surfaces are built over. A
   capability is a bundle of world-touching closures: the record holds
   one closure per session operation, and the compiler's injected
   unwrap and wrap are the identity between the capability value and
   its bundle, so the record here is layout-compatible with the
   compiler's one-field records and generated code reads the closures
   through its own layout. The shared bundle families and the value
   builders over the emitted unit's identities live in capval.c. */

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
                                                 cap_out_drop, NULL, NULL };

typedef char cap_out_slot_check
    [(offsetof(ek_cap_out, write)
      == sizeof(ek_obj) + EK_OUT_WRITE * sizeof(ek_val)) ? 1 : -1];

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

/* The write closure of the error bundle: the same bytes on stderr, so
   a program's diagnostics never mix into its data stream. */
static ek_val err_write(ek_val env, ek_val *args, size_t argc)
{
    ek_obj *s;

    (void)env;
    if (argc != 1 || !ek_val_is_ref(args[0])
        || args[0].as.ref->info->kind != EK_KIND_STR) {
        return ek_val_unit();
    }
    s = args[0].as.ref;
    fwrite(ek_str_cstr(s), 1, ek_str_len(s), stderr);
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

/* The Out bundle over the process's standard error, the constructor
   behind the compiler's Out.err row: the same bundle shape as the
   standard output, whose write closure lands on fd 2. The caller owns
   the returned value; releasing it releases the write closure. */
ek_val ek_cap_out_stderr(void)
{
    ek_cap_out *out = ek_rt_alloc_zeroed(sizeof *out);

    out->obj.info = &ek_typeinfo_cap_out;
    out->obj.rc = 1;
    out->write = ek_closure_new(err_write, ek_val_unit());
    return ek_val_ref(&out->obj);
}

/* The process-control token: an unforgeable value with no members of
   its own, carried to ek_proc_exit. */
typedef struct {
    ek_obj obj;
} ek_cap_proc_tok;

static const ek_typeinfo ek_typeinfo_cap_proc = { EK_KIND_STRUCT, NULL, NULL, NULL };

ek_val ek_cap_proc(void)
{
    ek_cap_proc_tok *p = ek_rt_alloc_zeroed(sizeof *p);

    p->obj.info = &ek_typeinfo_cap_proc;
    p->obj.rc = 1;
    return ek_val_ref(&p->obj);
}

/* Exits with the code: a program-requested exit passes its code
   through, so the status vocabulary stays with the program. */
void ek_proc_exit(ek_val proc, ek_val code)
{
    (void)proc;
    exit(code.tag == EK_VAL_INT ? (int)code.as.i : 0);
}
