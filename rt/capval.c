#include "internal.h"

#include <errno.h>
#include <string.h>

/* The values the capability bundles hand back, built over the
   generated identities declared in internal.h. Every builder here
   constructs the layout the compiler's families use -- the object
   header, then the payload slots in declaration order -- and stamps it
   with the emitted unit's typeinfo, so generated pattern matches and
   drops treat the value as one of their own. Like the generated
   construction helpers, a builder retains every reference it stores
   and the caller releases its own. */

/* The one-payload shape of a constructor family. */
typedef struct {
    ek_obj obj;
    ek_val p0;
} ek_gen_p0_shape;

/* The fieldless shape of a zero-payload constructor family. */
typedef struct {
    ek_obj obj;
} ek_gen_bare_shape;

/* The FileInfo record of the capability module: the fields in the
   declaration's order. */
typedef struct {
    ek_obj obj;
    ek_val f_size;
    ek_val f_mtime;
    ek_val f_is_dir;
} ek_gen_finfo;

ek_val ek_gen_one(const ek_typeinfo *info, ek_val p0)
{
    ek_gen_p0_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = info;
    o->obj.rc = 1;
    o->p0 = p0;
    ek_retain(p0);
    return ek_val_ref(&o->obj);
}

ek_val ek_gen_zero(const ek_typeinfo *info)
{
    ek_gen_bare_shape *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = info;
    o->obj.rc = 1;
    return ek_val_ref(&o->obj);
}

ek_val ek_gen_result_ok(ek_val payload)
{
    return ek_gen_one(&ekT_std_Result_Ok, payload);
}

ek_val ek_gen_result_err(ek_val payload)
{
    return ek_gen_one(&ekT_std_Result_Err, payload);
}

ek_val ek_gen_option_some(ek_val payload)
{
    return ek_gen_one(&ekT_std_Option_Some, payload);
}

ek_val ek_gen_option_none(void)
{
    return ek_gen_zero(&ekT_std_Option_None);
}

ek_val ek_gen_ioerr(int kind, const char *msg)
{
    static const ek_typeinfo *kinds[] = {
        &ekT_cap_IoError_NotFound, &ekT_cap_IoError_Permission,
        &ekT_cap_IoError_Exists,   &ekT_cap_IoError_NotFile,
        &ekT_cap_IoError_Closed
    };
    ek_val d;
    ek_val r;

    if (kind <= EK_IO_CLOSED) {
        return ek_gen_zero(kinds[kind]);
    }
    d = ek_val_ref(ek_str_from_cstr(msg != NULL ? msg : ""));
    r = ek_gen_one(kind == EK_IO_INVALID_DATA ? &ekT_cap_IoError_InvalidData
                                           : &ekT_cap_IoError_Other,
                d);
    ek_release(d);
    return r;
}

ek_val ek_gen_time(int64_t ns)
{
    return ek_gen_one(&ekT_time_Time_Time, ek_val_int(ns));
}

ek_val ek_gen_mono(int64_t ns)
{
    return ek_gen_one(&ekT_time_Mono_Mono, ek_val_int(ns));
}

ek_val ek_gen_file_info(uint64_t size, ek_val mtime, int is_dir)
{
    ek_gen_finfo *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = &ekT_cap_FileInfo;
    o->obj.rc = 1;
    o->f_size = ek_val_uint(size);
    o->f_mtime = mtime;
    o->f_is_dir = ek_bool(is_dir);
    ek_retain(mtime);
    return ek_val_ref(&o->obj);
}

int64_t ek_gen_ns(ek_val v)
{
    if (!ek_val_is_ref(v)) {
        return 0;
    }
    return ((const ek_gen_p0_shape *)v.as.ref)->p0.as.i;
}

/* The shared bundle families: the header followed by the member
   closures. The drops release every member with the record. */

typedef struct {
    ek_obj obj;
    ek_val a;
} ek_bundle1;

typedef struct {
    ek_obj obj;
    ek_val a;
    ek_val b;
    ek_val c;
} ek_bundle3;

typedef struct {
    ek_obj obj;
    ek_val f[16];
} ek_bundle16;

static void bundle1_drop(ek_obj *obj)
{
    ek_release(((ek_bundle1 *)obj)->a);
}

static void bundle3_drop(ek_obj *obj)
{
    ek_bundle3 *b = (ek_bundle3 *)obj;

    ek_release(b->a);
    ek_release(b->b);
    ek_release(b->c);
}

static void bundle16_drop(ek_obj *obj)
{
    ek_bundle16 *b = (ek_bundle16 *)obj;
    size_t i;

    for (i = 0; i < sizeof b->f / sizeof *b->f; i++) {
        ek_release(b->f[i]);
    }
}

static const ek_typeinfo bundle1_info = { EK_KIND_STRUCT, bundle1_drop };
static const ek_typeinfo bundle3_info = { EK_KIND_STRUCT, bundle3_drop };
static const ek_typeinfo bundle16_info = { EK_KIND_STRUCT, bundle16_drop };

ek_val ek_bundle1_new(ek_val a)
{
    ek_bundle1 *b = ek_rt_alloc_zeroed(sizeof *b);

    b->obj.info = &bundle1_info;
    b->obj.rc = 1;
    b->a = a;
    return ek_val_ref(&b->obj);
}

ek_val ek_bundle3_new(ek_val a, ek_val b, ek_val c)
{
    ek_bundle3 *r = ek_rt_alloc_zeroed(sizeof *r);

    r->obj.info = &bundle3_info;
    r->obj.rc = 1;
    r->a = a;
    r->b = b;
    r->c = c;
    return ek_val_ref(&r->obj);
}

ek_val ek_bundle16_new(const ek_val *fields)
{
    ek_bundle16 *b = ek_rt_alloc_zeroed(sizeof *b);
    size_t i;

    b->obj.info = &bundle16_info;
    b->obj.rc = 1;
    for (i = 0; i < sizeof b->f / sizeof *b->f; i++) {
        b->f[i] = fields[i];
    }
    return ek_val_ref(&b->obj);
}

/* The plumbing of the file-family closures: one errno mapping, one
   failure writer, and the Ok and Err sides over the owned payloads.
   An owned argument dies with the call; a message becomes the fresh
   detail String the two payload reasons carry. */

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
