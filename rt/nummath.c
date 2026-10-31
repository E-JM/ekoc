#include "ekort.h"

#include <math.h>

/* The floating-point mathematics surface: each entry reads the Float
   slot and answers the Float slot, C99's libm under the names the
   standard pins, and the three predicates answer Bool. Static typing
   leaves no other tag in these entries; the neutral unit value keeps
   the dispatch total without reading a payload. */

static ek_val float1(double (*f)(double), double x)
{
    return ek_val_float(f(x));
}

ek_val ek_num_floor(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(floor, a.as.f) : ek_val_unit();
}

ek_val ek_num_ceil(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(ceil, a.as.f) : ek_val_unit();
}

ek_val ek_num_trunc(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(trunc, a.as.f) : ek_val_unit();
}

ek_val ek_num_round(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(round, a.as.f) : ek_val_unit();
}

ek_val ek_num_sqrt(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(sqrt, a.as.f) : ek_val_unit();
}

ek_val ek_num_exp(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(exp, a.as.f) : ek_val_unit();
}

ek_val ek_num_log(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(log, a.as.f) : ek_val_unit();
}

ek_val ek_num_log2(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(log2, a.as.f) : ek_val_unit();
}

ek_val ek_num_log10(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(log10, a.as.f) : ek_val_unit();
}

ek_val ek_num_sin(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(sin, a.as.f) : ek_val_unit();
}

ek_val ek_num_cos(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(cos, a.as.f) : ek_val_unit();
}

ek_val ek_num_tan(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(tan, a.as.f) : ek_val_unit();
}

ek_val ek_num_asin(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(asin, a.as.f) : ek_val_unit();
}

ek_val ek_num_acos(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(acos, a.as.f) : ek_val_unit();
}

ek_val ek_num_atan(ek_val a)
{
    return a.tag == EK_VAL_FLOAT ? float1(atan, a.as.f) : ek_val_unit();
}

ek_val ek_num_pow(ek_val a, ek_val b)
{
    if (a.tag != EK_VAL_FLOAT || b.tag != EK_VAL_FLOAT) {
        return ek_val_unit();
    }
    return ek_val_float(pow(a.as.f, b.as.f));
}

ek_val ek_num_atan2(ek_val a, ek_val b)
{
    if (a.tag != EK_VAL_FLOAT || b.tag != EK_VAL_FLOAT) {
        return ek_val_unit();
    }
    return ek_val_float(atan2(a.as.f, b.as.f));
}

int ek_num_is_nan(ek_val a)
{
    return a.tag == EK_VAL_FLOAT && isnan(a.as.f);
}

int ek_num_is_inf(ek_val a)
{
    return a.tag == EK_VAL_FLOAT && isinf(a.as.f);
}

int ek_num_is_finite(ek_val a)
{
    return a.tag == EK_VAL_FLOAT && isfinite(a.as.f);
}
