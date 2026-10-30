#include "ekort.h"

#include <math.h>
#include <stdint.h>

/* Checked arithmetic and comparisons over the domain words. Static
   typing guarantees both operands of an entry carry the same tag, so
   each entry dispatches once and reads both payloads in that width.
   The integer paths check division by zero and overflow; the Float
   path is plain IEEE arithmetic with no checks. Only the system cc
   builds this file, so the compiler's checked-arithmetic builtins are
   always available. */

static void die_div(void)
{
    ek_panic("P-div", "division by zero");
}

static void die_overflow(void)
{
    ek_panic("P-overflow", "arithmetic overflow");
}

/* Static typing leaves no other tag in a word entry; the neutral unit
   value keeps the dispatch total without reading a payload. */
static ek_val no_word(void)
{
    return ek_val_unit();
}

/* Int: signed 64-bit, truncating division, checked. */

static ek_val int_add(int64_t a, int64_t b)
{
    int64_t r;

    if (__builtin_add_overflow(a, b, &r))
        die_overflow();
    return ek_val_int(r);
}

static ek_val int_sub(int64_t a, int64_t b)
{
    int64_t r;

    if (__builtin_sub_overflow(a, b, &r))
        die_overflow();
    return ek_val_int(r);
}

static ek_val int_mul(int64_t a, int64_t b)
{
    int64_t r;

    if (__builtin_mul_overflow(a, b, &r))
        die_overflow();
    return ek_val_int(r);
}

/* The pair with divisor -1 and the minimum dividend has no
   representable quotient, so C leaves both / and % undefined there. */
static ek_val int_div(int64_t a, int64_t b)
{
    if (b == 0)
        die_div();
    if (a == INT64_MIN && b == -1)
        die_overflow();
    return ek_val_int(a / b);
}

static ek_val int_mod(int64_t a, int64_t b)
{
    if (b == 0)
        die_div();
    if (a == INT64_MIN && b == -1)
        die_overflow();
    return ek_val_int(a % b);
}

/* Zero minus a, checked: only the minimum overflows. */
static ek_val int_neg(int64_t a)
{
    return int_sub(0, a);
}

/* UInt: unsigned 64-bit, truncating division, checked. */

static ek_val uint_add(uint64_t a, uint64_t b)
{
    uint64_t r;

    if (__builtin_add_overflow(a, b, &r))
        die_overflow();
    return ek_val_uint(r);
}

static ek_val uint_sub(uint64_t a, uint64_t b)
{
    uint64_t r;

    if (__builtin_sub_overflow(a, b, &r))
        die_overflow();
    return ek_val_uint(r);
}

static ek_val uint_mul(uint64_t a, uint64_t b)
{
    uint64_t r;

    if (__builtin_mul_overflow(a, b, &r))
        die_overflow();
    return ek_val_uint(r);
}

static ek_val uint_div(uint64_t a, uint64_t b)
{
    if (b == 0)
        die_div();
    return ek_val_uint(a / b);
}

static ek_val uint_mod(uint64_t a, uint64_t b)
{
    if (b == 0)
        die_div();
    return ek_val_uint(a % b);
}

/* The domain has no UInt negation; the checked zero minus a panics on
   any operand above zero instead of wrapping. */
static ek_val uint_neg(uint64_t a)
{
    return uint_sub(0, a);
}

/* Float: plain IEEE arithmetic, no checks. */

static ek_val float_div(double a, double b)
{
    return ek_val_float(a / b);
}

static ek_val float_mod(double a, double b)
{
    return ek_val_float(fmod(a, b));
}

