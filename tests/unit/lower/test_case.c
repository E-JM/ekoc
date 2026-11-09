#include "common.h"

/* The case cascade, the patterns, the self tail-call transform, the
   closures, and the function values: the emitted text shapes that the
   compile-and-run gates hold up at runtime. */

static const char *case_src =
    "Key: type = Up | Down\n"
    "pick: (k: Key) -> Int =\n"
    "    case k:\n"
    "        Up -> 1\n"
    "        Down -> 2\n"
    "main: () -> _ =\n"
    "    v = pick(Up)\n"
    "    _\n";

static void cascade_shape(void)
{
    lfx f;

    lfx_begin(&f, case_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 2);
    /* the scrutinee temp is kept once over the borrowed parameter
       and released at the join */
    EK_CHECK(lfx_has(&f, "ek_val t0 = ek_keep(ek_p0);\n"
                         "    ek_val t1 = ek_val_unit();\n"
                         "    do {"));
    /* the constructor identity is the typeinfo pointer */
    EK_CHECK(lfx_has(&f, "t2 = t2 && (t0.as.ref->info "
                         "== &ekT_test$Key$Up);"));
    EK_CHECK(lfx_has(&f, "t3 = t3 && (t0.as.ref->info "
                         "== &ekT_test$Key$Down);"));
    /* the first match leaves the cascade through the join */
    EK_CHECK(lfx_has(&f, "ek_panic(\"P-panic\", "
                         "\"no matching case arm\");\n"
                         "    } while (0);\n"
                         "    ek_release(t0);"));
    lfx_end(&f);
}

static void literal_patterns(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    n = 4\n"
                  "    r = case n:\n"
                  "        0 -> 1\n"
                  "        -3 -> 2\n"
                  "        _ -> 3\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "t2 = t2 && (t0.as.i == (int64_t)0ULL);"));
    EK_CHECK(lfx_has(&f, "t3 = t3 && (t0.as.i == -(int64_t)3ULL);"));
    lfx_end(&f);
}

