#include "common.h"

/* The binding forms: the discard bindings, the fresh-name rebind, and
   the rebind whose right-hand side is a block-valued form. */

static void discard_let(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    _ = 1 + 2\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    /* the value dies and nothing is bound */
    EK_CHECK(lfx_has(&f, "ek_val t2 = ek_num_add(t0, t1);\n"
                         "    ek_release(t1);\n    ek_release(t0);\n"
                         "    ek_release(t2);"));
    EK_CHECK(lfx_lacks(&f, "ek_val _"));
    lfx_end(&f);
}

static void discard_rebind(void)
{
    lfx f;

    lfx_begin(&f, "id: (x: Int) -> Int = x\n"
                  "main: () -> _ =\n"
                  "    _ := id(7)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 2);
    /* the call result dies right after the call */
    EK_CHECK(lfx_has(&f, "ek_val t1 = ekf_test_id(t0);\n"
                         "    ek_release(t0);\n    ek_release(t1);"));
    EK_CHECK(lfx_lacks(&f, "ek_val _"));
    /* the binding statement's value is the unit value */
    EK_CHECK(lfx_has(&f, "ek_val t2 = ek_val_unit();\n    return t2;"));
    lfx_end(&f);
}

static void fresh_rebind(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    y := 9\n"
                  "    d = 1 / (10 - y)\n"
                  "    _ := d\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* a name no scope carries yet is bound fresh, as check binds it */
    EK_CHECK(lfx_has(&f, "ek_val y = t0;"));
    EK_CHECK(lfx_has(&f, "ek_num_sub("));
    lfx_end(&f);
}

static const char *rebind_if_src =
    "main: () -> _ =\n"
    "    x = 5\n"
    "    x := if x == 5: 6 else: 7\n"
    "    d = 1 / (7 - x)\n"
    "    _ := d\n"
    "    _\n";

static void rebind_if(void)
{
    lfx f;

    lfx_begin(&f, rebind_if_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    /* the if result is a temp of the block, the old value releases,
       and the temp moves in */
    EK_CHECK(lfx_has(&f, "ek_release(x);\n    x = t3;"));
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_val_unit();"));
    lfx_end(&f);
}

static void rebind_if_fresh(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    y := if 1 == 1: 2 else: 3\n"
                  "    d = 1 / (3 - y)\n"
                  "    _ := d\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the fresh binding takes the if's result temp directly */
    EK_CHECK(lfx_has(&f, "ek_val y = t"));
    lfx_end(&f);
}

static void rebind_string(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    s = \"a\"\n"
                  "    s := if 1 == 1: \"b\" else: s\n"
                  "    _ := s\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* a heap-valued rebind releases the old object exactly once */
    EK_CHECK(lfx_has(&f, "ek_release(s);\n    s = t"));
    lfx_end(&f);
}

int main(void)
{
    discard_let();
    discard_rebind();
    fresh_rebind();
    rebind_if();
    rebind_if_fresh();
    rebind_string();
    return ek_test_report("lower/bind");
}
