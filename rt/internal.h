#ifndef EK_RT_INTERNAL_H
#define EK_RT_INTERNAL_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "ekort.h"

/* Shared declarations inside the runtime. Not part of the public
   surface and not visible to compiled programs. */

/* The generated constructor families whose identity the runtime shares
   with the emitted unit. A constructor pattern in generated code tests
   pointer equality against the unit's own typeinfo, so every value the
   bundles hand back carries the unit's typeinfo, never a runtime one.
   The names are the lowerer's stems, one per std, capability, and time
   constructor the bundle closures build; the emitted unit defines each
   of them whenever the main manifest requests the capability whose
   bundle builds it (the compiler registers the set), so the references
   resolve at link time. The bundle records themselves stay
   runtime-typed: they are only field-read and dropped, never
   pattern-matched.

   The layout contract, the same one range.c reads generated structs
   through: a family is the object header followed by the payload slots
   in declaration order, one ek_val each -- a constructor payload in
   p0, record fields in the declaration's field order. The generated
   drop releases every slot, so a runtime-built value with the unit's
   typeinfo is reclaimed exactly like a unit-built one. */
extern const ek_typeinfo ekT_std_Result_Ok;
extern const ek_typeinfo ekT_std_Result_Err;
extern const ek_typeinfo ekT_std_Option_Some;
extern const ek_typeinfo ekT_std_Option_None;
extern const ek_typeinfo ekT_cap_IoError_NotFound;
extern const ek_typeinfo ekT_cap_IoError_Permission;
extern const ek_typeinfo ekT_cap_IoError_Exists;
extern const ek_typeinfo ekT_cap_IoError_NotFile;
extern const ek_typeinfo ekT_cap_IoError_Closed;
extern const ek_typeinfo ekT_cap_IoError_InvalidData;
extern const ek_typeinfo ekT_cap_IoError_Other;
extern const ek_typeinfo ekT_cap_OpenMode_Read;
extern const ek_typeinfo ekT_cap_OpenMode_Write;
extern const ek_typeinfo ekT_cap_OpenMode_Append;
extern const ek_typeinfo ekT_cap_OpenMode_New;
extern const ek_typeinfo ekT_cap_OpenMode_Update;
extern const ek_typeinfo ekT_cap_FileInfo;
extern const ek_typeinfo ekT_time_Time_Time;
extern const ek_typeinfo ekT_time_Mono_Mono;

/* Value builders over the shared identities above (capval.c). Each
   takes ownership of the references it stores: a payload argument was
   owned by the caller and is owned by the built value afterwards. */

/* The std Result constructors: Ok and Err over a one-slot payload. */
ek_val ek_gen_result_ok(ek_val payload);
ek_val ek_gen_result_err(ek_val payload);

/* The std Option constructors. */
ek_val ek_gen_option_some(ek_val payload);
ek_val ek_gen_option_none(void);

/* The IoError of one kind; the two payload kinds carry a fresh detail
   String over the message, the others read no message. */
ek_val ek_gen_ioerr(int kind, const char *msg);

/* The time companion's wall-clock point and monotonic reading, from
   their nanosecond payloads. */
ek_val ek_gen_time(int64_t ns);
ek_val ek_gen_mono(int64_t ns);

/* The FileInfo record of the capability module: size in bytes, the
   modification time as a Time value, and the directory flag. */
ek_val ek_gen_file_info(uint64_t size, ek_val mtime, int is_dir);

/* The nanosecond payload of a time companion value: the single
   constructor slot behind the header. A value that is not a reference
   reads as zero. */
int64_t ek_gen_ns(ek_val v);

/* The plumbing of the file-family closures. ek_gen_cstr reads a
   String argument's bytes; ek_gen_io_kind maps a errno onto the
   closed reason union; ek_gen_fail writes the failure and returns 0.
   ek_gen_ok and ek_gen_err wrap an owned payload or a reason -- the
   message becomes the fresh detail String, and an object detail dies
   with the call. */
const char *ek_gen_cstr(ek_val v);
int ek_gen_io_kind(int e);
int ek_gen_fail(int *err, ek_obj **detail, int kind, const char *msg);
ek_val ek_gen_ok(ek_val payload);
ek_val ek_gen_err(int kind, const char *msg);
ek_val ek_gen_err_of(int kind, ek_obj *detail);

/* Opens the path in one of the OpenMode variant order's modes over
   the handle table: 1 with the File value in *file, or 0 with the
   failure written to *err and *detail like the ek_file_ entries. */
int ek_file_open_cstr(const char *path, int mode, ek_val *file, int *err,
                      ek_obj **detail);

/* The directory family's bundle closures (fsdir.c): the whole-path
   operations on directories, each returned as a fresh closure over
   the unit. */
ek_val ek_fs_list_dir(void);
ek_val ek_fs_mkdir(void);
ek_val ek_fs_mkdir_all(void);
ek_val ek_fs_rm(void);
ek_val ek_fs_rm_dir(void);
ek_val ek_fs_rename(void);
ek_val ek_fs_stat(void);
ek_val ek_fs_realpath(void);

/* The bundle records: header plus one ek_val per member, in the
   compiler's field order. One shared family per arity, runtime-typed;
   the drop releases every member closure with the record. */
ek_val ek_bundle1_new(ek_val a);
ek_val ek_bundle3_new(ek_val a, ek_val b, ek_val c);
ek_val ek_bundle16_new(const ek_val *fields);

/* Reads one line from the stream, without its terminating newline: a
   fresh String object, or NULL at end of input with *eof set, or NULL
   with *bad set when the bytes are not well-formed UTF-8. */
ek_obj *ek_line_read(FILE *fp, int *eof, int *bad);

/* Static typeinfos of the runtime-owned families. A family's drop
   lands with its implementation; NULL means the family holds no child
   references. */
extern const ek_typeinfo ek_typeinfo_str;
extern const ek_typeinfo ek_typeinfo_bytes;
extern const ek_typeinfo ek_typeinfo_cons;
extern const ek_typeinfo ek_typeinfo_dict;
extern const ek_typeinfo ek_typeinfo_closure;
extern const ek_typeinfo ek_typeinfo_nil;

/* Drops of the families that hold child references. The release path
   calls them at the last reference; each releases its children with
   constant C stack, so long chains and wide tables unwind without
   recursion. */
void ek_cons_drop(ek_obj *obj);
void ek_dict_drop(ek_obj *obj);
void ek_closure_drop(ek_obj *obj);

/* Steps one count down and returns the count after the step. A zero
   result hands the reclamation to the caller, which releases the
   children and frees the storage; the list spine release uses it to
   consume a chain of cells in constant stack. Ordinary releases go
   through ek_release. */
unsigned long ek_rc_dec(ek_obj *obj);

/* The length of the sequence starting at p, or 0 when the bytes do
   not form one code point within left bytes: overlong forms,
   surrogate encodings, code points above the maximum, and truncated
   tails are invalid. Shared by the string and bytes decoders. */
size_t ek_utf8_seq_len(const unsigned char *p, size_t left);

/* Writes the UTF-8 encoding of code into out and returns its length.
   The code must be a scalar value; the caller checks the domain. */
size_t ek_utf8_encode(uint32_t code, char *out);

/* Zeroed storage of the given size, or an abort: memory exhaustion
   ends the process, it is not a panic. */
static inline void *ek_rt_alloc_zeroed(size_t size)
{
    void *p = calloc(1, size);

    if (p == NULL)
        ek_abort("out of memory");
    return p;
}

/* The immortal empty-list object: static storage, born with one count,
   never dropped and never freed. */
ek_obj *ek_nil_obj(void);

#endif
