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

/* Bool travels as an Int of 0 or 1: the constructor normalizes any
   nonzero b to 1. */
static inline ek_val ek_bool(int b)
{
    return ek_val_int(b != 0);
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

/* Arithmetic and comparison over the domain words. Static typing
   guarantees both operands of an entry carry the same tag, and each
   entry dispatches on that tag. Arithmetic yields the matching tag,
   the comparisons yield Bool. */

/* The arithmetic entries add, subtract, multiply, divide, and take the
   remainder of a and b. The integer paths truncate toward zero and
   check the result: division or modulo by zero panics with "P-div",
   integer overflow with "P-overflow". The Float path is plain IEEE
   arithmetic with no checks, and % is C fmod. */
ek_val ek_num_add(ek_val a, ek_val b);
ek_val ek_num_sub(ek_val a, ek_val b);
ek_val ek_num_mul(ek_val a, ek_val b);
ek_val ek_num_div(ek_val a, ek_val b);
ek_val ek_num_mod(ek_val a, ek_val b);

/* Negation over the matching tag; negating the Int minimum overflows
   and panics like the arithmetic above. */
ek_val ek_num_neg(ek_val a);

/* The comparisons compare a and b in the tag's own order — Int
   signed, UInt unsigned, Float the IEEE ordering, Char by code
   point — and return Bool. NaN is unordered: every ordered
   comparison over it is false and inequality is true. */
ek_val ek_num_eq(ek_val a, ek_val b);
ek_val ek_num_ne(ek_val a, ek_val b);
ek_val ek_num_lt(ek_val a, ek_val b);
ek_val ek_num_le(ek_val a, ek_val b);
ek_val ek_num_gt(ek_val a, ek_val b);
ek_val ek_num_ge(ek_val a, ek_val b);

/* The kinds the parse family reports: a value was read, the input
   was missing, the input was malformed, or the value lies outside
   the target's limits. The compiler wraps the kind into the parse
   result; the runtime never builds that result type. */
typedef enum {
    EK_PARSE_OK,
    EK_PARSE_EMPTY,
    EK_PARSE_INVALID,
    EK_PARSE_RANGE
} ek_parse_status;

/* The display channel: each entry renders one scalar as a fresh
   String object in the form the standard pins. Int and UInt print the
   full decimal value with a sign only when negative; Bool prints
   True or False from the 0/1 Int it travels as; Char prints the
   character bare as its UTF-8 encoding; Float prints the shortest
   decimal form that reads back as the same double, always carrying a
   fractional point in the fixed positions, and prints NaN, Inf, and
   -Inf for the non-finite values. Passing a value of another tag is
   undefined. The caller owns the returned object. */
ek_obj *ek_show_int(ek_val v);
ek_obj *ek_show_uint(ek_val v);
ek_obj *ek_show_float(ek_val v);
ek_obj *ek_show_char(ek_val v);
ek_obj *ek_show_bool(ek_val v);

/* The parse family reads one scalar out of a String object. Each
   entry validates the whole input and reports one of four kinds:
   EK_PARSE_OK writes the value to *out; EK_PARSE_EMPTY means the
   input was the empty string; EK_PARSE_INVALID means malformed
   input; EK_PARSE_RANGE means well-formed input whose value is
   outside the target's limits. *out is written only on
   EK_PARSE_OK. */

/* Well-formed text is an optional sign followed by one or more
   digits of the given radix (2, 8, or 16, case-insensitive, no
   prefix). Int.parse and UInt.parse are radix 10. A minus sign on
   the UInt entries is a range failure, not a malformed one. */
ek_parse_status ek_parse_int(ek_obj *s, int64_t *out);
ek_parse_status ek_parse_uint(ek_obj *s, uint64_t *out);
ek_parse_status ek_parse_int_radix(ek_obj *s, int radix, int64_t *out);
ek_parse_status ek_parse_uint_radix(ek_obj *s, int radix, uint64_t *out);

/* Well-formed text is an optional sign, then either the spelling
   NaN, Inf, -Inf, or +Inf, or one or more digits with an optional
   fraction (a point followed by at least one digit) and an optional
   e-notation exponent. A leading digit is required. Overflow and
   underflow take the IEEE result they read as. */
ek_parse_status ek_parse_float(ek_obj *s, double *out);

/* The conversion family moves one domain word into another slot. The
   ek_try_ entries are the narrowing rows: they return nonzero and
   write *out when the value lies in the target's limits, and return
   zero leaving *out untouched otherwise. Float truncates toward
   zero, checks NaN, the infinities, and the range, and for unsigned
   targets checks the sign before truncation, so -0.5 fails and -0.0
   yields zero. A tag outside the Int/UInt/Float slots is undefined. */

/* The domain rows: UInt.to_int and Float.to_int share the first,
   Int.to_uint and Float.to_uint the second. An Int source is its own
   identity. */
int ek_try_to_int(ek_val v, int64_t *out);
int ek_try_to_uint(ek_val v, uint64_t *out);

/* The machine-word rows: the value must fit the named width, signed
   or unsigned. The result travels in the Int or UInt slot. */
int ek_try_to_i8(ek_val v, int64_t *out);
int ek_try_to_i16(ek_val v, int64_t *out);
int ek_try_to_i32(ek_val v, int64_t *out);
int ek_try_to_i64(ek_val v, int64_t *out);
int ek_try_to_u8(ek_val v, uint64_t *out);
int ek_try_to_u16(ek_val v, uint64_t *out);
int ek_try_to_u32(ek_val v, uint64_t *out);
int ek_try_to_u64(ek_val v, uint64_t *out);

/* The total rows: to_float is Int.to_float and UInt.to_float, to_f32
   rounds through the binary32 width, to_f64 is exact. The result
   travels in the Float slot; a value of another tag yields the
   neutral unit value. */
ek_val ek_to_float(ek_val v);
ek_val ek_to_f32(ek_val v);
ek_val ek_to_f64(ek_val v);

/* Char's code-point bridge. The code is the Unicode scalar value; a
   Char is one code point. */

/* The code point as an Int. Total. */
ek_val ek_char_to_code(ek_val c);

/* The Char whose code point is the given Int: nonzero and the code
   in *out when it lies in the scalar-value domain (below the
   encoding maximum, outside the surrogate range), zero with *out
   untouched otherwise. */
int ek_try_char_from_code(ek_val n, uint32_t *out);

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

/* The string's scalar operations. A String is a sequence of Unicode
   scalar values over the UTF-8 bytes; the length counts scalars, the
   access is by scalar index, and the bridge members move between the
   string and a list of Char values. The entries take the object of a
   string value; passing an object of another family is undefined. */

/* The number of Unicode scalar values, not bytes. */
size_t ek_str_length(ek_obj *str);

/* The code point at the given scalar index: nonzero and the code in
   *code when the index lies in 0..length-1, zero with *code
   untouched otherwise. O(n). */
int ek_str_at(ek_obj *str, int64_t index, uint32_t *code);

/* The scalars as a fresh list of Char values, in order. Aborts on
   exhaustion. */
ek_val ek_str_chars(ek_obj *str);

/* A fresh string holding the single scalar. */
ek_obj *ek_str_from_char(ek_val c);

/* A fresh string holding the list's Char values in order. Aborts on
   exhaustion. */
ek_obj *ek_str_from_chars(ek_val chars);

/* The lexicographic order over the scalar values: negative when a
   orders before b, zero when equal, positive otherwise. For valid
   UTF-8 this is the byte order. */
int ek_str_cmp(ek_obj *a, ek_obj *b);

/* The ordering family over the lexicographic order. */
int ek_str_lt(ek_obj *a, ek_obj *b);
int ek_str_le(ek_obj *a, ek_obj *b);
int ek_str_gt(ek_obj *a, ek_obj *b);
int ek_str_ge(ek_obj *a, ek_obj *b);

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

/* The bytes operations. The element domain is Int in 0..255; the
   bridges move between the block and lists. */

/* The byte at the given index: nonzero and the element in *out when
   the index lies in 0..len-1, zero with *out untouched otherwise. */
int ek_bytes_get(ek_obj *bytes, int64_t index, int64_t *out);

/* A fresh block holding the bytes of a followed by those of b; both
   operands are untouched. Aborts on exhaustion. */
ek_obj *ek_bytes_concat(ek_obj *a, ek_obj *b);

/* The elements as a fresh list of Int values, in order. Aborts on
   exhaustion. */
ek_val ek_bytes_to_ints(ek_obj *bytes);

/* A fresh block of the list's Int elements, or NULL when an element
   falls outside 0..255. Aborts on exhaustion. */
ek_obj *ek_bytes_from_ints(ek_val ints);

/* The bytes encoded as lowercase hexadecimal with no separators. */
ek_obj *ek_bytes_to_hex(ek_obj *bytes);

/* Decodes hexadecimal text: nonzero and the block in *out when every
   character is hexadecimal and the count is even (the empty text
   yields the empty block); zero with the failure detail in *detail
   otherwise. The caller owns both objects. */
int ek_bytes_from_hex(ek_obj *hex, ek_obj **out, ek_obj **detail);

/* The block as a fresh string, which is always valid UTF-8: nonzero
   and the string in *out when the bytes are well-formed UTF-8; zero
   with the first invalid offset and its surrounding bytes in *detail
   otherwise. The caller owns both objects. */
int ek_bytes_to_string(ek_obj *bytes, ek_obj **out, ek_obj **detail);

/* A fresh block holding the string's UTF-8 bytes. Total. */
ek_obj *ek_str_to_bytes(ek_obj *str);

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

/* Nonzero when v refers to a cons cell. */
static inline int ek_val_is_cons(ek_val v)
{
    return v.tag == EK_VAL_REF && v.as.ref != NULL
        && v.as.ref->info->kind == EK_KIND_CONS;
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

/* A fresh dictionary without key's mapping: every remaining entry
   keeps its insertion order. Removing an absent key yields dict
   unchanged. Aborts on exhaustion. */
ek_val ek_dict_remove(ek_val dict, ek_val key);

/* A fresh dictionary holding every mapping of a and then every
   mapping of b, b winning on duplicate keys: a's keys keep their
   order, and b's new keys append in b's order. Aborts on
   exhaustion. */
ek_val ek_dict_merge(ek_val a, ek_val b);

/* Visits the mappings in insertion order: writes the next key and
   value into *key and *val (borrowed) and returns nonzero while a
   mapping remains. *cursor starts at zero and is owned by the walk;
   the caller passes it back unchanged between calls. */
int ek_dict_next(ek_val dict, size_t *cursor, ek_val *key, ek_val *val);

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

/* The capability channel: a capability value is a bundle record of
   world-touching closures, layout-compatible with the compiler's
   one-field records, so generated code reads the closures through its
   own layout and the release path reclaims the bundle's closures. */

/* The Out bundle over the process's standard output: its write closure
   writes the String argument's bytes to stdout without a terminator
   and returns the unit value. The caller owns the returned bundle. */
ek_val ek_cap_out_stdout(void);

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

/* The compiler-provided panic over a value: a String object is
   reported with code "P-panic" and its bytes as the message; any other
   value reports the same code with the message "panic of a non-string"
   and never reads a payload. Exits with status 1 either way. */
void ek_panic_value(ek_val msg);

/* Writes "abort: reason" to standard error, best effort, and calls
   abort(), so the process ends with the platform abort status. This is
   the path for exhaustion and native faults, not for program-level
   invariants. */
void ek_abort(const char *reason);

#endif
