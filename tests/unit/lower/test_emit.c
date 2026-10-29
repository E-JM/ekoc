#include "common.h"

/* The emitted text: reachability from `main`, the naming scheme, the
   operator mapping, and the ownership shape at call sites, bindings,
   and block ends. */

static const char *reach_src =
    "_helper: (x: Int) -> Int = x + 1\n"
    "unused: (x: Int) -> Int = x * 2\n"
    "add2: (a: Int, b: Int) -> Int = _helper(a) + _helper(b)\n"
    "main: () -> _ =\n"
    "    v = add2(1, 2)\n"
    "    _\n";

static void reachability(void)
{
    lfx f;

    lfx_begin(&f, reach_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 3);
    EK_CHECK(lfx_has(&f, "#include \"ekort.h\""));
    EK_CHECK(lfx_has(&f, "static ek_val ek_keep(ek_val v) "
                         "{ ek_retain(v); return v; }"));
    /* every emitted function is declared first */
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test_main(void);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test_add2(ek_val a, "
                         "ek_val b);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test__helper(ek_val x);"));
    /* the untouched std surface and the unreferenced binding are
       absent */
    EK_CHECK(lfx_lacks(&f, "ekf_test_unused"));
    EK_CHECK(lfx_lacks(&f, "ekf_std_"));
    EK_CHECK(lfx_has(&f, "int main(void) { ek_val t = ekf_test_main(); "
                         "ek_release(t); return 0; }"));
    lfx_end(&f);
}

static void ownership(void)
{
    lfx f;

    lfx_begin(&f, reach_src);
    /* a name argument becomes an owned copy, released after the call */
    EK_CHECK(lfx_has(&f, "ek_val t0 = ek_keep(a);"));
    EK_CHECK(lfx_has(&f, "ek_val t1 = ekf_test__helper(t0);"));
    EK_CHECK(lfx_has(&f, "ek_release(t0);"));
    /* the callee never releases its borrowed parameters */
    EK_CHECK(lfx_lacks(&f, "ek_release(x)"));
    EK_CHECK(lfx_lacks(&f, "ek_release(a)"));
    EK_CHECK(lfx_lacks(&f, "ek_release(b)"));
    /* a binding transfers; the function's own bindings release in
       reverse order before the return */
    EK_CHECK(lfx_has(&f, "ek_val v = t2;"));
    EK_CHECK(lfx_has(&f, "ek_release(v);\n    return t3;"));
    lfx_end(&f);
}

static const char *ops_src =
    "main: () -> _ =\n"
    "    a = 1\n"
    "    b = 2\n"
    "    x = a + b\n"
    "    x := a - b\n"
    "    x := a * b\n"
    "    x := a / b\n"
    "    x := a % b\n"
    "    if a == b: _\n"
    "    if a != b: _\n"
    "    if a < b: _\n"
    "    if a <= b: _\n"
    "    if a > b: _\n"
    "    if a >= b: _\n"
    "    _\n";

