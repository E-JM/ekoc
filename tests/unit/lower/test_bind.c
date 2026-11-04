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
    EK_CHECK(lfx_has(&f, "ek_val t1 = ekf_test$id(t0);\n"
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

/* A lambda-valued function binding beta-reduces: the lambda's own
   parameter names adopt the ABI slots -- also where the annotation is
   silent -- and the lambda's body is the function body, so a call runs
   the lambda instead of returning its closure. */
static void lambda_valued_binding(void)
{
    lfx f;

    lfx_begin(&f, "bar: (String) -> String = (s): \"hi {s}\"\n"
                  "main: () -> _ =\n"
                  "    r = bar(\"x\")\n"
                  "    _ := r\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the call site stays a direct call */
    EK_CHECK(lfx_has(&f, "ekf_test$bar(t"));
    /* the lambda's parameter stays borrowed: no prologue keep, the
       body reads the ABI slot through the alias, and the show slot
       keeps its own copy of it */
    EK_CHECK(lfx_lacks(&f, "ek_val s = "));
    EK_CHECK(lfx_has(&f, "ek_val t1 = ek_keep(ek_p0);"));
    /* no closure is born for the binding's value */
    EK_CHECK(lfx_lacks(&f, "ek_closure_new"));
    lfx_end(&f);
}

/* A same-scope re-let: a read before the second binding resolves to
   the first one, a read after it resolves to the latest one, and both
   bindings release at the scope's close. */
static void relet_lookup(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    v: Int = 1\n"
                  "    u: Int = v + 1\n"
                  "    v: Int = u + 1\n"
                  "    _ := v\n"
                  "    _ := u\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the two bindings carry distinct C names, and each read lands on
       the binding live where it stands */
    EK_CHECK(lfx_has(&f, "ek_val v = ek_val_int((int64_t)1ULL);"));
    EK_CHECK(lfx_has(&f, "ek_val t0 = ek_keep(v);"));
    EK_CHECK(lfx_has(&f, "ek_val v_ = t5;"));
    EK_CHECK(lfx_has(&f, "ek_val t6 = ek_keep(v_);"));
    /* both bindings release at the close, most recent first */
    EK_CHECK(lfx_has(&f, "ek_release(v_);\n"
                         "    ek_release(u);\n"
                         "    ek_release(v);"));
    lfx_end(&f);
}

/* A same-scope re-let of a heap value: the earlier binding keeps
   holding its own object until the scope's close releases it exactly
   once, and the later binding serves the reads. */
static void relet_string(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    s: String = \"first\"\n"
                  "    t: String = s\n"
                  "    s: String = \"second\"\n"
                  "    _ := s\n"
                  "    _ := t\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the read before the re-let keeps the first binding's object,
       the read after it reads the later one */
    EK_CHECK(lfx_has(&f, "ek_val t = ek_keep(s);"));
    EK_CHECK(lfx_has(&f, "ek_val s_ = ek_keep(ekc1);"));
    EK_CHECK(lfx_has(&f, "ek_val t0 = ek_keep(s_);"));
    /* both bindings release exactly once, at the close */
    EK_CHECK(lfx_has(&f, "ek_release(s_);\n"
                         "    ek_release(t);\n"
                         "    ek_release(s);"));
    lfx_end(&f);
}

/* A rebind whose value opens a case, an arm body rebinding the
   parameter: the parameter needs its own reference, so the prologue
   keeps it however deeply the rebind sits. */
static void nested_rebind_keeps_param(void)
{
    lfx f;

    lfx_begin(&f, "pick: (c: Bool, o: Out) ! =\n"
                  "    x := case c:\n"
                  "        True ->\n"
                  "            o := Out.null()\n"
                  "            1\n"
                  "        False -> 2\n"
                  "    o.println(\"done\")\n"
                  "\n"
                  "main: (o: Out) ! =\n"
                  "    pick(True, o)\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_val o = ek_keep(ek_p1);"));
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
    lambda_valued_binding();
    relet_lookup();
    relet_string();
    nested_rebind_keeps_param();
    return ek_test_report("lower/bind");
}
