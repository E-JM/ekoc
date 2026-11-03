#include "ekort.h"

#include <stdint.h>

/* The ring arithmetic of the machine word: the bitwise family over
   UInt and the wrapping forms over Int and UInt. Every entry computes
   on the integers modulo 2^64 and never panics; truncation is the
   semantics. Static typing guarantees both operands of a binary entry
   carry the UInt tag the family requires. */

/* A shift count of 64 or more yields zero, so the count masks into
   range only after the threshold test. */
static uint64_t shifted(uint64_t a, uint64_t n, int left)
{
    if (n >= 64) {
        return 0;
    }
    return left ? a << n : a >> n;
}

ek_val ek_uint_band(ek_val a, ek_val b)
{
    return ek_val_uint(a.as.u & b.as.u);
}

ek_val ek_uint_bor(ek_val a, ek_val b)
{
    return ek_val_uint(a.as.u | b.as.u);
}

ek_val ek_uint_bxor(ek_val a, ek_val b)
{
    return ek_val_uint(a.as.u ^ b.as.u);
}

ek_val ek_uint_bnot(ek_val a)
{
    return ek_val_uint(~a.as.u);
}

ek_val ek_uint_shl(ek_val a, ek_val b)
{
    return ek_val_uint(shifted(a.as.u, b.as.u, 1));
}

ek_val ek_uint_shr(ek_val a, ek_val b)
{
    return ek_val_uint(shifted(a.as.u, b.as.u, 0));
}

/* The wrapping forms: the signed word travels through the unsigned
   ring and returns, so every result bit pattern is a value. */
ek_val ek_int_wrap_add(ek_val a, ek_val b)
{
    return ek_val_int((int64_t)((uint64_t)a.as.i + (uint64_t)b.as.i));
}

ek_val ek_int_wrap_sub(ek_val a, ek_val b)
{
    return ek_val_int((int64_t)((uint64_t)a.as.i - (uint64_t)b.as.i));
}

ek_val ek_int_wrap_mul(ek_val a, ek_val b)
{
    return ek_val_int((int64_t)((uint64_t)a.as.i * (uint64_t)b.as.i));
}

ek_val ek_int_wrap_neg(ek_val a)
{
    return ek_val_int((int64_t)(0 - (uint64_t)a.as.i));
}

ek_val ek_uint_wrap_add(ek_val a, ek_val b)
{
    return ek_val_uint(a.as.u + b.as.u);
}

ek_val ek_uint_wrap_sub(ek_val a, ek_val b)
{
    return ek_val_uint(a.as.u - b.as.u);
}

ek_val ek_uint_wrap_mul(ek_val a, ek_val b)
{
    return ek_val_uint(a.as.u * b.as.u);
}

ek_val ek_uint_wrap_neg(ek_val a)
{
    return ek_val_uint(0 - a.as.u);
}
