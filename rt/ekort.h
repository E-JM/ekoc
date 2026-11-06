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
    EK_KIND_SET,
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

/* Family classification of a heap object. The eq and hash members are
   the structural keying hooks: eq answers nonzero when a and b carry
   the same key value, and hash mixes v into a word the dictionary and
   set tables spread over their slots. Both are deterministic within
   one process run, and values eq calls equal hash equal. NULL means
   the family carries no structural hook and keys on identity. The
   hooks are consulted only over values of one family: a key of
   another family compares unequal before any hook runs. */
typedef struct ek_typeinfo {
    uint8_t kind;
    ek_drop_fn drop;
    int (*eq)(ek_val a, ek_val b);
    unsigned long (*hash)(ek_val v);
} ek_typeinfo;

/* The key hooks a dictionary or set carries: the pair answers the
   table's comparisons and mixes over the key domain the container was
   built for. They have the shape of the typeinfo's eq and hash, so a
   generated service fills either slot. Both are deterministic within
   one process run, and values the eq hook calls equal the hash hook
   hashes equal. */
typedef int (*ek_key_eq_fn)(ek_val a, ek_val b);
typedef unsigned long (*ek_key_hash_fn)(ek_val v);

/* The default key pair, over the domains the runtime itself can key:
   String and Bytes on their bytes, scalars on the tag and the payload,
   Float on IEEE equality with the two zeros one key and NaN matching
   nothing, and a reference through the hooks its family carries, the
   object's address the fallback of a family with none. The pair a
   container passes whose keys need no derived service. */
int ek_key_eq(ek_val a, ek_val b);
unsigned long ek_key_hash(ek_val v);

/* Heap object header. Every heap family embeds it as the first member
   of its struct, and a new object is born with rc == 1. */
struct ek_obj {
    const ek_typeinfo *info;
    unsigned long rc;
};

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

/* The floor family over the same tags: the quotient rounds toward
   negative infinity and the Int remainder takes the divisor's sign,
   with the same zero and overflow checks as the truncating pair. On
   UInt the floor is the truncating division, and on Float the IEEE
   division and remainder. */
ek_val ek_num_fdiv(ek_val a, ek_val b);
ek_val ek_num_fmod(ek_val a, ek_val b);

/* Negation over the matching tag; negating the Int minimum overflows
   and panics like the arithmetic above. */
ek_val ek_num_neg(ek_val a);

/* The bitwise family over UInt and the wrapping forms over Int and
   UInt: arithmetic on the integers modulo 2^64 with no panic, and a
   shift count of 64 or more yielding zero. */
ek_val ek_uint_band(ek_val a, ek_val b);
ek_val ek_uint_bor(ek_val a, ek_val b);
ek_val ek_uint_bxor(ek_val a, ek_val b);
ek_val ek_uint_bnot(ek_val a);
ek_val ek_uint_shl(ek_val a, ek_val b);
ek_val ek_uint_shr(ek_val a, ek_val b);
ek_val ek_int_wrap_add(ek_val a, ek_val b);
ek_val ek_int_wrap_sub(ek_val a, ek_val b);
ek_val ek_int_wrap_mul(ek_val a, ek_val b);
ek_val ek_int_wrap_neg(ek_val a);
ek_val ek_uint_wrap_add(ek_val a, ek_val b);
ek_val ek_uint_wrap_sub(ek_val a, ek_val b);
ek_val ek_uint_wrap_mul(ek_val a, ek_val b);
ek_val ek_uint_wrap_neg(ek_val a);

/* The absolute value; the Int minimum has none and panics with the
   overflow code, the unsigned word is its own, and the Float value
   drops its sign. */
ek_val ek_num_abs(ek_val a);

/* The integer power: the Int base to the UInt exponent by repeated
   squaring, one at exponent zero, and the overflow code panics on a
   product outside the domain. A value of another tag reads as the
   neutral unit value. */
ek_val ek_int_pow(ek_val a, ek_val b);

