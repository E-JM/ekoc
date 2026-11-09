#include "common.h"

/* The text suites of the native interface: the prototype each
   signature shape emits, the marshalling a call site renders per the
   whitelist, the ctype structs in declaration order, and the callback
   adapter shape. The executable gates with real shims live in
   run_extern.c. */

/* The F64 round trip: the exact-width cast out and the domain word
   back. */
static void scalar_prototype_and_marshal(void)
{
    lfx f;

    lfx_begin(&f,
              "_sqrt: (x: F64) -> F64 = extern \"shim_sqrt\" Pure\n"
              "main: () -> _ =\n"
              "    r = _sqrt(2.25.to_f64())\n"
              "    rf = Float.from_f64(r)\n"
              "    d = 1.0 / (rf - 1.5)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern double shim_sqrt(double);"));
    EK_CHECK(lfx_has(&f, "shim_sqrt((double)(t1).as.f)"));
    EK_CHECK(lfx_has(&f, "= ek_val_float((double)(shim_sqrt"));
    EK_CHECK(lfx_lacks(&f, "ek_closure_call("));
    lfx_end(&f);
}

/* The integer word and the domain words both marshal by slot. */
static void word_and_domain_marshal(void)
{
    lfx f;

    lfx_begin(&f,
              "_one: () -> I32 = extern \"shim_one\" Pure\n"
              "_echo: (x: I32) -> I32 = extern \"shim_echo\" Pure\n"
              "main: () -> _ =\n"
              "    r = _echo(_one())\n"
              "    rf = Int.from_i32(r)\n"
              "    d = 1 / (rf - 2)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern int32_t shim_echo(int32_t);"));
    EK_CHECK(lfx_has(&f, "extern int32_t shim_one(void);"));
    EK_CHECK(lfx_has(&f, "shim_echo((int32_t)(t0).as.i)"));
    EK_CHECK(lfx_has(&f, "= ek_val_int((int64_t)(shim_echo"));
    lfx_end(&f);
}

/* A String parameter reads the bytes, and a String result copies
   fresh with the UTF-8 claim checked. */
static void string_marshal(void)
{
    lfx f;

    lfx_begin(&f,
              "_shout: (s: String) !-> String = extern \"shim_shout\"\n"
              "main: () ! =\n"
              "    r = _shout(\"abc\")\n"
              "    w = 0\n"
              "    if r == \"ABC\":\n"
              "        w := 1\n"
              "        _\n"
              "    d = 1 / w\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern const char * shim_shout(const char *);"));
    EK_CHECK(lfx_has(&f, "shim_shout(ek_str_cstr((t0).as.ref))"));
    EK_CHECK(lfx_has(&f, "ek_str_from_cstr(shim_shout("));
    EK_CHECK(lfx_has(&f, "ek_utf8_valid(ek_str_cstr("));
    EK_CHECK(lfx_has(&f, "ek_panic(\"P-panic\", \"a String result is not "
                         "UTF-8\");"));
    EK_CHECK(lfx_has(&f, "= ek_val_ref("));
    lfx_end(&f);
}

/* A Bytes parameter passes the payload pointer read-only; a `_`
   result lowers as the bare statement call. */
static void bytes_param_and_unit_result(void)
{
    lfx f;

    lfx_begin(&f,
              "_two: () -> U64 = extern \"shim_two\" Pure\n"
              "_sum: (buf: Bytes, len: U64) !-> _ = extern \"shim_sum\"\n"
              "main: () ! =\n"
              "    _sum(\"ab\".to_bytes(), _two())\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern uint64_t shim_two(void);"));
    EK_CHECK(lfx_has(&f, "extern void shim_sum(const void *, uint64_t);"));
    EK_CHECK(lfx_has(&f, "shim_sum(ek_bytes_data((t1).as.ref), "
                         "(uint64_t)(t2).as.u);"));
    EK_CHECK(lfx_lacks(&f, "ek_val t2 = shim_sum"));
    lfx_end(&f);
}

/* The opaque pointer crosses both directions through its runtime
   wrapper. */
static void ptr_marshal(void)
{
    lfx f;

    lfx_begin(&f,
              "_box: () -> C.Ptr.Void = extern \"shim_box\" Pure\n"
              "_peek: (p: C.Ptr.Void) -> C.Size = extern \"shim_peek\" "
              "Pure\n"
              "main: () -> _ =\n"
              "    p = _box()\n"
              "    n = _peek(p)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern void * shim_box(void);"));
    EK_CHECK(lfx_has(&f, "extern size_t shim_peek(void *);"));
    EK_CHECK(lfx_has(&f, "= ek_ptr_wrap(shim_box())"));
    EK_CHECK(lfx_has(&f, "shim_peek(ek_ptr_unwrap(t1))"));
    EK_CHECK(lfx_has(&f, "= ek_val_uint((uint64_t)(shim_peek"));
    lfx_end(&f);
}

