#include "common.h"

/* The top-level value bindings and the flat tuple destructuring: the
   lazy slot shape, the reference discipline, reachability, and the
   capture descent that keeps a nested lambda's free names reachable
   through an inverted binding's continuation. */

static void value_slot_shape(void)
{
    lfx f;

    lfx_begin(&f,
              "_path: String = \"todos.json\"\n"
              "main: (out: Out) ! =\n"
              "    out.println(_path)\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the slot, its flag, and the getter stand once */
    EK_CHECK(lfx_has(&f, "static ek_val ekv_test__path;\n"));
    EK_CHECK(lfx_has(&f, "static int ekv_test__path_on;\n"));
    EK_CHECK(lfx_has(&f, "static ek_val eks_test__path(void)\n"));
    /* the initializer runs once behind the flag and the reference
       reads the getter */
    EK_CHECK(lfx_has(&f, "if (!ekv_test__path_on) {\n"));
    EK_CHECK(lfx_has(&f, "ekv_test__path = "));
    EK_CHECK(lfx_has(&f, "return ek_keep(ekv_test__path);"));
    EK_CHECK(lfx_has(&f, "eks_test__path()"));
    lfx_end(&f);
}

static void value_reachable_only_when_read(void)
{
    lfx f;

    /* a binding nothing reads joins no body */
    lfx_begin(&f,
              "_unused: String = \"quiet\"\n"
              "main: (out: Out) ! = out.println(\"loud\")\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_lacks(&f, "ekv_test__unused"));
    EK_CHECK(lfx_lacks(&f, "eks_test__unused"));

    /* the initializer's own references pull their definitions: the
       getter of the outer binding calls the getter of the inner one */
    lfx_begin(&f,
              "_inner: String = \"a\"\n"
              "_outer: String = _inner ++ \"b\"\n"
              "main: (out: Out) ! = out.println(_outer)\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "eks_test__inner()"));
    lfx_end(&f);
}

static void value_record_initializer(void)
{
    lfx f;

    /* a declared record value constructs through its family inside
       the getter, and a field read rides the reference */
    lfx_begin(&f,
              "Store: type = [next_id: Int, tasks: []String]\n"
              "empty: Store = {next_id: 1, tasks: {}}\n"
              "main: (out: Out) ! =\n"
              "    out.println(show(empty.next_id))\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekn_test_Store_rec("));
    EK_CHECK(lfx_has(&f, "return ek_keep(ekv_test_empty);"));
    lfx_end(&f);
}

static void tuple_let_binds_components(void)
{
    lfx f;

    /* the flat destructuring reads the generated tuple slots as owned
       copies and releases the tuple temp */
    lfx_begin(&f,
              "pair: [Int, String] = {7, \"x\"}\n"
              "main: (out: Out) ! =\n"
              "    (a, b) = pair\n"
              "    out.println(show(a))\n"
              "    out.println(b)\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_tup_2 *)t"));
    EK_CHECK(lfx_has(&f, ")->f_0);"));
    EK_CHECK(lfx_has(&f, ")->f_1);"));
    lfx_end(&f);

    /* the wildcard component dies with the tuple */
    lfx_begin(&f,
              "pair: [Int, String] = {7, \"x\"}\n"
              "main: (out: Out) ! =\n"
              "    (a, _) = pair\n"
              "    out.println(show(a))\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, ")->f_0);"));
    EK_CHECK(lfx_lacks(&f, ")->f_1);\n    "));
    lfx_end(&f);
}

static void capture_descends_into_lambda(void)
{
    lfx f;

    /* a free name read only inside a lambda that sits in an inverted
       binding's continuation must reach the continuation's capture
       set, or the inner lambda's read strands */
    lfx_begin(&f,
              "g: (s: String) -> Result String String = Ok s\n"
              "f: (path: String) -> Result Int String =\n"
              "    a <- g(path).try()\n"
              "    b <- Int.parse(a).map_err((e): \"bad {path}: {e}\")"
              ".try()\n"
              "    Ok b\n"
              "main: (out: Out) ! =\n"
              "    case f(\"x\"):\n"
              "        Ok n -> out.println(show(n))\n"
              "        Err e -> out.println(e)\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the continuation's environment carries the name the inner
       lambda reads */
    EK_CHECK(lfx_has(&f, "->f_path"));
    lfx_end(&f);
}

int main(void)
{
    value_slot_shape();
    value_reachable_only_when_read();
    value_record_initializer();
    tuple_let_binds_components();
    capture_descends_into_lambda();
    return ek_test_report("lower/values");
}