/* The floating-point mathematics surface: each entry reads the Float
   slot and answers the Float slot, C99's libm under the names the
   standard pins, and the three predicates answer Bool. A value of
   another tag reads as the neutral unit value; the predicates read as
   false. */
ek_val ek_num_floor(ek_val a);
ek_val ek_num_ceil(ek_val a);
ek_val ek_num_trunc(ek_val a);
ek_val ek_num_round(ek_val a);
ek_val ek_num_sqrt(ek_val a);
ek_val ek_num_exp(ek_val a);
ek_val ek_num_log(ek_val a);
ek_val ek_num_log2(ek_val a);
ek_val ek_num_log10(ek_val a);
ek_val ek_num_sin(ek_val a);
ek_val ek_num_cos(ek_val a);
ek_val ek_num_tan(ek_val a);
ek_val ek_num_asin(ek_val a);
ek_val ek_num_acos(ek_val a);
ek_val ek_num_atan(ek_val a);
ek_val ek_num_pow(ek_val a, ek_val b);
ek_val ek_num_atan2(ek_val a, ek_val b);
int ek_num_is_nan(ek_val a);
int ek_num_is_inf(ek_val a);
int ek_num_is_finite(ek_val a);

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

/* The radix channel over the integer slots: the words the radix
   parsers read back. The digits carry the unsigned magnitude of the
   64-bit slot word, so an Int's negative values render in
   two's-complement form and a UInt renders its own word. The form is
   lowercase and shortest, with no leading zeros and the plain zero as
   one zero digit. The radix is 2, 8, or 16, the width below is a UInt
   value, and a value of another tag is undefined. The caller owns each
   returned object. */

/* The radix form: hex for 16, octal for 8, binary for 2. */
ek_obj *ek_show_radix(ek_val v, int radix);

/* Fixed-width hexadecimal: the radix-16 form zero-padded on the left
   to at least width digits and never truncated. */
ek_obj *ek_show_hex_pad(ek_val v, ek_val width);

/* The decimal text channel of the float slot: fixed point carries
   exactly that many fractional digits, and scientific notation one
   digit before the point with count minus one after it. Both sign
   only negative values, render NaN, Inf, and -Inf as themselves, and
   stop at the exact form's reach when the count asks past it. The
   count travels as a UInt value; a value of another tag is undefined.
   The caller owns each returned object. */
ek_obj *ek_show_fixed(ek_val v, ek_val digits);
ek_obj *ek_show_exp(ek_val v, ek_val digits);

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

/* The Json document reader: the whole text as one value, wrapped in
   the Result the parse family returns. Ok carries the Json ADT value
   the text builds -- the seven variants, with an object preserving the
   text's key order -- and Err carries the ParseError: Empty when the
   text holds no value at all, Invalid over a detail naming the byte
   offset and context of the first malformed piece, and OutOfRange when
   integer syntax leaves the Int domain. A number without a fraction or
   an exponent reads as Int, anything else as Float, and the scanner
   accepts the spellings NaN, Inf, and -Inf that the Float display
   channel produces. Nesting past 512 container levels panics with
   P-panic rather than exhausting the stack. The returned Result is the
   caller's to release. */
ek_val ek_json_parse(ek_obj *text);

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

/* Char classification. The letter and digit predicates classify the
   ASCII range; is_space is the Unicode White_Space property; the XID
   pair is the Unicode property the lexer's identifier rule builds on
   (the lexer admits '_' as a first character on top of it). A value
   that is not a Char classifies as false. */
int ek_char_is_alpha(ek_val c);
int ek_char_is_digit(ek_val c);
int ek_char_is_alnum(ek_val c);
int ek_char_is_space(ek_val c);
int ek_char_is_upper(ek_val c);
int ek_char_is_lower(ek_val c);
int ek_char_is_xid_start(ek_val c);
int ek_char_is_xid_continue(ek_val c);

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

