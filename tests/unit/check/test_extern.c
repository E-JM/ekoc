#include "common.h"

/* The native interface at compile time: the negative conformance
   entries N21-N24 at their exact codes, and the positive flows -- pure
   and world-touching externs, callback parameters, the marshalling
   whitelist in both directions, and the ctype field predicate. */

/* N21 -- extern naming a reserved `eko_` symbol. */
static void n21(void)
{
    cfx f;

    cfx_begin(&f, "_wrap: (_OutB) -> Out = extern \"eko_out_wrap\" Pure\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-reserved-symbol", 1));
    cfx_end(&f);
}

/* N22 -- ctype field outside machine words. */
static void n22(void)
{
    cfx f;

    cfx_begin(&f, "Stat: ctype = [size: I64, label: String]\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-ctype-field", 1));
    cfx_end(&f);
}

/* N23 -- extern parameter outside the marshalling whitelist. */
static void n23(void)
{
    cfx f;

    cfx_begin(&f, "_sum: (xs: []I64) !-> I64 = extern \"sum64\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-type", 1));
    cfx_end(&f);
}

/* N24 -- type variable in an extern signature. */
static void n24(void)
{
    cfx f;

    cfx_begin(&f, "_id: (x: t) !-> t = extern \"id_any\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-type", 1));
    cfx_end(&f);
}

/* A pure extern over whitelisted words, and a world-touching one with
   a String parameter and a callback. The extern tail stays on the
   declaration line. */
static void extern_forms(void)
{
    cfx f;

    cfx_begin(&f,
              "_sqrt: (x: F64) -> F64 = extern \"sqrt\" Pure\n"
              "_crc: (crc: U32, buf: Bytes, len: U64) -> U32 ="
              " extern \"crc32\" Pure\n"
              "_qs: (base: C.Ptr.Void, n: C.Size,"
              " cmp: (C.Ptr.Void, C.Ptr.Void) -> Int) ! ="
              " extern \"qsort\"\n");
    EK_CHECK(cfx_errors(&f) == 0);
    cfx_end(&f);
}

/* A ctype result and a unit result are whitelisted; a capability and
   a Char are not. */
static void whitelist_results(void)
{
    cfx f;

    cfx_begin(&f,
              "DivC: ctype = [quot: I32, rem: I32]\n"
              "_div: (n: I32, d: I32) -> DivC = extern \"div\" Pure\n"
              "_beep: () ! = extern \"beep\"\n");
    EK_CHECK(cfx_errors(&f) == 0);
    cfx_end(&f);

    cfx_begin(&f, "_get: () !-> Out = extern \"get_out\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-type", 1));
    cfx_end(&f);

    cfx_begin(&f, "_ord: (c: Char) -> U32 = extern \"ord\" Pure\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-type", 1));
    cfx_end(&f);
}

/* `Bytes` and a closure are parameter-only; `_` is result-only. */
static void whitelist_directions(void)
{
    cfx f;

    cfx_begin(&f, "_unbox: (n: I64) !-> Bytes = extern \"unbox\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-type", 1));
    cfx_end(&f);

    cfx_begin(&f,
              "_take: (f: (Int) -> Int) -> (Int) -> Int ="
              " extern \"take_fn\" Pure\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-type", 1));
    cfx_end(&f);
}

/* The arrow rule: `->` needs `Pure`; an open arrow is not admitted
   (the violation has no published code). */
static void arrow_rules(void)
{
    cfx f;

    cfx_begin(&f, "_f: (n: Int) -> Int = extern \"f\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-arrow", 1));
    cfx_end(&f);

    cfx_begin(&f, "_g: (f: (Int) ?-> Int) !-> Int = extern \"g\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-extern-arrow", 1));
    cfx_end(&f);
}

/* An ordinary record field is rejected; a nested ctype field
   passes. */
static void ctype_fields(void)
{
    cfx f;

    cfx_begin(&f,
              "Pair: ctype = [a: I64, b: I64]\n"
              "Outer: ctype = [p: Pair, tag: U8]\n");
    EK_CHECK(cfx_errors(&f) == 0);
    cfx_end(&f);

    cfx_begin(&f, "Cap: ctype = [fs: Fs]\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-ctype-field", 1));
    cfx_end(&f);

    cfx_begin(&f, "Bad: ctype = [n: Int]\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-ctype-field", 1));
    cfx_end(&f);
}

int main(void)
{
    n21();
    n22();
    n23();
    n24();
    extern_forms();
    whitelist_results();
    whitelist_directions();
    arrow_rules();
    ctype_fields();
    return ek_test_report("check/extern");
}