ek_val ek_num_add(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_add(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_add(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return ek_val_float(a.as.f + b.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_sub(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_sub(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_sub(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return ek_val_float(a.as.f - b.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_mul(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_mul(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_mul(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return ek_val_float(a.as.f * b.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_div(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_div(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_div(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return float_div(a.as.f, b.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_mod(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_mod(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_mod(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return float_mod(a.as.f, b.as.f);
    default:           return no_word();
    }
}

/* The floor family over Int: the quotient rounds toward negative
   infinity and the remainder takes the divisor's sign, so both stay in
   the same checks as the truncating pair. */
static ek_val int_fdiv(int64_t a, int64_t b)
{
    if (b == 0)
        die_div();
    if (a == INT64_MIN && b == -1)
        die_overflow();
    if (((a % b) != 0) && ((a < 0) != (b < 0)))
        return ek_val_int(a / b - 1);
    return ek_val_int(a / b);
}

static ek_val int_fmod(int64_t a, int64_t b)
{
    if (b == 0)
        die_div();
    if (a == INT64_MIN && b == -1)
        die_overflow();
    if (((a % b) != 0) && ((a < 0) != (b < 0)))
        return ek_val_int(a % b + b);
    return ek_val_int(a % b);
}

/* The absolute value: negating the Int minimum has no result and
   panics like the arithmetic above; the unsigned word is its own
   absolute value. */
ek_val ek_num_abs(ek_val a)
{
    switch (a.tag) {
    case EK_VAL_INT:
        if (a.as.i >= 0)
            return a;
        if (a.as.i == INT64_MIN)
            die_overflow();
        return ek_val_int(-a.as.i);
    case EK_VAL_UINT:  return a;
    case EK_VAL_FLOAT: return ek_val_float(a.as.f < 0 ? -a.as.f : a.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_fdiv(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_fdiv(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_div(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return float_div(a.as.f, b.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_fmod(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_fmod(a.as.i, b.as.i);
    case EK_VAL_UINT:  return uint_mod(a.as.u, b.as.u);
    case EK_VAL_FLOAT: return float_mod(a.as.f, b.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_neg(ek_val a)
{
    switch (a.tag) {
    case EK_VAL_INT:   return int_neg(a.as.i);
    case EK_VAL_UINT:  return uint_neg(a.as.u);
    case EK_VAL_FLOAT: return ek_val_float(-a.as.f);
    default:           return no_word();
    }
}

ek_val ek_num_eq(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return ek_bool(a.as.i == b.as.i);
    case EK_VAL_UINT:  return ek_bool(a.as.u == b.as.u);
    case EK_VAL_FLOAT: return ek_bool(a.as.f == b.as.f);
    case EK_VAL_CHAR:  return ek_bool(a.as.c == b.as.c);
    default:           return ek_bool(0);
    }
}

ek_val ek_num_ne(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return ek_bool(a.as.i != b.as.i);
    case EK_VAL_UINT:  return ek_bool(a.as.u != b.as.u);
    case EK_VAL_FLOAT: return ek_bool(a.as.f != b.as.f);
    case EK_VAL_CHAR:  return ek_bool(a.as.c != b.as.c);
    default:           return ek_bool(0);
    }
}

ek_val ek_num_lt(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return ek_bool(a.as.i < b.as.i);
    case EK_VAL_UINT:  return ek_bool(a.as.u < b.as.u);
    case EK_VAL_FLOAT: return ek_bool(a.as.f < b.as.f);
    case EK_VAL_CHAR:  return ek_bool(a.as.c < b.as.c);
    default:           return ek_bool(0);
    }
}

ek_val ek_num_le(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return ek_bool(a.as.i <= b.as.i);
    case EK_VAL_UINT:  return ek_bool(a.as.u <= b.as.u);
    case EK_VAL_FLOAT: return ek_bool(a.as.f <= b.as.f);
    case EK_VAL_CHAR:  return ek_bool(a.as.c <= b.as.c);
    default:           return ek_bool(0);
    }
}

ek_val ek_num_gt(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return ek_bool(a.as.i > b.as.i);
    case EK_VAL_UINT:  return ek_bool(a.as.u > b.as.u);
    case EK_VAL_FLOAT: return ek_bool(a.as.f > b.as.f);
    case EK_VAL_CHAR:  return ek_bool(a.as.c > b.as.c);
    default:           return ek_bool(0);
    }
}

ek_val ek_num_ge(ek_val a, ek_val b)
{
    switch (a.tag) {
    case EK_VAL_INT:   return ek_bool(a.as.i >= b.as.i);
    case EK_VAL_UINT:  return ek_bool(a.as.u >= b.as.u);
    case EK_VAL_FLOAT: return ek_bool(a.as.f >= b.as.f);
    case EK_VAL_CHAR:  return ek_bool(a.as.c >= b.as.c);
    default:           return ek_bool(0);
    }
}

void ek_panic_value(ek_val msg)
{
    if (msg.tag == EK_VAL_REF && msg.as.ref != NULL
        && msg.as.ref->info->kind == EK_KIND_STR)
        ek_panic("P-panic", ek_str_cstr(msg.as.ref));
    ek_panic("P-panic", "panic of a non-string");
}