/* The ctype struct mirrors the declaration's fields, in declaration
   order, at their exact widths, and both prototypes name it before
   the value family appears. */
static void ctype_layout_and_marshal(void)
{
    lfx f;

    lfx_begin(&f,
              "Stat: ctype = [ok: I32, size: I64, mode: U32]\n"
              "_bump: (p: Stat) !-> Stat = extern \"shim_bump\"\n"
              "main: () ! =\n"
              "    p: Stat = {ok: 1, size: 2, mode: 3}\n"
              "    q = _bump(p)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "typedef struct { int32_t f_ok; int64_t f_size; "
                         "uint32_t f_mode; } ekC_test$Stat;"));
    EK_CHECK(lfx_has(&f, "extern ekC_test$Stat shim_bump(ekC_test$Stat);"));
    /* the C struct stands before the prototype that names it */
    EK_CHECK(f.low.text
             && (size_t)(strstr(f.low.text, "typedef struct { int32_t "
                                            "f_ok")
                         - f.low.text)
                    < (size_t)(strstr(f.low.text, "extern ekC_test$Stat")
                               - f.low.text));
    /* the value family keeps the declaration order behind the header */
    EK_CHECK(lfx_has(&f, "typedef struct { ek_obj obj; ek_val f_ok; ek_val "
                         "f_size; ek_val f_mode; } ekS_test$Stat;"));
    /* the call packs by value and rebuilds through the helper */
    EK_CHECK(lfx_has(&f, "ekC_test$Stat t5 = (ekC_test$Stat){"));
    EK_CHECK(lfx_has(&f, ".f_ok = (int32_t)("));
    EK_CHECK(lfx_has(&f, ".f_mode = (uint32_t)("));
    EK_CHECK(lfx_has(&f, "= shim_bump(t5);"));
    EK_CHECK(lfx_has(&f, "= ekn_test$Stat$rec(ek_val_int((int64_t)((t6).f_"
                         "ok)), ek_val_int((int64_t)((t6).f_size)), "
                         "ek_val_uint((uint64_t)((t6).f_mode)));"));
    lfx_end(&f);
}

/* A nested ctype field packs into its own C struct. */
static void nested_ctype(void)
{
    lfx f;

    lfx_begin(&f,
              "Inner: ctype = [x: I16]\n"
              "Outer: ctype = [head: I8, inner: Inner]\n"
              "_id: (o: Outer) -> Outer = extern \"shim_oid\" Pure\n"
              "main: () -> _ =\n"
              "    o: Outer = {head: 1, inner: Inner {x: 2}}\n"
              "    r = _id(o)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "typedef struct { int16_t f_x; } ekC_test$Inner;"));
    EK_CHECK(lfx_has(&f, "typedef struct { int8_t f_head; ekC_test$Inner "
                         "f_inner; } ekC_test$Outer;"));
    EK_CHECK(lfx_has(&f, ".f_inner = (ekC_test$Inner){.f_x = "
                         "(int16_t)("));
    lfx_end(&f);
}

/* The copy-in borrow passes the address of the packed temporary. */
static void c_in_address(void)
{
    lfx f;

    lfx_begin(&f,
              "Pair: ctype = [a: I32, b: I32]\n"
              "_peek: (p: C.In Pair) -> I32 = extern \"shim_peek\" Pure\n"
              "main: () -> _ =\n"
              "    p: Pair = {a: 7, b: 8}\n"
              "    r = _peek(p)\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern int32_t shim_peek(const ekC_test$Pair *);"));
    EK_CHECK(lfx_has(&f, "ekC_test$Pair t4 = (ekC_test$Pair){"));
    EK_CHECK(lfx_has(&f, "shim_peek(&t4)"));
    lfx_end(&f);
}

/* The closure parameter marshals as the function pointer and context
   pair, and the adapter converts through the uniform call ABI. */
