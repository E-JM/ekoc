#include "common.h"

/* The namespace rows: the scalar domain bounds read through the
   type's name, the integer power, the decimal text forms of the
   float, and the byte reader whose index travels as a value. */

static void scalar_bounds_read(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    a = Int.min.lt(Int.max)\n"
              "    b = UInt.max.to_int()\n"
              "    _ := a\n"
              "    _ := b\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the bounds are born over the value slots their domains travel
       in, unowned, wherever the qualified head reads them */
    EK_CHECK(lfx_has(&f, "ek_val_int(INT64_MIN)"));
    EK_CHECK(lfx_has(&f, "ek_val_int(INT64_MAX)"));
    EK_CHECK(lfx_has(&f, "ek_val_uint(UINT64_MAX)"));
    lfx_end(&f);
}

static void int_pow_row_routes(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    n = 2.pow(8)\n"
              "    _ := n\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the literal receiver and the exponent pass as the value slots */
    EK_CHECK(lfx_has(&f, "ek_int_pow("));
    lfx_end(&f);
}

static void decimal_text_rows_route(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    s = Float.to_fixed(3.14159, 2)\n"
              "    t = Float.to_exp(3.14159, 2)\n"
              "    _ := s\n"
              "    _ := t\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the qualified head supplies the receiver value and the digit
       count as the two value slots; each form renders a fresh string
       object */
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_show_fixed("));
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_show_exp("));
    lfx_end(&f);
}

static void bytes_get_reads_value_index(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    b = \"ab\".to_bytes()\n"
              "    c = b.get(0)\n"
              "    _ := c\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the block passes as the object and the index as the value; the
       object-only reference shows once, on the block slot */
    EK_CHECK(lfx_has(&f, "ek_bytes_get("));
    lfx_end(&f);
}

int main(void)
{
    scalar_bounds_read();
    int_pow_row_routes();
    decimal_text_rows_route();
    bytes_get_reads_value_index();
    return ek_test_report("lower/nsrows");
}
