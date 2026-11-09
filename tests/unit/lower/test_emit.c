#include "common.h"

/* The emitted text: reachability from `main`, the naming scheme, the
   operator mapping, and the ownership shape at call sites, bindings,
   and block ends. Every expectation is a fragment naming the emitted
   construct it asserts; a fragment spans several lines only where the
   adjacency itself is the point. */

/* The occurrences of a fragment in the emitted unit. */
static int text_count(const char *text, const char *frag)
{
    const char *p = text;
    int n = 0;

    while (p != NULL && (p = strstr(p, frag)) != NULL) {
        n++;
        p++;
    }
    return n;
}

/* True when the first fragment occurs before the second. */
static int str_before(const char *text, const char *a, const char *b)
{
    const char *pa = strstr(text, a);
    const char *pb = strstr(text, b);

    return pa != NULL && pb != NULL && pa < pb;
}

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
    /* the retention helper is inline, so a unit that never calls it
       strict-compiles clean */
    EK_CHECK(lfx_has(&f, "static inline ek_val ek_keep(ek_val v)"));
    EK_CHECK(lfx_has(&f, "ek_retain(v);"));
    /* every emitted function is declared first; the parameters are
       the borrowed ABI slots */
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$main(void);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$add2(ek_val ek_p0, "
                         "ek_val ek_p1);"));
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$_helper(ek_val ek_p0);"));
    /* the untouched std surface and the unreferenced binding are
       absent */
    EK_CHECK(lfx_lacks(&f, "ekf_test$unused"));
    EK_CHECK(lfx_lacks(&f, "ekf_std$"));
    EK_CHECK(lfx_has(&f, "int main(void)\n{\n"));
    EK_CHECK(lfx_has(&f, "ek_val t = ekf_test$main();"));
    EK_CHECK(lfx_has(&f, "ek_release(t);"));
    lfx_end(&f);
}

static void ownership(void)
{
    lfx f;

    lfx_begin(&f, reach_src);
    /* a name argument passes the frame's own local: the caller's
       frame keeps it alive across the call, so no keep is born and
       nothing releases after the call */
    EK_CHECK(lfx_has(&f, "ek_frame_site(3, 33);\n"
                         "    ek_val t0 = ekf_test$_helper(ek_p0);\n"
                         "    ek_frame_site(3, 46);\n"
                         "    ek_val t1 = ekf_test$_helper(ek_p1);"));
    EK_CHECK(lfx_lacks(&f, "ekf_test$_helper(t"));
    /* a parameter the body only reads stays borrowed: the prologue
       keeps nothing, the local aliases its ABI slot, and the scope
       close releases nothing */
    EK_CHECK(lfx_lacks(&f, "ek_val a = ek_keep(ek_p0);"));
    EK_CHECK(lfx_lacks(&f, "ek_release(ek_p1);"));
    /* an operand read still keeps its own copy and releases it once
       the result stands */
    EK_CHECK(lfx_has(&f, "ek_val t2 = ek_num_add(t0, t1);\n"
                         "    ek_release(t1);\n    ek_release(t0);"));
    /* a binding transfers; the function's own bindings release in
       reverse order before the return */
    EK_CHECK(lfx_has(&f, "ek_val v = t2;"));
    EK_CHECK(lfx_has(&f, "ek_release(v);\n    ek_frame_pop();\n    return t3;"));
    lfx_end(&f);
}

/* The argument shapes at one glance: a local argument travels
   borrowed, an owned temp argument keeps exactly its own release, and
   a borrowed field read keeps the slot's copy while the receiver
   itself travels borrowed. */