static void operators(void)
{
    lfx f;

    lfx_begin(&f, ops_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_num_add("));
    EK_CHECK(lfx_has(&f, "ek_num_sub("));
    EK_CHECK(lfx_has(&f, "ek_num_mul("));
    EK_CHECK(lfx_has(&f, "ek_num_div("));
    EK_CHECK(lfx_has(&f, "ek_num_mod("));
    EK_CHECK(lfx_has(&f, "ek_num_eq("));
    EK_CHECK(lfx_has(&f, "ek_num_ne("));
    EK_CHECK(lfx_has(&f, "ek_num_lt("));
    EK_CHECK(lfx_has(&f, "ek_num_le("));
    EK_CHECK(lfx_has(&f, "ek_num_gt("));
    EK_CHECK(lfx_has(&f, "ek_num_ge("));
    /* the operands are temps, released once the result exists */
    EK_CHECK(lfx_has(&f, "ek_val t2 = ek_num_add(t0, t1);"));
    EK_CHECK(lfx_has(&f, "ek_release(t1);\n    ek_release(t0);"));
    /* a rebind releases the old value and moves the temp in */
    EK_CHECK(lfx_has(&f, "ek_release(x);\n    x = t"));
    lfx_end(&f);
}

static const char *if_src =
    "main: () -> _ =\n"
    "    r = 43\n"
    "    if r == 43:\n"
    "        r := r - 1\n"
    "        _\n"
    "    _\n";

static void if_shape(void)
{
    lfx f;

    lfx_begin(&f, if_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the condition reads the Int slot; the missing else yields the
       unit value in the result temp */
    EK_CHECK(lfx_has(&f, ".as.i) {"));
    EK_CHECK(lfx_has(&f, "} else {"));
    EK_CHECK(lfx_has(&f, "ek_val_unit();"));
    EK_CHECK(lfx_has(&f, "ek_num_eq("));
    /* the branch rebinds the outer binding and the result temp is
       released when the statement discards it */
    EK_CHECK(lfx_has(&f, "ek_release(r);\n        r = t"));
    EK_CHECK(lfx_has(&f, "ek_release(t2);\n    ek_release(t3);"));
    lfx_end(&f);
}

static void literals(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    u: UInt = 7\n"
                  "    i: Int = 8\n"
                  "    n = -5\n"
                  "    x = 1.5\n"
                  "    c = 'x'\n"
                  "    t = True\n"
                  "    g = False\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_val_uint(7ULL)"));
    EK_CHECK(lfx_has(&f, "ek_val_int((int64_t)8ULL)"));
    EK_CHECK(lfx_has(&f, "ek_val_int((int64_t)5ULL)"));
    EK_CHECK(lfx_has(&f, "ek_num_neg("));
    EK_CHECK(lfx_has(&f, "ek_val_float(1.5)"));
    EK_CHECK(lfx_has(&f, "ek_val_char(120u)"));
    EK_CHECK(lfx_has(&f, "ek_bool(1)"));
    EK_CHECK(lfx_has(&f, "ek_bool(0)"));
    lfx_end(&f);
}

static void string_escaping(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    s = \"a\\\"b\\\\c\\nd\\u{1}e\\u{3f}\"\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the quote, backslash, and question mark gain a backslash; the
       newline and the control byte become octal escapes */
    EK_CHECK(lfx_has(&f, "ek_val_ref(ek_str_from_cstr("
                         "\"a\\\"b\\\\c\\012d\\001e\\?\"))"));
    lfx_end(&f);
}

static const char *shadow_src =
    "main: () -> _ =\n"
    "    a = 1\n"
    "    if 1 == 1:\n"
    "        a = 0\n"
    "        _\n"
    "    _\n";

static void shadowing(void)
{
    lfx f;

    lfx_begin(&f, shadow_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the branch's binding is a fresh declaration in its own C block,
       released before the branch closes, so the outer binding resumes */
    EK_CHECK(lfx_has(&f, "ek_val a = ek_val_int((int64_t)1ULL);"));
    EK_CHECK(lfx_has(&f, "        ek_val a = ek_val_int((int64_t)0ULL);"));
    EK_CHECK(lfx_has(&f, "ek_release(a);\n    } else {"));
    lfx_end(&f);
}

static void param_naming(void)
{
    lfx f;

    lfx_begin(&f, "_mix: (x: Int, _: Int) -> Int = x\n"
                  "main: () -> _ =\n"
                  "    q = _mix(1, 2)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the written name stands; the unnamed parameter is numbered */
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test__mix(ek_val x, "
                         "ek_val a1);"));
    lfx_end(&f);
}

static void no_main_no_functions(void)
{
    lfx f;

    lfx_begin(&f, "h: (x: Int) -> Int = x\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 0);
    EK_CHECK(f.low.text != NULL);
    EK_CHECK(lfx_lacks(&f, "ekf_"));
    EK_CHECK(lfx_lacks(&f, "int main(void)"));
    lfx_end(&f);
}

int main(void)
{
    reachability();
    ownership();
    operators();
    if_shape();
    literals();
    string_escaping();
    shadowing();
    param_naming();
    no_main_no_functions();
    return ek_test_report("lower/emit");
}
