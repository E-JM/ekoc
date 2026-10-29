#include "ekort.h"

#include <stdint.h>

/* The conversion family between the domain words and the machine
   widths. Each narrowing entry dispatches on the source tag: Int
   checks both ends of the target, UInt only the top, and Float
   truncates toward zero after checking the value is finite and in
   range. A tag outside the three slots has no row and reports a
   miss. */

/* The Int sources: the value must lie inside the closed range. */
static int int_to_iwidth(int64_t v, int64_t lo, int64_t hi, int64_t *out)
{
    if (v < lo || v > hi)
        return 0;
    *out = v;
    return 1;
}

static int int_to_uwidth(int64_t v, uint64_t hi, uint64_t *out)
{
    if (v < 0 || (uint64_t)v > hi)
        return 0;
    *out = (uint64_t)v;
    return 1;
}

/* The UInt sources: the value is never negative, so only the top of
   the target decides. */
static int uint_to_iwidth(uint64_t v, int64_t hi, int64_t *out)
{
    if (v > (uint64_t)hi)
        return 0;
    *out = (int64_t)v;
    return 1;
}

static int uint_to_uwidth(uint64_t v, uint64_t hi, uint64_t *out)
{
    if (v > hi)
        return 0;
    *out = v;
    return 1;
}

/* The Float sources: truncation toward zero over the target. below
   is the first value the target refuses, so the fraction between it
   and the minimum still truncates in; hi is the first value above
   the maximum. The 64-bit edges pass the limits themselves: the
   doubles step over the fractional band there. */
static int float_to_iwidth(double x, double below, double hi, int64_t *out)
{
    if (!(x > below) || !(x < hi))
        return 0;
    *out = (int64_t)x;
    return 1;
}

static int float_to_i64(double x, int64_t *out)
{
    if (!(x >= -9223372036854775808.0) || !(x < 9223372036854775808.0))
        return 0;
    *out = (int64_t)x;
    return 1;
}

/* The sign is checked before truncation, so -0.5 misses while -0.0
   truncates to zero. */
static int float_to_uwidth(double x, double hi, uint64_t *out)
{
    if (x < 0.0 || !(x < hi))
        return 0;
    *out = (uint64_t)x;
    return 1;
}

int ek_try_to_int(ek_val v, int64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   *out = v.as.i; return 1;
    case EK_VAL_UINT:  return uint_to_iwidth(v.as.u, INT64_MAX, out);
    case EK_VAL_FLOAT: return float_to_i64(v.as.f, out);
    default:           return 0;
    }
}

int ek_try_to_uint(ek_val v, uint64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_uwidth(v.as.i, UINT64_MAX, out);
    case EK_VAL_UINT:  *out = v.as.u; return 1;
    case EK_VAL_FLOAT: return float_to_uwidth(v.as.f, 18446744073709551616.0,
                                              out);
    default:           return 0;
    }
}

int ek_try_to_i8(ek_val v, int64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_iwidth(v.as.i, -128, 127, out);
    case EK_VAL_UINT:  return uint_to_iwidth(v.as.u, 127, out);
    case EK_VAL_FLOAT: return float_to_iwidth(v.as.f, -129.0, 128.0, out);
    default:           return 0;
    }
}

int ek_try_to_i16(ek_val v, int64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_iwidth(v.as.i, -32768, 32767, out);
    case EK_VAL_UINT:  return uint_to_iwidth(v.as.u, 32767, out);
    case EK_VAL_FLOAT: return float_to_iwidth(v.as.f, -32769.0, 32768.0, out);
    default:           return 0;
    }
}

int ek_try_to_i32(ek_val v, int64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_iwidth(v.as.i, -2147483647 - 1,
                                            2147483647, out);
    case EK_VAL_UINT:  return uint_to_iwidth(v.as.u, 2147483647, out);
    case EK_VAL_FLOAT: return float_to_iwidth(v.as.f, -2147483649.0,
                                              2147483648.0, out);
    default:           return 0;
    }
}

int ek_try_to_i64(ek_val v, int64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   *out = v.as.i; return 1;
    case EK_VAL_UINT:  return uint_to_iwidth(v.as.u, INT64_MAX, out);
    case EK_VAL_FLOAT: return float_to_i64(v.as.f, out);
    default:           return 0;
    }
}

int ek_try_to_u8(ek_val v, uint64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_uwidth(v.as.i, 255, out);
    case EK_VAL_UINT:  return uint_to_uwidth(v.as.u, 255, out);
    case EK_VAL_FLOAT: return float_to_uwidth(v.as.f, 256.0, out);
    default:           return 0;
    }
}

int ek_try_to_u16(ek_val v, uint64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_uwidth(v.as.i, 65535, out);
    case EK_VAL_UINT:  return uint_to_uwidth(v.as.u, 65535, out);
    case EK_VAL_FLOAT: return float_to_uwidth(v.as.f, 65536.0, out);
    default:           return 0;
    }
}

int ek_try_to_u32(ek_val v, uint64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_uwidth(v.as.i, 4294967295, out);
    case EK_VAL_UINT:  return uint_to_uwidth(v.as.u, 4294967295, out);
    case EK_VAL_FLOAT: return float_to_uwidth(v.as.f, 4294967296.0, out);
    default:           return 0;
    }
}

int ek_try_to_u64(ek_val v, uint64_t *out)
{
    switch (v.tag) {
    case EK_VAL_INT:   return int_to_uwidth(v.as.i, UINT64_MAX, out);
    case EK_VAL_UINT:  *out = v.as.u; return 1;
    case EK_VAL_FLOAT: return float_to_uwidth(v.as.f, 18446744073709551616.0,
                                              out);
    default:           return 0;
    }
}

/* The total rows: the widening directions cannot miss. The machine
   words travel in the slots, so an F32 result rounds through the
   binary32 width and lands back in the Float slot. A tag outside the
   three slots yields the neutral unit value. */
ek_val ek_to_float(ek_val v)
{
    switch (v.tag) {
    case EK_VAL_INT:   return ek_val_float((double)v.as.i);
    case EK_VAL_UINT:  return ek_val_float((double)v.as.u);
    case EK_VAL_FLOAT: return v;
    default:           return ek_val_unit();
    }
}

ek_val ek_to_f32(ek_val v)
{
    switch (v.tag) {
    case EK_VAL_INT:   return ek_val_float((double)(float)v.as.i);
    case EK_VAL_UINT:  return ek_val_float((double)(float)v.as.u);
    case EK_VAL_FLOAT: return ek_val_float((double)(float)v.as.f);
    default:           return ek_val_unit();
    }
}

ek_val ek_to_f64(ek_val v)
{
    switch (v.tag) {
    case EK_VAL_INT:   return ek_val_float((double)v.as.i);
    case EK_VAL_UINT:  return ek_val_float((double)v.as.u);
    case EK_VAL_FLOAT: return v;
    default:           return ek_val_unit();
    }
}
