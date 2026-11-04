#include "common.h"

/* The generic specialization in the emitted text: one body per
   (function, applied type arguments) pair, the instantiated name
   spelling the applied arguments after the `$` separator, the
   substituted derived dispatch answering the instantiation's concrete
   services, the self tail-call transform intact in a specialized
   body, and the non-generic names untouched. */

/* Two applied instantiations of one generic emit two bodies with
   distinct names; the non-generic main keeps its plain name. */
static void instantiation_naming(void)
{
    lfx f;

    lfx_begin(&f,
              "Person: type = [name: String, age: Int]\n"
              "main: () -> _ =\n"
              "    assert_eq(41, 42)\n"
              "    a: Person = {name: \"ada\", age: 36}\n"
              "    b: Person = {name: \"ada\", age: 37}\n"
              "    assert_eq(a, b)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_std$assert_eq$Int"
                         "(ek_val ek_p0, ek_val ek_p1);\n"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_std$assert_eq$N$test$Person"
                         "(ek_val ek_p0, ek_val ek_p1);\n"));
    EK_CHECK(lfx_has(&f, "ekf_std$assert_eq$Int("));
    EK_CHECK(lfx_has(&f, "ekf_std$assert_eq$N$test$Person("));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$main(void);\n"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$main(void)\n"));
    /* one body per instantiation: the applied Int site never calls
       the Person one */
    EK_CHECK(lfx_lacks(&f, "ekf_std$assert_eq("));
    lfx_end(&f);
}

/* The specialized body's derived dispatch reads the instantiation's
   concrete services: the record's eq in the case, the record's
   inspect in the interpolated panic message. */
static void substituted_dispatch(void)
{
    lfx f;

    lfx_begin(&f,
              "Person: type = [name: String, age: Int]\n"
              "main: () -> _ =\n"
              "    a: Person = {name: \"ada\", age: 36}\n"
              "    b: Person = {name: \"bob\", age: 36}\n"
              "    assert_eq(a, b)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    {
        const char *body = strstr(f.low.text,
                                  "ekf_std$assert_eq$N$test$Person("
                                  "ek_val ek_p0, ek_val ek_p1)\n{");

        EK_CHECK(body != NULL);
        EK_CHECK(strstr(body, "ekq_N$test$Person(") != NULL);
        EK_CHECK(strstr(body, "ekd_inspect_N$test$Person(") != NULL);
    }
    lfx_end(&f);
}

/* The scalar instantiation keeps the runtime comparisons: the
   specialized body folds to ek_num_eq over the Int slots. */
static void scalar_dispatch(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    assert_eq(41, 41)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    {
        const char *body = strstr(f.low.text,
                                  "ekf_std$assert_eq$Int("
                                  "ek_val ek_p0, ek_val ek_p1)\n{");

        EK_CHECK(body != NULL);
        EK_CHECK(strstr(body, "ek_num_eq(") != NULL);
    }
    lfx_end(&f);
}

/* A generic recursion keeps the retry transform: the specialized body
   of a tail self-calling generic carries the label and the jump. */
static void self_recursion_retry(void)
{
    lfx f;

    lfx_begin(&f,
              "down: (xs: []t, n: Int) -> Int =\n"
              "    case xs:\n"
              "        {} -> n\n"
              "        {_, ..rest} -> down(rest, n + 1)\n"
              "main: () -> _ =\n"
              "    xs: []Int = {1, 2, 3}\n"
              "    d = 1 / (down(xs, 0) - 3)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$down$Int"
                         "(ek_val ek_p0, ek_val ek_p1);\n"));
    {
        const char *body = strstr(f.low.text,
                                  "ekf_test$down$Int("
                                  "ek_val ek_p0, ek_val ek_p1)\n{");

        EK_CHECK(body != NULL);
        EK_CHECK(strstr(body, "retry: ;\n") != NULL);
        EK_CHECK(strstr(body, "goto retry;\n") != NULL);
    }
    lfx_end(&f);
}

/* A generic composed through another generic keys the inner
   emission by the substituted arguments: one Int instantiation of the
   std map serves both inner calls of the wrapper's body. */
static void generic_to_generic(void)
{
    lfx f;

    lfx_begin(&f,
              "twice: (xs: []t, f: (t) ?-> t) ?-> []t =\n"
              "    List.map(List.map(xs, f), f)\n"
              "main: () -> _ =\n"
              "    xs: []Int = {1, 2}\n"
              "    ys = twice(xs, (x): x + 1)\n"
              "    d = 1 / (ys.get(1).unwrap_or(0) - 3)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$twice$Int"
                         "(ek_val ek_p0, ek_val ek_p1);\n"));
    EK_CHECK(lfx_has(&f, "ekf_std$List$map$Int$Int("));
    EK_CHECK(lfx_lacks(&f, "ekf_std$List$map("));
    lfx_end(&f);
}

/* The lambda a generic body builds lowers under the same
   instantiation: List.sort's own comparator reads the ordering
   service of the applied element type inside the closure body. */
static void lambda_in_specialized_body(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    xs: []String = {\"b\", \"a\"}\n"
              "    ys = xs.sort()\n"
              "    d = 1 / (ys.get(0).unwrap_or(\"\").length() - 2)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekf_std$List$sort$String("));
    EK_CHECK(lfx_has(&f, "eko_String("));
    lfx_end(&f);
}

/* Non-generic functions keep today's single names, and a generic
   function used as a value still emits its plain body over the
   trampoline. */
static void plain_names_stand(void)
{
    lfx f;

    lfx_begin(&f,
              "double: (x: Int) -> Int = x * 2\n"
              "apply: (f: (Int) -> Int, x: Int) -> Int = f(x)\n"
              "main: () -> _ =\n"
              "    d = 1 / (apply(double, 21) - 42)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$double(ek_val ek_p0);\n"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$apply(ek_val ek_p0, "
                         "ek_val ek_p1);\n"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$main(void);\n"));
    EK_CHECK(lfx_lacks(&f, "ekf_test$apply$"));
    EK_CHECK(lfx_lacks(&f, "ekf_test$double$"));
    lfx_end(&f);
}

int main(void)
{
    instantiation_naming();
    substituted_dispatch();
    scalar_dispatch();
    self_recursion_retry();
    generic_to_generic();
    lambda_in_specialized_body();
    plain_names_stand();
    return ek_test_report("lower/spec");
}
