#ifndef EK_RT_EKORT_H
#define EK_RT_EKORT_H

#include <stddef.h>
#include <stdint.h>

/* A value is a tagged scalar or a reference to a heap object. Bool
   travels as an Int of 0 or 1, machine words widen into the Int and
   UInt slots, F32 widens into the Float slot, and a Char is a Unicode
   code point. The composite families live behind the reference. */
typedef enum {
    EK_VAL_UNIT,
    EK_VAL_INT,
    EK_VAL_UINT,
    EK_VAL_FLOAT,
    EK_VAL_CHAR,
    EK_VAL_REF
} ek_tag;

/* The heap families. Struct objects are compiler-emitted composites:
   the runtime calls their drop but never builds them. Nil marks the
   immortal empty-list singleton. */
typedef enum {
    EK_KIND_STR,
    EK_KIND_BYTES,
    EK_KIND_CONS,
    EK_KIND_DICT,
    EK_KIND_CLOSURE,
    EK_KIND_STRUCT,
    EK_KIND_NIL
} ek_kind;

typedef struct ek_obj ek_obj;

/* Releases an object's child references by calling ek_release on each.
   It never frees the object itself; the release path frees the storage
   after drop returns. NULL means the object holds no child
   references. */
typedef void (*ek_drop_fn)(ek_obj *);

/* Family classification of a heap object. */
typedef struct ek_typeinfo {
    uint8_t kind;
    ek_drop_fn drop;
} ek_typeinfo;

/* Heap object header. Every heap family embeds it as the first member
   of its struct, and a new object is born with rc == 1. */
struct ek_obj {
    const ek_typeinfo *info;
    unsigned long rc;
};

/* A value occupies exactly 16 bytes: the tag word and its payload. */
typedef struct ek_val {
    ek_tag tag;
    union {
        int64_t i;
        uint64_t u;
        double f;
        uint32_t c;
        ek_obj *ref;
    } as;
} ek_val;

/* Each constructor wraps a scalar in a value of the matching tag; the
   payload is stored unchanged. */
static inline ek_val ek_val_unit(void)
{
    ek_val v;
    v.tag = EK_VAL_UNIT;
    v.as.i = 0;
    return v;
}

static inline ek_val ek_val_int(int64_t n)
{
    ek_val v;
    v.tag = EK_VAL_INT;
    v.as.i = n;
    return v;
}

static inline ek_val ek_val_uint(uint64_t n)
{
    ek_val v;
    v.tag = EK_VAL_UINT;
    v.as.u = n;
    return v;
}

static inline ek_val ek_val_float(double x)
{
    ek_val v;
    v.tag = EK_VAL_FLOAT;
    v.as.f = x;
    return v;
}

static inline ek_val ek_val_char(uint32_t code)
{
    ek_val v;
    v.tag = EK_VAL_CHAR;
    v.as.c = code;
    return v;
}

/* Stores the pointer as given and adds no reference: retention is the
   caller's accounting. A NULL pointer is stored, and both ek_retain
   and ek_release treat it as no reference. */
static inline ek_val ek_val_ref(ek_obj *obj)
{
    ek_val v;
    v.tag = EK_VAL_REF;
    v.as.ref = obj;
    return v;
}

/* Nonzero when v carries the matching tag. */
static inline int ek_val_is_unit(ek_val v)
{
    return v.tag == EK_VAL_UNIT;
}

static inline int ek_val_is_int(ek_val v)
{
    return v.tag == EK_VAL_INT;
}

static inline int ek_val_is_uint(ek_val v)
{
    return v.tag == EK_VAL_UINT;
}

static inline int ek_val_is_float(ek_val v)
{
    return v.tag == EK_VAL_FLOAT;
}

static inline int ek_val_is_char(ek_val v)
{
    return v.tag == EK_VAL_CHAR;
}

static inline int ek_val_is_ref(ek_val v)
{
    return v.tag == EK_VAL_REF;
}

/* A string is an immutable UTF-8 byte sequence on the heap. The
   accessors take the object of a string value; passing an object of
   another family is undefined. */

/* Copies len bytes into a fresh string and stores a NUL terminator
   behind them, so the C-string view needs no copy. The bytes are not
   validated: compiler literals arrive pre-validated. Aborts on
   exhaustion. */
ek_obj *ek_str_new(const char *utf8, size_t len);

/* ek_str_new over the bytes of the NUL-terminated s. */
ek_obj *ek_str_from_cstr(const char *s);

/* The byte length, excluding the stored terminator. */
size_t ek_str_len(ek_obj *str);