/* A fresh string holding the quote character, the bytes of s with the
   quote, the backslash, and the C0 controls escaped (the named pair as
   \n \t \r, every other control as \uXXXX), and the quote character
   again: the literal form the structural channel shows a String in.
   The quote is a Unicode scalar value. The caller owns the returned
   object. */
ek_obj *ek_str_quote(ek_obj *s, uint32_t quote);

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
   the index lies in 0..len-1, zero with *out untouched otherwise.
   The index travels as an Int value. */
int ek_bytes_get(ek_obj *bytes, ek_val index, int64_t *out);

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

/* Zeroed storage for a boundary copy-in array of n elements of w bytes;
   exhaustion aborts. The caller owns the array for the call's duration. */
void *ek_cin_array(size_t n, size_t w);

/* A dictionary maps keys to values over the key hooks it is born
   with. Setting a key builds a fresh dictionary; the old one is
   untouched and keeps its own references. The pair of hooks travels
   with the dictionary: every derived construction clones the source's
   pair, so one key domain keys the same way across a whole build
   chain. The fast domains -- scalars, Float, and the text families --
   are keyed by the runtime itself; a reference key outside them is
   answered by the carried eq and hash hooks, so two structurally equal
   composite values are one key. Passing NULL for both builds a table
   that answers a reference key only when the key's own family carries
   hooks; an unanswerable key aborts the process. The accessors take a
   dictionary value. */

/* The empty dictionary over the given key hooks. Aborts on
   exhaustion. */
ek_val ek_dict_new(ek_key_eq_fn keq, ek_key_hash_fn kh);

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
   keeps its insertion order. Removing an absent key yields the
   original, and the returned dictionary is the caller's to release
   either way -- the fresh one, or a reference to the original added.
   Aborts on exhaustion. */
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

/* The traversals run as native loops over the insertion order, so
   their stack usage is independent of the entry count. The entries are
   borrowed, and each callback retains whatever it stores. */

/* The left-to-right fold in insertion order: the accumulator starts at
   acc, each step calls f with the accumulator, the key, and the value,
   and the last step's result is returned. acc is borrowed -- the
   caller keeps its own reference -- and the returned accumulator is
   the caller's to release. */
ek_val ek_dict_fold(ek_val dict, ek_val acc, ek_val f);

/* The callback over every mapping in insertion order, with the key
   first and the value second; the results are dropped and the unit
   value returned. */
ek_val ek_dict_each(ek_val dict, ek_val f);

/* A fresh dictionary whose values are the callback's results, the keys
   and their insertion order unchanged. */
ek_val ek_dict_map(ek_val dict, ek_val f);

/* A fresh dictionary holding the mappings whose callback says keep, in
   insertion order. */
ek_val ek_dict_filter(ek_val dict, ek_val f);

/* A set is an immutable collection of distinct elements with
   copy-on-insert, carried over the same table discipline as the
   dictionary: insertion order is kept in a build-order chain and
   survives every rehash. Elements key over the hook pair the set is
   born with, cloned by every derived construction the same way the
   dictionary's is. The fast domains -- scalars, Float, String, and
   Bytes -- are keyed by the runtime itself; a reference element
   outside them is answered by the carried hooks, so 0.0 and -0.0 are
   one element while a NaN element never matches, not even itself, and
   two structurally equal composite elements are one. Passing NULL for
   both leaves the answer of an unhooked reference element to the
   element's own family, and an unanswerable element aborts the
   process. The accessors take a set value. */

/* The empty set over the given element hooks. Aborts on exhaustion. */
ek_val ek_set_new(ek_key_eq_fn keq, ek_key_hash_fn kh);

/* A fresh set over the list's elements in order, deduplicated with the
   first occurrence kept, keyed by the given element hooks. Aborts on
   exhaustion. */
ek_val ek_set_of(ek_val list, ek_key_eq_fn keq, ek_key_hash_fn kh);

/* A fresh set with the element added: an element already present
   yields the original, and the returned set is the caller's to release
   either way -- the fresh one, or a reference to the original added.
   Aborts on exhaustion. */
ek_val ek_set_insert(ek_val set, ek_val elem);