static void string_pattern(void)
{
    lfx f;

    lfx_begin(&f, "look: (r: Result Int String) -> Int =\n"
                  "    case r:\n"
                  "        Err \"timeout\" -> 1\n"
                  "        _ -> 2\n"
                  "main: () -> _ =\n"
                  "    v = look(Err \"timeout\")\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the interned literal is kept into the arm and the comparison is
       a pure condition over the payload slot read through the cast */
    EK_CHECK(lfx_has(&f, "ekc0 = ek_val_ref(ek_str_from_cstr("
                         "\"timeout\"));"));
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_keep(ekc0);"));
    EK_CHECK(lfx_has(&f, "ek_str_eq(((ekS_std$Result$Err *)"
                         "t0.as.ref)->p0.as.ref, t3.as.ref)));"));
    EK_CHECK(lfx_has(&f, "ek_release(t3);\n                break;"));
    lfx_end(&f);
}

static const char *or_src =
    "Key: type = Up | Down | Left | Right\n"
    "axis: (k: Key) -> Int =\n"
    "    case k:\n"
    "        Up | Down -> 1\n"
    "        _ -> 2\n"
    "main: () -> _ =\n"
    "    v = axis(Left)\n"
    "    _\n";

static void or_pattern(void)
{
    lfx f;

    lfx_begin(&f, or_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the alternatives disjoin into one conjunct */
    EK_CHECK(lfx_has(&f, "t2 = t2 && (t0.as.ref->info == &ekT_test$Key$Up"
                         " || t0.as.ref->info == &ekT_test$Key$Down);"));
    lfx_end(&f);
}

static void binding_patterns(void)
{
    lfx f;

    lfx_begin(&f, "Holder: type = Holder [x: Int, y: Int]\n"
                  "get: (h: Holder) -> Int =\n"
                  "    case h:\n"
                  "        Holder {x, y: b} -> x + b\n"
                  "main: () -> _ =\n"
                  "    v = get(Holder {x: 1, y: 2})\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the bindings are owned keeps of the destructured slots, and the
       arm releases them before the match leaves */
    EK_CHECK(lfx_has(&f, "x = ek_keep(((ekS_rec0 *)"
                         "((ekS_test$Holder$Holder *)t0.as.ref)->p0.as.ref)"
                         "->f_x);"));
    EK_CHECK(lfx_has(&f, "b = ek_keep(((ekS_rec0 *)"
                         "((ekS_test$Holder$Holder *)t0.as.ref)->p0.as.ref)"
                         "->f_y);"));
    EK_CHECK(lfx_has(&f, "ek_release(b);\n                ek_release(x);\n"
                         "                break;"));
    lfx_end(&f);
}

static const char *list_src =
    "pick: (xs: []Int) -> Int =\n"
    "    case xs:\n"
    "        {h, ..t} -> 1\n"
    "        {} -> 2\n"
    "main: () -> _ =\n"
    "    v = pick({3})\n"
    "    _\n";

static void list_patterns(void)
{
    lfx f;

    lfx_begin(&f, list_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the head tests one cons cell, the rest binds the walked tail,
       and the empty arm is the exact-length test */
    EK_CHECK(lfx_has(&f, "t2 = t2 && ((t0.tag == EK_VAL_REF"
                         " && t0.as.ref != 0"
                         " && t0.as.ref->info->kind == EK_KIND_CONS));"));
    EK_CHECK(lfx_has(&f, "h = ek_keep(ek_cons_head(t0.as.ref));\n"
                         "                t = ek_keep(ek_cons_tail(t0.as.ref));"));
    EK_CHECK(lfx_has(&f, "t3 = t3 && (ek_val_is_nil(t0));"));
    lfx_end(&f);
}

static void prefix_rest_pattern(void)
{
    lfx f;

    lfx_begin(&f, "last_of: (xs: []Int) -> Int =\n"
                  "    case xs:\n"
                  "        {..rest, last} -> last\n"
                  "        {} -> 0\n"
                  "main: () -> _ =\n"
                  "    v = last_of({1, 2, 3})\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the walk counts the cells, then the second pass rebuilds the
       prefix while the fixed tail keeps */
    EK_CHECK(lfx_has(&f, "size_t t4 = 0;\n"
                         "            while ((t3.tag == EK_VAL_REF"
                         " && t3.as.ref != 0"
                         " && t3.as.ref->info->kind == EK_KIND_CONS)) {"));
    EK_CHECK(lfx_has(&f, "t2 = t2 && (t4 >= 1);"));
    EK_CHECK(lfx_has(&f, "last = ek_keep(t5);\n"
                         "                rest = ek_keep(t6);"));
    lfx_end(&f);
}

static void guarded_arm(void)
{
    lfx f;

    lfx_begin(&f, "sign: (n: Int) -> Int =\n"
                  "    case n:\n"
                  "        n if n < 0 -> 1\n"
                  "        _ -> 2\n"
                  "main: () -> _ =\n"
                  "    v = sign(0 - 4)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the guard evaluates under the arm's bindings and its Bool moves
       an int flag */
    EK_CHECK(lfx_has(&f, "n = ek_keep(t0);"));
    EK_CHECK(lfx_has(&f, "int t3 = t6.as.i;\n"
                         "                ek_release(t6);\n"
                         "                if (t3) {"));
    lfx_end(&f);
}

static const char *sum_src =
    "sum: (xs: []Int, acc: Int) -> Int =\n"
    "    case xs:\n"
    "        {} -> acc\n"
    "        {h, ..t} -> sum(t, acc + h)\n"
    "main: () -> _ =\n"
    "    v = sum({1, 2}, 0)\n"
    "    _\n";

static void tail_call_shape(void)
{
    lfx f;

    lfx_begin(&f, sum_src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the body reenters at the retry label */
    EK_CHECK(lfx_has(&f, "    ek_val acc = ek_keep(ek_p1);\n"
                         "retry: ;"));
    /* the arguments evaluate into temps before any old parameter
       releases, then each parameter takes its temp and the frame
       jumps */
    EK_CHECK(lfx_has(&f, "ek_val t5 = ek_keep(acc);\n"
                         "                ek_val t6 = ek_keep(h);\n"
                         "                ek_val t7 = ek_num_add(t5, t6);\n"
                         "                ek_release(t6);\n"
                         "                ek_release(t5);\n"
                         "                ek_release(t0);\n"
                         "                ek_release(t);\n"
                         "                ek_release(h);\n"
                         "                ek_release(xs);\n"
                         "                xs = t4;\n"
                         "                ek_release(acc);\n"
                         "                acc = t7;\n"
                         "                goto retry;"));
    lfx_end(&f);
}

static void no_tail_call_outside_tails(void)
{
    lfx f;

    /* the self call is a binding's right-hand side: an ordinary call */
    lfx_begin(&f, "down: (n: Int) -> Int =\n"
                  "    r = down(n - 1)\n"
                  "    r\n"
                  "main: () -> _ =\n"
                  "    _ := down(2)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_lacks(&f, "retry"));
    EK_CHECK(lfx_lacks(&f, "goto"));
    EK_CHECK(lfx_has(&f, "ek_val t1 = ekf_test$down(t0);"));
    lfx_end(&f);
}

static void closure_shape(void)
{
    lfx f;

    lfx_begin(&f, "twice: (f: (Int) -> Int, x: Int) -> Int = f(f(x))\n"
                  "main: () -> _ =\n"
                  "    base = 10\n"
                  "    h: (Int) -> Int = (x): x * base + 1\n"
                  "    v = twice(h, 2)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the environment struct keeps the capture and the closure is born
       over it; the closure body owns its parameter and reads the
       capture through the environment reference */
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; ek_val f_base; } "
                         "ekS_env0;"));
    EK_CHECK(lfx_has(&f, "ek_val t0 = ek_keep(base);\n"
                         "    ek_val t1 = ekn_env0(t0);\n"
                         "    ek_release(t0);\n"
                         "    ek_val t2 = ek_closure_new(ekl_0, t1);\n"
                         "    ek_release(t1);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekl_0(ek_val env, ek_val *args, "
                         "size_t argc)"));
    EK_CHECK(lfx_has(&f, "ek_val x = ek_keep(args[0]);"));
    EK_CHECK(lfx_has(&f, "ek_keep(((ekS_env0 *)env.as.ref)->f_base);"));
    /* a call on a parameter of function type goes through the ABI;
       the argument is the frame's own local, passed borrowed */
    EK_CHECK(lfx_has(&f, "ek_val t4[] = {ek_p1};"));
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_closure_call(t2, t4, 1);"));
    lfx_end(&f);
}

static void trampoline_shape(void)
{
    lfx f;

    lfx_begin(&f, "double: (x: Int) -> Int = x * 2\n"
                  "apply: (f: (Int) -> Int, x: Int) -> Int = f(x)\n"
                  "main: () -> _ =\n"
                  "    v = apply(double, 21)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK_U64(f.low.fn_count, 3);
    /* the function value is a fresh closure over the static
       trampoline, which forwards the argument slots */
    EK_CHECK(lfx_has(&f, "static ek_val ekt_test$double(ek_val env, "
                         "ek_val *args, size_t argc);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekt_test$double(ek_val env, "
                         "ek_val *args, size_t argc)\n{\n"
                         "    (void)env;\n    (void)argc;\n"
                         "    return ekf_test$double(args[0]);\n}"));
    EK_CHECK(lfx_has(&f, "ek_val t0 = ek_closure_new(ekt_test$double, "
                         "ek_val_unit());"));
    lfx_end(&f);
}

static void invert_shape(void)
{
    lfx f;

    lfx_begin(&f, "chain: (r: Result Int String) -> Result Int String =\n"
                  "    a <- Result.try(r)\n"
                  "    Ok a\n"
                  "main: () -> _ =\n"
                  "    v = chain(Ok 3)\n"
                  "    _ := v\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the continuation is the call's last argument, its parameters
       owned like any function's; the receiver is the frame's own
       local, passed borrowed; Result.try specializes over its
       applied variables t, e, u in first-occurrence order */
    EK_CHECK(lfx_has(&f, "ek_val t1 = ek_closure_new(ekl_0, t0);\n"
                         "    ek_release(t0);\n"
                         "    ek_frame_site(2, 10);\n"
                         "    ek_val t2 = ekf_std$Result$try$Int$"
                         "String$Int(ek_p0, t1);\n"
                         "    ek_release(t1);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekl_0(ek_val env, ek_val *args, "
                         "size_t argc)"));
    EK_CHECK(lfx_has(&f, "ek_val a = ek_keep(args[0]);"));
    lfx_end(&f);
}

/* The logical operators lower by their defining expansion, and the
   right operand is a tail position: a self call there reenters the
   function through the if-branch. */
static void logical_expansion(void)
{
    lfx f;

    lfx_begin(&f, "sign: (n: Int) -> Bool =\n"
                  "    n > 0 && n < 10\n"
                  "main: () -> _ =\n"
                  "    v = sign(4)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* `a && b` is `if a: b else: False`; the left operand is the
       condition, released after the branches */
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_val_unit();\n"
                         "    if (t2.as.i) {"));
    EK_CHECK(lfx_has(&f, "} else {\n"
                         "        t3 = ek_bool(0);\n"
                         "    }\n"
                         "    ek_release(t2);"));
    lfx_end(&f);
}

static void logical_or_expansion(void)
{
    lfx f;

    lfx_begin(&f, "pick: (b: Bool) -> Bool =\n"
                  "    b || b\n"
                  "main: () -> _ =\n"
                  "    v = pick(True)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* `a || b` is `if a: True else: b`; the left value dies with the
       condition since only the branch values carry the result, and
       the branch reads the borrowed parameter through its alias */
    EK_CHECK(lfx_has(&f, "if (t0.as.i) {\n"
                         "        t1 = ek_bool(1);\n"
                         "    } else {\n"
                         "        t1 = ek_keep(ek_p0);\n"
                         "    }\n"
                         "    ek_release(t0);"));
    lfx_end(&f);
}

static void tail_through_logic_shape(void)
{
    lfx f;

    lfx_begin(&f, "down: (n: Int) -> Bool =\n"
                  "    case n:\n"
                  "        0 -> True\n"
                  "        _ -> n > 0 && down(n - 1)\n"
                  "main: () -> _ =\n"
                  "    v = down(3)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the right operand lands in the if-branch tail, so the self call
       reassigns the owned parameter and jumps */
    EK_CHECK(lfx_has(&f, "if (t6.as.i) {\n"
                         "                    ek_val t8 = ek_keep(n);"));
    EK_CHECK(lfx_has(&f, "                    ek_release(n);\n"
                         "                    n = t10;\n"
                         "                    goto retry;"));
    EK_CHECK(lfx_has(&f, "} else {\n"
                         "                    t7 = ek_bool(0);\n"
                         "                }"));
    lfx_end(&f);
}

int main(void)
{
    cascade_shape();
    literal_patterns();
    string_pattern();
    or_pattern();
    binding_patterns();
    list_patterns();
    prefix_rest_pattern();
    guarded_arm();
    tail_call_shape();
    no_tail_call_outside_tails();
    closure_shape();
    trampoline_shape();
    invert_shape();
    logical_expansion();
    logical_or_expansion();
    tail_through_logic_shape();
    return ek_test_report("lower/case");
}