/* The stored bytes, NUL-terminated; the storage is shared with the
   object and lives as long as its last reference. */
const char *ek_str_cstr(ek_obj *str);

/* A fresh string with the bytes of a followed by those of b; both
   operands are untouched. Aborts on exhaustion. */
ek_obj *ek_str_concat(ek_obj *a, ek_obj *b);

/* Nonzero when the two strings have the same length and the same
   bytes. */
int ek_str_eq(ek_obj *a, ek_obj *b);

/* Nonzero when s holds exactly len bytes of well-formed UTF-8: no
   overlong forms, no surrogate encodings, and no code point above the
   encoding maximum. */
int ek_utf8_valid(const char *s, size_t len);

/* A bytes value is an immutable byte sequence on the heap; there is no
   mutation operation. The accessors take the object of a bytes value. */

/* Copies len bytes into a fresh value. Aborts on exhaustion. */
ek_obj *ek_bytes_new(const void *data, size_t len);

/* The byte length. */
size_t ek_bytes_len(ek_obj *bytes);

/* The stored bytes; the storage is shared with the object and lives as
   long as its last reference. */
const void *ek_bytes_data(ek_obj *bytes);

/* A list is a chain of immutable cons cells ending at the empty list.
   A cell owns the references it stores: they were retained at
   insertion and are released when the cell is reclaimed. */

/* The empty list: a reference to the immortal singleton, whose count
   may reach zero without ending it. */
ek_val ek_nil_val(void);

/* Nonzero when v is the empty-list value. */
static inline int ek_val_is_nil(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref != NULL
        && v.as.ref->info->kind == EK_KIND_NIL;
}

/* A fresh cell holding head and tail. Both are retained; the caller's
   own references are untouched. Aborts on exhaustion. */
ek_val ek_cons(ek_val head, ek_val tail);

/* The stored head or tail, with no reference added: the value lives as
   long as the cell does. */
ek_val ek_cons_head(ek_obj *cell);
ek_val ek_cons_tail(ek_obj *cell);

/* A fresh spine holding the elements of a in order, with b as the tail
   of its last cell: the spine retains each head and b, while a keeps
   its own cells. Appending to the empty list yields b with one added
   reference. Aborts on exhaustion. */
ek_val ek_list_append(ek_val a, ek_val b);

/* The number of cells; the empty list has none. */
size_t ek_list_len(ek_val list);

/* A dictionary maps inline scalar keys to values. Setting a key builds
   a fresh dictionary; the old one is untouched and keeps its own
   references. Keys compare on the tag and the exact payload bytes. The
   accessors take a dictionary value. */

/* The empty dictionary. Aborts on exhaustion. */
ek_val ek_dict_new(void);

/* Writes the stored value into *out and returns nonzero when key is
   present; the value is borrowed. */
int ek_dict_get(ek_val dict, ek_val key, ek_val *out);

/* A fresh dictionary in which key maps to val: every reference it
   stores is retained, an existing mapping for key is replaced, and
   dict keeps its own references. Aborts on exhaustion. */
ek_val ek_dict_set(ek_val dict, ek_val key, ek_val val);

/* The number of keys. */
size_t ek_dict_len(ek_val dict);

/* The uniform call ABI: the closure receives the environment it
   captured and the argument array of argc values, or NULL when argc is
   zero. The callee retains whatever it stores. */
typedef ek_val (*ek_closure_fn)(ek_val env, ek_val *args, size_t argc);

/* A fresh closure holding call and env; the environment is retained.
   Aborts on exhaustion. */
ek_val ek_closure_new(ek_closure_fn call, ek_val env);

/* Calls the closure with the arguments. A value that is not a closure
   panics. */
ek_val ek_closure_call(ek_val closure, ek_val *args, size_t argc);

/* The captured environment, with no reference added. */
ek_val ek_closure_env(ek_obj *closure);

/* Adds one reference when v refers to a heap object; any other tag is
   left unchanged. */
void ek_retain(ek_val v);

/* Drops one reference when v refers to a heap object; any other tag is
   left unchanged. At the last release the object's drop runs and its
   storage is freed. The empty-list singleton is immortal: its count
   may reach zero, but it is never dropped and never freed. */
void ek_release(ek_val v);

/* Writes the single line "panic [code]: message" to standard error and
   exits with status 1. No release pass runs and nothing else is
   written. code and message are null-terminated strings. */
void ek_panic(const char *code, const char *msg);

/* Writes "abort: reason" to standard error, best effort, and calls
   abort(), so the process ends with the platform abort status. This is
   the path for exhaustion and native faults, not for program-level
   invariants. */
void ek_abort(const char *reason);

#endif