static void argument_shapes(void)
{
    lfx f;

    lfx_begin(&f, "_echo: (n: Int) -> Int = n\n"
                  "main: () -> _ =\n"
                  "    q = _echo(_echo(4))\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the inner call's result is an owned temp: it moves into the
       outer argument slot and dies with its own release */
    EK_CHECK(lfx_has(&f, "ek_val t1 = ekf_test$_echo(t0);\n"
                         "    ek_release(t0);\n"
                         "    ek_frame_site(3, 9);\n"
                         "    ek_val t2 = ekf_test$_echo(t1);\n"
                         "    ek_release(t1);"));
    lfx_end(&f);

    lfx_begin(&f, "Person: type = [name: String, age: Int]\n"
                  "_adult: (n: Int) -> Bool = n >= 18\n"
                  "main: () -> _ =\n"
                  "    p: Person = {name: \"ada\", age: 36}\n"
                  "    a = _adult(p.age)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the field read keeps the slot's own copy into the argument temp
       and the temp releases after the call; the receiver is the
       frame's own local, borrowed */
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_keep(((ekS_test$Person *)"
                         "p.as.ref)->f_age);\n"
                         "    ek_frame_site(5, 9);\n"
                         "    ek_val t4 = ekf_test$_adult(t3);\n"
                         "    ek_release(t3);"));
    EK_CHECK(lfx_lacks(&f, "ek_keep(p);"));
    lfx_end(&f);
}

/* The receiver slot of a member call: a receiver whose parameter is
   an anonymous record adapts -- the nominal object rebuilds field by
   field into the anonymous family, and the adapted value travels as
   an owned temp with its own release -- while a receiver whose
   parameter is its own type keeps the borrowed local read. */
static void receiver_shapes(void)
{
    lfx f;

    lfx_begin(&f, "Q: type = [b: Int, a: Int]\n"
                  "Q.geta: (p: [a: Int, b: Int]) -> Int = p.a\n"
                  "Q.getb: (p: Q) -> Int = p.b\n"
                  "main: () -> _ =\n"
                  "    q: Q = {b: 2, a: 1}\n"
                  "    x = q.geta()\n"
                  "    y = q.getb()\n"
                  "    d = 1 / (x - y)\n"
                  "    _ := d\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the adapting receiver is an owned read of the frame local, the
       rebuild names the anonymous family, and the adapted value
       releases with the call */
    EK_CHECK(lfx_has(&f, "ek_val t3 = ek_keep(q);"));
    EK_CHECK(lfx_has(&f, "ek_val t4 = ek_keep(((ekS_test$Q *)"
                         "t3.as.ref)->f_a);"));
    EK_CHECK(lfx_has(&f, "ek_val t6 = ekn_rec0(t4, t5);"));
    EK_CHECK(lfx_has(&f, "ek_val t7 = ekf_test$Q$geta(t6);\n"
                         "    ek_release(t6);"));
    EK_CHECK(lfx_has(&f, "ek_release(t3);"));
    /* the non-adapting receiver borrows the frame local: the call
       passes the local's own name, no keep and no release around it */
    EK_CHECK(lfx_has(&f, "ek_val t8 = ekf_test$Q$getb(q);"));
    EK_CHECK(lfx_lacks(&f, "ekf_test$Q$getb(ek_keep(q))"));
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
       newline and the control byte become octal escapes; the escaped
       literal stands in the shim's priming line */
    EK_CHECK(lfx_has(&f, "ekc0 = ek_val_ref(ek_str_from_cstr("
                         "\"a\\\"b\\\\c\\012d\\001e\\?\"));"));
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

    lfx_begin(&f, "_twice: (x: Int, _: Int) -> Int =\n"
                  "    x := x + 1\n"
                  "    x\n"
                  "main: () -> _ =\n"
                  "    q = _twice(1, 2)\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the written name stands where the body needs the owned copy:
       a rebind releases the alias's own value and moves the temp in;
       the unnamed parameter is numbered and consumed unread */
    EK_CHECK(lfx_has(&f, "static ek_val ekf_test$_twice(ek_val ek_p0, "
                         "ek_val ek_p1);"));
    EK_CHECK(lfx_has(&f, "ek_val x = ek_keep(ek_p0);"));
    EK_CHECK(lfx_has(&f, "(void)ek_p1;"));
    EK_CHECK(lfx_has(&f, "ek_release(x);\n    x = t2;"));
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

/* The manifest scopes the runtime-shared families: an Out manifest
   defines no shared identity (the Out bundle stays runtime-typed and
   the generated unit only reads its record layout), an In manifest
   defines the Result, Option, and IoError identities its reader's
   builders name, a Clock manifest defines only the time pair, and the
   stat and mode families stay with the file system. A field-less
   family stands over the NULL drop, with no drop function. */
static void manifest_scopes_families(void)
{
    lfx f;

    lfx_begin(&f, "main: (out: Out) ! = out.println(\"hello\")\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_lacks(&f, "ekT_std$"));
    EK_CHECK(lfx_lacks(&f, "ekT_sys$"));
    EK_CHECK(lfx_lacks(&f, "ekT_time$"));
    EK_CHECK(lfx_lacks(&f, "drop_"));
    lfx_end(&f);

    lfx_begin(&f, "main: (i: In) ! =\n"
                  "    case i.read_line():\n"
                  "        Ok _ -> _\n"
                  "        Err _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekT_std$Result$Ok = { EK_KIND_STRUCT, "
                         "drop_std$Result$Ok, NULL, NULL };"));
    EK_CHECK(lfx_has(&f, "ekT_std$Option$None = { EK_KIND_STRUCT, "
                         "NULL, NULL, NULL };"));
    EK_CHECK(lfx_has(&f, "ekT_sys$IoError$Other = { EK_KIND_STRUCT, "
                         "drop_sys$IoError$Other, NULL, NULL };"));
    EK_CHECK(lfx_lacks(&f, "ekT_sys$FileInfo"));
    EK_CHECK(lfx_lacks(&f, "ekT_time$"));
    EK_CHECK(lfx_lacks(&f, "ekT_sys$OpenMode$"));
    EK_CHECK(lfx_lacks(&f, "drop_std$Option$None"));
    lfx_end(&f);

    lfx_begin(&f, "main: (c: Clock, out: Out) ! ="
                  " out.println(c.now().to_ns().show())\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_time$Mono$Mono = "));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_time$Time$Time = "));
    EK_CHECK(lfx_lacks(&f, "ekT_std$"));
    EK_CHECK(lfx_lacks(&f, "ekT_sys$"));
    lfx_end(&f);
}

/* The bundle records carry the runtime's published slot order as
   compile-time checks: one per field, one for the slot count. */
static void bundle_layout_checks(void)
{
    lfx f;

    lfx_begin(&f, "main: (out: Out) ! = out.println(\"hello\")\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "typedef char ekB_cap$_OutB$write[(offsetof("
                         "ekS_cap$_OutB, f_write) == sizeof(ek_obj) + "
                         "EK_OUT_WRITE * sizeof(ek_val)) ? 1 : -1];"));
    EK_CHECK(lfx_has(&f, "typedef char ekB_cap$_OutB$slots[(sizeof("
                         "ekS_cap$_OutB) == sizeof(ek_obj) + EK_OUT_SLOTS "
                         "* sizeof(ek_val)) ? 1 : -1];"));
    lfx_end(&f);
}

/* The two embedded modules keep their own stems in the unit: the
   vocabulary families (IoError, OpenMode, FileInfo) emit under sys, the
   bundle records the pairs unwrap to under cap, and nothing crosses. */
static void vocabulary_and_channel_stems(void)
{
    lfx f;

    lfx_begin(&f, "main: (fs: Fs, out: Out) ! =\n"
                  "    case fs.read(\"x\"):\n"
                  "        Ok s -> out.println(s)\n"
                  "        Err NotFound -> out.println(\"none\")\n"
                  "        Err _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_sys$IoError$NotFound = "));
    EK_CHECK(lfx_has(&f, "} ekS_sys$FileInfo;"));
    EK_CHECK(lfx_has(&f, "ekB_sys$FileInfo$mtime"));
    EK_CHECK(lfx_has(&f, "} ekS_cap$_OutB;"));
    EK_CHECK(lfx_lacks(&f, "ekT_cap$IoError"));
    EK_CHECK(lfx_lacks(&f, "ekS_sys$_OutB"));
    lfx_end(&f);
}

/* A test root's derived constructors can build values over any
   capability row (no manifest bounds them), so every family registers
   and the test binary accepts the size. */
static void test_roots_stamp_all(void)
{
    lfx f;

    lfx_begin_tests(&f, "reads: test =\n"
                        "    i = In.from_lines({\"one\"})\n"
                        "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "const ek_typeinfo ekT_sys$OpenMode$Read = "));
    lfx_end(&f);
}

/* The Option-wrapping read: the Ok arm binds the inner constructor to
   its own named temp and releases it once the outer Result stands, in
   both the Some and the None arm, so the wrapping leaks no reference
   per call. */
static void read_line_ok_arm(void)
{
    lfx f;

    lfx_begin(&f, "main: (fs: Fs) ! =\n"
                  "    f := Fs.open(fs, \"lines.txt\", Read).unwrap()\n"
                  "    case f.read_line():\n"
                  "        Ok _ -> _\n"
                  "        Err _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the Some wrapper is a named temp of its own, consumed and
       released beside the payload temp */
    EK_CHECK(lfx_has(&f, "ek_val t10 = ekn_std$Option$Some(t9);\n"
                         "            t5 = ekn_std$Result$Ok(t10);\n"
                         "            ek_release(t10);\n"
                         "            ek_release(t9);"));
    /* the zero-payload None arm releases its constructor result the
       same way */
    EK_CHECK(lfx_has(&f, "ek_val t10 = ekn_std$Option$None();\n"
                         "            t5 = ekn_std$Result$Ok(t10);\n"
                         "            ek_release(t10);"));
    lfx_end(&f);
}

/* The interned string literals: identical byte sequences share one
   cache variable, the shim primes it before the entry dispatch, and a
   use site keeps the cache instead of building a fresh object. */
static void string_interning(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    a = \"ada\"\n"
                  "    b = \"ada\"\n"
                  "    c = \"byron\"\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* one cache variable per distinct literal, in first-use order */
    EK_CHECK(lfx_has(&f, "static ek_val ekc0;\n"
                         "static ek_val ekc1;\n"));
    EK_CHECK(lfx_lacks(&f, "ekc2"));
    /* the shim primes each variable once; the repeated \"ada\" adds
       no second builder call, so every ek_str_from_cstr in the unit
       stands in the priming */
    EK_CHECK(lfx_has(&f, "ekc0 = ek_val_ref(ek_str_from_cstr(\"ada\"));"));
    EK_CHECK(lfx_has(&f, "ekc1 = ek_val_ref(ek_str_from_cstr("
                         "\"byron\"));"));
    EK_CHECK_U64((unsigned long long)text_count(f.low.text,
                                                "ek_str_from_cstr"), 2);
    /* the priming precedes the entry dispatch */
    EK_CHECK(str_before(f.low.text, "ekc0 = ek_val_ref",
                        "ekf_test$main();"));
    /* a use site keeps the cache */
    EK_CHECK(lfx_has(&f, "ek_keep(ekc0);"));
    EK_CHECK(lfx_has(&f, "ek_keep(ekc1);"));
    lfx_end(&f);
}

/* The cache prefix is reserved: a written binding spelled like a
   literal-cache variable renames away, so the literal use inside the
   same body still reads the file-scope cache. */
static void cache_prefix_reserved(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    ekc0 = \"ada\"\n"
                  "    _ := ekc0.length()\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the cache variable keeps its own name and its priming */
    EK_CHECK(lfx_has(&f, "static ek_val ekc0;\n"));
    EK_CHECK(lfx_has(&f, "ekc0 = ek_val_ref(ek_str_from_cstr(\"ada\"));"));
    /* the written binding takes the u_ prefix instead, and the body's
       own use reads the renamed binding */
    EK_CHECK(lfx_has(&f, "ek_val u_ekc0 = ek_keep(ekc0);"));
    EK_CHECK(lfx_has(&f, "ek_keep(u_ekc0);"));
    EK_CHECK(lfx_lacks(&f, "ek_val ekc0 ="));
    lfx_end(&f);
}

/* A written binding in any generated family's shape is emitted under
   the u_ prefix, so it can never shadow the family's own symbols. */
static void family_prefixes_reserved(void)
{
    lfx f;

    lfx_begin(&f, "main: () -> _ =\n"
                  "    ekq_show = 3\n"
                  "    ekt_1 = \"s\"\n"
                  "    _ := ekq_show + ekt_1.length()\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_val u_ekq_show = "));
    EK_CHECK(lfx_has(&f, "ek_val u_ekt_1 = "));
    EK_CHECK(lfx_lacks(&f, "ek_val ekq_show ="));
    lfx_end(&f);
}

/* Member spellings that would share one underscore join keep distinct
   names: the pieces of a generated name are separated by `$`, which no
   written identifier supplies, so an underscored member can never
   merge with its owner's piece. */
static void mangled_pieces_disambiguate(void)
{
    lfx f;

    lfx_begin(&f, "Pq: type = [x: Int]\n"
                  "Pq.q_f: (p: Pq) -> Int = p.x\n"
                  "P: type = [y: Int]\n"
                  "P.f: (p: P) -> Int = p.y\n"
                  "main: () -> _ =\n"
                  "    d = 1 / (Pq.q_f({x: 1}) - P.f({y: 2}))\n"
                  "    _ := d\n"
                  "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekf_test$Pq$q_f"));
    EK_CHECK(lfx_has(&f, "ekf_test$P$f"));
    EK_CHECK(lfx_lacks(&f, "ekf_test_P_"));
    lfx_end(&f);
}

/* The file-read rows hand the length to the runtime as the raw int64
   word its signature reads. The unit harness sees emitted text only,
   so the signature-level fact is covered by the call's emitted
   argument fragment: the value slot spelled .as.i over the out-slot
   pointers. */
static void file_read_rows(void)
{
    lfx f;

    lfx_begin(&f, "main: (fs: Fs) ! =\n"
                  "    case fs.open(\"probe.txt\", Read):\n"
                  "        Ok h ->\n"
                  "            case h.read(3):\n"
                  "                Ok s -> _\n"
                  "                Err _ -> _\n"
                  "        Err _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_file_read(t"));
    EK_CHECK(lfx_has(&f, ".as.i, &"));
    lfx_end(&f);

    lfx_begin(&f, "main: (fs: Fs) ! =\n"
                  "    case fs.open(\"probe.txt\", Read):\n"
                  "        Ok h ->\n"
                  "            case h.read_bytes(4):\n"
                  "                Ok b -> _\n"
                  "                Err _ -> _\n"
                  "        Err _ -> _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_file_read_bytes(t"));
    EK_CHECK(lfx_has(&f, ".as.i, &"));
    lfx_end(&f);
}

int main(void)
{
    reachability();
    ownership();
    argument_shapes();
    receiver_shapes();
    operators();
    if_shape();
    literals();
    string_escaping();
    shadowing();
    family_prefixes_reserved();
    mangled_pieces_disambiguate();
    param_naming();
    no_main_no_functions();
    manifest_scopes_families();
    vocabulary_and_channel_stems();
    bundle_layout_checks();
    test_roots_stamp_all();
    read_line_ok_arm();
    string_interning();
    cache_prefix_reserved();
    file_read_rows();
    return ek_test_report("lower/emit");
}
