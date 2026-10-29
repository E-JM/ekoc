#ifndef EK_RT_EKORT_H
#define EK_RT_EKORT_H

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
