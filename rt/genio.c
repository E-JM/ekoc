#include "internal.h"

#include <errno.h>

/* The IoError reasons and the plumbing of the file-family closures:
   one errno mapping, one failure writer, and the Ok and Err sides over
   the owned payloads. An owned argument dies with the call; a message
   becomes the fresh detail String the two payload reasons carry. */

ek_val ek_gen_ioerr(int kind, const char *msg)
{
    static const ek_typeinfo *kinds[] = {
        &ekT_sys$IoError$NotFound, &ekT_sys$IoError$Permission,
        &ekT_sys$IoError$Exists,   &ekT_sys$IoError$NotFile,
        &ekT_sys$IoError$Closed
    };
    ek_val d;
    ek_val r;

    if (kind <= EK_IO_CLOSED) {
        return ek_gen_zero(kinds[kind]);
    }
    d = ek_val_ref(ek_str_from_cstr(msg != NULL ? msg : ""));
    r = ek_gen_one(kind == EK_IO_INVALID_DATA ? &ekT_sys$IoError$InvalidData
                                           : &ekT_sys$IoError$Other,
                d);
    ek_release(d);
    return r;
}

const char *ek_gen_cstr(ek_val v)
{
    if (ek_val_is_ref(v) && v.as.ref->info->kind == EK_KIND_STR) {
        return ek_str_cstr(v.as.ref);
    }
    return "";
}

int ek_gen_io_kind(int e)
{
    switch (e) {
    case ENOENT:  return EK_IO_NOT_FOUND;
    case EEXIST:  return EK_IO_EXISTS;
    case EACCES:
    case EPERM:
    case EROFS:   return EK_IO_PERMISSION;
    case EISDIR:
    case ENOTDIR: return EK_IO_NOT_FILE;
    default:      return EK_IO_OTHER;
    }
}

int ek_gen_fail(int *err, ek_obj **detail, int kind, const char *msg)
{
    *err = kind;
    *detail = msg != NULL ? ek_str_from_cstr(msg) : NULL;
    return 0;
}

ek_val ek_gen_ok(ek_val payload)
{
    ek_val r = ek_gen_result_ok(payload);

    ek_release(payload);
    return r;
}

ek_val ek_gen_err(int kind, const char *msg)
{
    ek_val ioerr = ek_gen_ioerr(kind, msg);
    ek_val r = ek_gen_result_err(ioerr);

    ek_release(ioerr);
    return r;
}

ek_val ek_gen_err_of(int kind, ek_obj *detail)
{
    ek_val r = ek_gen_err(kind, detail != NULL ? ek_str_cstr(detail) : NULL);

    if (detail != NULL) {
        ek_release(ek_val_ref(detail));
    }
    return r;
}
