#include <stddef.h>

#include "internal.h"

/* The FileInfo record of the capability module: the fields in the
   declaration's order, the modification time as a Time value. */

typedef struct {
    ek_obj obj;
    ek_val f_size;
    ek_val f_mtime;
    ek_val f_is_dir;
} ek_gen_finfo;

/* The record the builder fills and the record the unit reads are the
   same layout: the header, then the fields in the published order. */
typedef char ek_gen_finfo_slots
    [(sizeof(ek_gen_finfo)
      == sizeof(ek_obj) + EK_FILEINFO_SLOTS * sizeof(ek_val)) ? 1 : -1];
typedef char ek_gen_finfo_size
    [(offsetof(ek_gen_finfo, f_size)
      == sizeof(ek_obj) + EK_FILEINFO_SIZE * sizeof(ek_val)) ? 1 : -1];
typedef char ek_gen_finfo_mtime
    [(offsetof(ek_gen_finfo, f_mtime)
      == sizeof(ek_obj) + EK_FILEINFO_MTIME * sizeof(ek_val)) ? 1 : -1];
typedef char ek_gen_finfo_is_dir
    [(offsetof(ek_gen_finfo, f_is_dir)
      == sizeof(ek_obj) + EK_FILEINFO_IS_DIR * sizeof(ek_val)) ? 1 : -1];

ek_val ek_gen_file_info(uint64_t size, ek_val mtime, int is_dir)
{
    ek_gen_finfo *o = ek_rt_alloc_zeroed(sizeof *o);

    o->obj.info = &ekT_sys$FileInfo;
    o->obj.rc = 1;
    o->f_size = ek_val_uint(size);
    o->f_mtime = mtime;
    o->f_is_dir = ek_bool(is_dir);
    ek_retain(mtime);
    return ek_val_ref(&o->obj);
}