/* A fresh set without the element: the remaining elements keep their
   insertion order, and an absent element yields the original, with the
   same release rule as insert. Aborts on exhaustion. */
ek_val ek_set_remove(ek_val set, ek_val elem);

/* Nonzero when the element is present. */
int ek_set_contains(ek_val set, ek_val elem);

/* The number of elements. */
size_t ek_set_len(ek_val set);

/* The elements as a fresh list, in insertion order. Aborts on
   exhaustion. */
ek_val ek_set_to_list(ek_val set);

/* The union keeps the left operand's order first and appends the
   right operand's new elements in right order; the intersection and
   the difference keep the left operand's order. All three carry the
   left operand's element hooks and abort on exhaustion. */
ek_val ek_set_union(ek_val a, ek_val b);
ek_val ek_set_intersect(ek_val a, ek_val b);
ek_val ek_set_diff(ek_val a, ek_val b);

/* A range is a compiler-emitted struct over one ordered scalar: the
   object header is followed by three ek_val fields in this order --
   start, end, closed -- and the closed field carries Bool, an Int of
   0 or 1. That layout is the contract between the generated struct
   families and the entries below; the entries take the range value
   and read it through the contract. Endpoints travel in the Int,
   UInt, or Char slot, and the traversals visit in ascending order,
   empty when the start exceeds the end. Each traversal runs as a
   native loop, so its stack usage is independent of the element
   count, and the callback's world bit does not reach the runtime. */

/* The count of the elements: the span plus the closed flag. Empty
   when the start exceeds the end; a span outside the Int domain
   panics like any other arithmetic overflow. */
ek_val ek_range_length(ek_val range);

/* The elements as a fresh list, ascending. Aborts on exhaustion. */
ek_val ek_range_to_list(ek_val range);

/* The callback's results as a fresh list, ascending; the callback is
   called once per element in order. Aborts on exhaustion. */
ek_val ek_range_map(ek_val range, ek_val f);

/* The callback over every element in ascending order; the results
   are dropped and the unit value returned. */
ek_val ek_range_each(ek_val range, ek_val f);

/* The callback over every element with its position from zero. */
ek_val ek_range_each_index(ek_val range, ek_val f);

/* The left-to-right fold: the accumulator starts at acc, each step
   calls f with the accumulator and the element, and the last step's
   result is returned. acc is borrowed -- the caller keeps its own
   reference -- and the returned accumulator is the caller's to
   release. */
ek_val ek_range_fold(ek_val range, ek_val acc, ek_val f);

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

/* A call through a world-touching closure. A concurrency task checks
   its pending cancellation here, before the call, and ends without
   making it; outside a task the call behaves as ek_closure_call. */
ek_val ek_world_call(ek_val closure, ek_val *args, size_t argc);

/* The captured environment, with no reference added. */
ek_val ek_closure_env(ek_obj *closure);

/* The concurrency surface: a Conc value, a scope over its spawned
   tasks, and the task operations. A scope waits for every child and
   cancels the ones still running; a task reports Some result or None
   when cancelled; cancellation is cooperative at world calls. */
ek_val ek_cap_conc_os(void);
ek_val ek_conc_scope(ek_val conc, ek_val callback);
ek_val ek_scope_spawn(ek_val scope, ek_val callback);
ek_val ek_task_join(ek_val task);
ek_val ek_task_cancel(ek_val task);

/* The capability channel: a capability value is a bundle record of
   world-touching closures, layout-compatible with the compiler's
   one-field records, so generated code reads the closures through its
   own layout and the release path reclaims the bundle's closures. */

/* The slot order of each bundle record: a bundle is the object header
   followed by one ek_val per member, in this order, one enumeration per
   capability. The host fills its slots by these names, the generated
   unit lays its bundle record out to match, and the unit checks the two
   agree at compile time, so neither side can drift silently. */
typedef enum {
    EK_OUT_WRITE,
    EK_OUT_WRITE_LINE,
    EK_OUT_SLOTS
} ek_out_slot;

