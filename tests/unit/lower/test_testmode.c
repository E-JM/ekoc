#include "common.h"

/* The TESTS mode: the root module's test bindings are the roots and
   the unit ends with the dispatching shim; `main` is not a root. The
   ek_lower wrapper keeps the program shape. */

static const char *two_tests =
    "adds: test =\n"
    "    assert_eq(1 + 1, 2)\n"
    "\n"
    "fails: test =\n"
    "    x = 1 / 0\n"
    "    _\n";

static void tests_are_roots(void)
{
    lfx f;

    lfx_begin_tests(&f, two_tests);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* both tests, plus the std assert_eq instance their bodies call */
    EK_CHECK_U64(f.low.fn_count, 3);
    /* the dispatching shim matches each declared name and calls the
       mangled binding, releasing its value */
    EK_CHECK(lfx_has(&f, "int main(int argc, char **argv)\n{\n"));
    EK_CHECK(lfx_has(&f, "if (argc == 2) {\n"));
    EK_CHECK(lfx_has(&f, "if (strcmp(argv[1], \"adds\") == 0) {\n"));
    EK_CHECK(lfx_has(&f, "ek_val t = ekf_test_adds();\n"
                         "            ek_release(t);\n"
                         "            return 0;\n"));
    EK_CHECK(lfx_has(&f, "if (strcmp(argv[1], \"fails\") == 0) {\n"
                         "            ek_val t = ekf_test_fails();"));
    /* a missing or unknown name is a refused run, never a crash */
    EK_CHECK(lfx_has(&f, "    return 2;\n}\n"));
    /* no program shim stands next to it */
    EK_CHECK(lfx_lacks(&f, "int main(void)"));
    lfx_end(&f);
}

static const char *main_and_test =
    "main: () -> _ =\n    _\n"
    "\n"
    "counts: test =\n"
    "    assert_eq(2, 2)\n";

static void main_is_not_a_root(void)
{
    lfx f;

    lfx_begin_tests(&f, main_and_test);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* only the test binding lowers, plus the std assert_eq instance
       its body calls; the module's `main` stays out */
    EK_CHECK_U64(f.low.fn_count, 2);
    EK_CHECK(lfx_has(&f, "ekf_test_counts"));
    EK_CHECK(lfx_lacks(&f, "ekf_test_main"));
    EK_CHECK(lfx_lacks(&f, "int main(void)"));
    lfx_end(&f);
}

static void program_mode_unchanged(void)
{
    lfx f;

    /* the wrapper keeps the program roots: `main` lowers, the test
       binding is unreachable and joins no body */
    lfx_begin(&f, main_and_test);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 1);
    EK_CHECK(lfx_has(&f, "int main(void)\n{\n"));
    EK_CHECK(lfx_has(&f, "ek_val t = ekf_test_main();\n"
                         "    ek_release(t);\n    return 0;\n"));
    EK_CHECK(lfx_lacks(&f, "ekf_test_counts"));
    EK_CHECK(lfx_lacks(&f, "strcmp(argv[1]"));
    lfx_end(&f);
}

static const char *helper_and_test =
    "double: (x: Int) -> Int = x * 2\n"
    "\n"
    "doubles: test =\n"
    "    assert_eq(double(2), 4)\n";

static void tests_reach_helpers(void)
{
    lfx f;

    lfx_begin_tests(&f, helper_and_test);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* reachability starts at the test binding and pulls its helpers
       in, the std assert_eq instance among them */
    EK_CHECK_U64(f.low.fn_count, 3);
    EK_CHECK(lfx_has(&f, "ekf_test_doubles"));
    EK_CHECK(lfx_has(&f, "ekf_test_double"));
    lfx_end(&f);
}

static void dispatch_follows_declaration(void)
{
    lfx f;
    const char *text;
    const char *first;
    const char *second;

    lfx_begin_tests(&f, two_tests);
    text = f.low.text;
    first = strstr(text, "\"adds\"");
    second = strstr(text, "\"fails\"");
    EK_CHECK(first != NULL && second != NULL && first < second);
    lfx_end(&f);
}

static void non_unit_test_value(void)
{
    lfx f;

    /* a test body's value is whatever the body's last expression is;
       the shim discards it */
    lfx_begin_tests(&f, "probe: () -> Result Int String = Ok 1\n"
                        "\n"
                        "wrapped: test =\n"
                        "    assert_eq(1, 1)\n"
                        "    probe()\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_val t = ekf_test_wrapped();\n"
                         "            ek_release(t);"));
    lfx_end(&f);
}

static void panicking_reads(void)
{
    lfx f;

    /* the compiler rows `unwrap` and `expect` lower to the variant
       test over the shared std families and the runtime's panic */
    lfx_begin_tests(&f, "probe: (o: Option Int) -> Int =\n"
                        "    o.expect(\"empty\")\n"
                        "\n"
                        "tries: test =\n"
                        "    assert_eq(probe(Some 3), 3)\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "if (t0.as.ref->info != &ekT_std_Option_Some) {"));
    EK_CHECK(lfx_has(&f, "ek_panic(\"P-expect\", msgc);"));
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_std_Option_Some *)t0.as.ref)->p0)"));
    lfx_end(&f);

    lfx_begin_tests(&f, "broken: test =\n"
                        "    r: Result Int String = Err \"bad\"\n"
                        "    x = r.unwrap()\n"
                        "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_panic(\"P-unwrap\", \"Result.unwrap on Err\");"));
    lfx_end(&f);
}

static void free_namespace_call(void)
{
    lfx f;

    /* `show(p)` is the free-call form of a declared namespace member:
       the first argument is the receiver */
    lfx_begin_tests(&f, "P: type = [x: Int]\n"
                        "\n"
                        "P.show: (p: P) -> String = \"P({p.x})\"\n"
                        "\n"
                        "shows: test =\n"
                        "    p: P = {x: 1}\n"
                        "    assert_eq(show(p), \"P(1)\")\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "= ekf_test_P_show("));
    lfx_end(&f);
}

int main(void)
{
    tests_are_roots();
    main_is_not_a_root();
    program_mode_unchanged();
    tests_reach_helpers();
    dispatch_follows_declaration();
    non_unit_test_value();
    panicking_reads();
    free_namespace_call();
    return ek_test_report("lower/testmode");
}