static void callback_adapter(void)
{
    lfx f;

    lfx_begin(&f,
              "_run: (f: (I32) -> I32, n: C.Size) -> I32 = extern "
              "\"shim_run\" Pure\n"
              "main: () -> _ =\n"
              "    r = _run((x): x, 1)\n"
              "    rf = Int.from_i32(r)\n"
              "    d = 1 / (rf - 41)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern int32_t shim_run(int32_t (*)("
                         "int32_t, void *), void *, size_t);"));
    /* the context struct carries the closure and the kept slot */
    EK_CHECK(lfx_has(&f, "typedef struct { ek_val clo; ek_val keep; } "
                         "eko_cb_0;"));
    /* the adapter is the target-callable function over the uniform
       call ABI */
    EK_CHECK(lfx_has(&f, "static int32_t eko_cbr_0(int32_t a0, void *ctx)"));
    EK_CHECK(lfx_has(&f, "ek_val r = ek_closure_call(((eko_cb_0 *)ctx)"
                         "->clo, args, 1);"));
    EK_CHECK(lfx_has(&f, "int32_t out = (int32_t)(r).as.i;"));
    /* the call site passes the pair; the closure moves into the
       context, so its release rides the context's slots */
    EK_CHECK(lfx_has(&f, "eko_cb_0 t3;"));
    EK_CHECK(lfx_has(&f, "t3.clo = t1;"));
    EK_CHECK(lfx_has(&f, "t3.keep = ek_val_unit();"));
    EK_CHECK(lfx_has(&f, "shim_run(eko_cbr_0, (void *)&t3, "
                         "(size_t)(t2).as.u)"));
    EK_CHECK(lfx_has(&f, "ek_release(t3.keep);"));
    EK_CHECK(lfx_has(&f, "ek_release(t3.clo);"));
    lfx_end(&f);
}

/* A zero-parameter callback passes no argument array. */
static void callback_no_params(void)
{
    lfx f;

    lfx_begin(&f,
              "_run: (f: () -> Int) -> Int = extern \"shim_tick\" Pure\n"
              "main: () -> _ =\n"
              "    r = _run((): 41)\n"
              "    d = 1 / (r - 41)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "extern int64_t shim_tick(int64_t (*)("
                         "void *), void *);"));
    EK_CHECK(lfx_has(&f, "static int64_t eko_cbr_0(void *ctx)"));
    EK_CHECK(lfx_has(&f, "ek_closure_call(((eko_cb_0 *)ctx)->clo, "
                         "(ek_val *)0, 0);"));
    EK_CHECK(lfx_has(&f, "shim_tick(eko_cbr_0, (void *)&"));
    lfx_end(&f);
}

/* The namespace member reads the C thread-local errno through the
   runtime helper. */
static void c_get_errno(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    e = C.get_errno()\n"
              "    d = 1 / (Int.from_i32(e) - 0)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "= ek_val_int(ek_get_errno());"));
    EK_CHECK(lfx_lacks(&f, "extern"));
    lfx_end(&f);
}

/* A machine word outside the whitelist's C shapes keeps the unit
   balance: the arguments still evaluate and die. */
static void unknown_member_is_diagnosed(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    e = C.no_such_thing()\n"
              "    _ := e\n"
              "    _\n");
    EK_CHECK_U64(lfx_errors(&f), 1);
    EK_CHECK(lfx_has_code(&f, "E-lower"));
    lfx_end(&f);
}

/* The prototype and the adapter register once per declaration and
   parameter, however many call sites repeat them. */
static void registration_dedup(void)
{
    lfx f;

    lfx_begin(&f,
              "_run: (f: (I32) -> I32, n: C.Size) -> I32 = extern "
              "\"shim_run\" Pure\n"
              "main: () -> _ =\n"
              "    a = _run((x): x, 1)\n"
              "    b = _run((x): x, 2)\n"
              "    r = Int.from_i32(a)\n"
              "    q = Int.from_i32(b)\n"
              "    d = 1 / (2 - q + r)\n"
              "    _ := d\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    {
        const char *p = f.low.text;
        int protos = 0, adapters = 0, typedefs = 0;

        while (p && *p) {
            if ((p = strstr(p, "extern int32_t shim_run("))) {
                protos++;
                p++;
            }
        }
        p = f.low.text;
        while (p && *p) {
            if ((p = strstr(p, "static int32_t eko_cbr_0(int32_t a0"))) {
                adapters++;
                p++;
            }
        }
        p = f.low.text;
        while (p && *p) {
            if ((p = strstr(p, "typedef struct { ek_val clo;"))) {
                typedefs++;
                p++;
            }
        }
        EK_CHECK_U64((uint64_t)protos, 1);
        EK_CHECK_U64((uint64_t)adapters, 2); /* forward + body */
        EK_CHECK_U64((uint64_t)typedefs, 1);
    }
    lfx_end(&f);
}

int main(void)
{
    registration_dedup();
    scalar_prototype_and_marshal();
    word_and_domain_marshal();
    string_marshal();
    bytes_param_and_unit_result();
    ptr_marshal();
    ctype_layout_and_marshal();
    nested_ctype();
    c_in_address();
    callback_adapter();
    callback_no_params();
    c_get_errno();
    unknown_member_is_diagnosed();
    return ek_test_report("lower/test_extern");
}