typedef enum {
    EK_IN_READ_LINE,
    EK_IN_SLOTS
} ek_in_slot;

/* The FileInfo record's fields, in the declaration order of the
   vocabulary module. The runtime builds the record and the emitted unit
   reads it through its own struct, so both sides check their layout
   against these slots. */
typedef enum {
    EK_FILEINFO_SIZE,
    EK_FILEINFO_MTIME,
    EK_FILEINFO_IS_DIR,
    EK_FILEINFO_SLOTS
} ek_fileinfo_slot;

typedef enum {
    EK_FS_OPEN,
    EK_FS_WITH_FILE,
    EK_FS_EXISTS,
    EK_FS_READ,
    EK_FS_WRITE,
    EK_FS_APPEND,
    EK_FS_READ_BYTES,
    EK_FS_APPEND_BYTES,
    EK_FS_LIST_DIR,
    EK_FS_MKDIR,
    EK_FS_MKDIR_ALL,
    EK_FS_RM,
    EK_FS_RM_DIR,
    EK_FS_RENAME,
    EK_FS_STAT,
    EK_FS_REALPATH,
    EK_FS_SLOTS
} ek_fs_slot;

typedef enum {
    EK_ENV_GET,
    EK_ENV_SLOTS
} ek_env_slot;

typedef enum {
    EK_ARGS_ALL,
    EK_ARGS_SLOTS
} ek_args_slot;

typedef enum {
    EK_CLOCK_NOW,
    EK_CLOCK_MONO,
    EK_CLOCK_SLEEP,
    EK_CLOCK_SLOTS
} ek_clock_slot;

typedef enum {
    EK_RAND_INT,
    EK_RAND_SLOTS
} ek_rand_slot;

/* The Out bundle over the process's standard output: its write closure
   writes the String argument's bytes to stdout without a terminator
   and returns the unit value. The caller owns the returned bundle. */
ek_val ek_cap_out_stdout(void);

/* The Out bundle over the process's standard error, the constructor
   behind the compiler's Out.err row: the same bundle shape whose write
   closure writes the String argument's bytes to stderr without a
   terminator and returns the unit value. The caller owns the returned
   bundle. */
ek_val ek_cap_out_stderr(void);

/* The remaining session bundles, over the same shape: one closure per
   member of the compiler's bundle record, in the record's field
   order. Each constructor returns a bundle the caller owns.

   The In bundle reads the process's standard input; read_line yields
   Ok None at end of input and Err InvalidData for bytes that are not
   well-formed UTF-8. The Fs bundle works over the process's working
   directory: every path resolves through it, and no further
   restriction applies. The Env bundle reads the process environment.
   The Args bundle carries the argument vector WITHOUT the program
   name: argc counts the arguments after it and argv points at the
   first one. The Clock bundle reads CLOCK_REALTIME and
   CLOCK_MONOTONIC and sleeps through nanosleep, resuming on an
   interrupt; a non-positive duration returns immediately. The Rand
   bundle seeds a deterministic generator from the operating system's
   entropy. The Proc value is an unforgeable token with no members of
   its own. */
ek_val ek_cap_in_stdin(void);
ek_val ek_cap_fs_cwd(void);
ek_val ek_cap_env_os(void);
ek_val ek_cap_args(int argc, char **argv);
ek_val ek_cap_clock_wall(void);
ek_val ek_cap_rand_os(void);
ek_val ek_cap_proc(void);

/* The failure reasons the file family reports. The caller maps a kind
   onto the IoError variants in this order; the two payload kinds carry
   the detail string. */
typedef enum {
    EK_IO_NOT_FOUND,
    EK_IO_PERMISSION,
    EK_IO_EXISTS,
    EK_IO_NOT_FILE,
    EK_IO_CLOSED,
    EK_IO_INVALID_DATA,
    EK_IO_OTHER
} ek_io_kind;

/* The file family over the runtime's handle table. A File value packs
   its table slot and the slot's generation counter, so a handle copied
   before a close turns stale and every later use reports
   EK_IO_CLOSED.

   The fallible entries return 1 on success and 0 on failure. On
   success an entry that produces a value writes an owned object to
   *out (read_line writes NULL for end of input, which is success). On
   failure *err carries the ek_io_kind and *detail an owned detail
   String for the two payload kinds and NULL for every other; the
   caller that does not store the detail releases it. Passing a value
   that is not a File reports the failure paths rather than reading
   memory. */
int ek_file_read(ek_val f, int64_t nchars, ek_obj **out, int *err,
                 ek_obj **detail);
int ek_file_read_bytes(ek_val f, int64_t n, ek_obj **out, int *err,
                       ek_obj **detail);
int ek_file_read_line(ek_val f, ek_obj **out, int *err, ek_obj **detail);
int ek_file_write(ek_val f, ek_obj *s, int *err, ek_obj **detail);
int ek_file_write_bytes(ek_val f, ek_obj *b, int *err, ek_obj **detail);
int ek_file_seek(ek_val f, ek_val pos, int *err, ek_obj **detail);
int ek_file_flush(ek_val f, int *err, ek_obj **detail);
int ek_file_close(ek_val f, int *err, ek_obj **detail);

/* The advancing derivations of the capability module. ek_in_from_lines
   builds an In bundle over the list of String lines: read_line yields
   them in order and then end of input forever. ek_rand_with_seed
   builds a Rand bundle whose draws follow splitmix64 from the seed;
   the order is fixed by the seed and the sequence is an implementation
   detail. Both callers own the returned bundle. */
ek_val ek_in_from_lines(ek_obj *lines);
ek_val ek_rand_with_seed(ek_val seed);

/* Exits the process with the given code, which passes through
   unchanged; the normal exit is 0 and a panic exits 1. Never
   returns. */
void ek_proc_exit(ek_val proc, ek_val code);

/* The lexical path vocabulary over String objects, with no
   file-system contact. join inserts a separator between the parts,
   an empty part contributing nothing; normalize drops `.` segments,
   pops `..`, and keeps a leading separator, an empty result reading
   as "."; the absolute test reports a leading separator. Each entry
   returns a fresh object or value the caller owns. */
ek_obj *ek_path_join(ek_obj *a, ek_obj *b);
ek_obj *ek_path_normalize(ek_obj *path);
ek_val ek_path_is_absolute(ek_obj *path);

/* The C namespace's opaque pointer: an object wrapping one void *
   that crosses the native boundary unchanged in either direction.
   There is no dereference and no arithmetic; ek_ptr_wrap builds the
   object over the pointer and ek_ptr_unwrap reads it back. The
   wrapper owns no storage and holds no child references. ek_ptr_wrap
   returns a value the caller owns; ek_ptr_unwrap on a value of
   another family is undefined. */
ek_val ek_ptr_wrap(void *p);
void *ek_ptr_unwrap(ek_val v);

/* The C thread-local errno, which is program-internal state and
   therefore pure. */
int ek_get_errno(void);

/* Adds one reference when v refers to a heap object; any other tag is
   left unchanged. */
void ek_retain(ek_val v);

/* Drops one reference when v refers to a heap object; any other tag is
   left unchanged. At the last release the object's drop runs and its
   storage is freed. The empty-list singleton is immortal: its count
   may reach zero, but it is never dropped and never freed. */
void ek_release(ek_val v);

/* The frame stack behind the panic backtrace: the generated code of a
   named function pushes its source-level name and declaration position
   on entry and pops it at the return. Beyond the bound the newest
   frames are dropped and the depth keeps counting. */
#define EK_FRAME_MAX 256
typedef struct {
    const char *name;
    const char *file;
    int line;
    int col;
} ek_frame;
extern ek_frame ek_frames[EK_FRAME_MAX];
extern size_t ek_frame_depth;
void ek_frame_push(const char *name, const char *file, int line, int col);
void ek_frame_pop(void);

/* Writes the panic line and the frame backtrace to standard error and
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
