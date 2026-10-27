#include "common.h"

#include <sys/stat.h>

/* The unused warning: references anywhere in the closure are uses,
   tests are roots, and parameters and `_` names do not participate.
   An import is used by the importing file's references to its target. */

#define MOD_DIR "build/tmp-resolve/unused"

static size_t code_count(const rfx *f, const char *code)
{
    size_t i, n = 0;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].code && strcmp(f->ds[i].code, code) == 0) {
            n++;
        }
    }
    return n;
}

static void put_fixtures(void)
{
    mkdir("build/tmp-resolve", 0755);
    mkdir(MOD_DIR, 0755);
    EK_CHECK(rfx_put(MOD_DIR, "empty.eko", "_pub: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "only.eko", "pub: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "sel.eko",
                     "pub: Int = 1\n"
                     "t: test = pub\n"));
    EK_CHECK(rfx_put(MOD_DIR, "inner.eko",
                     "a: Int = 1\n"
                     "_b: Int = a\n"));
    EK_CHECK(rfx_put(MOD_DIR, "geo.eko",
                     "Shape: type = Circle Float\n"
                     "area: (s: Shape) -> Float = 1.0\n"));
}

static void run_in(rfx *f, const char *src)
{
    const char *path[1] = {MOD_DIR};

    rfx_run(f, src, path, 1, "use.eko");
}

static void top_binding(void)
{
    rfx f;

    rfx_begin(&f, "helper: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 1));
    EK_CHECK(f.dn == 1 && f.ds[0].severity == EK_SEV_WARNING);
    EK_CHECK(f.dn == 1 && f.ds[0].message != NULL
             && strstr(f.ds[0].message, "`helper`") != NULL);
    rfx_end(&f);
}

static void test_root_is_a_use(void)
{
    rfx f;

    /* A test binding is a compilation root, so its references count
       even though the test itself never enters the surface. */
    rfx_begin(&f,
              "helper: Int = 1\n"
              "t: test = helper\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);
}

static void exemptions(void)
{
    rfx f;

    /* A leading `_` keeps a top-level binding out of the warning. */
    rfx_begin(&f, "_helper: Int = 1\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);

    /* Parameters do not participate. */
    rfx_begin(&f,
              "f: (ignored: Int) -> Int = 1\n"
              "_use: Int = f(2)\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);

    /* The entry point is a root: the host calls it. */
    rfx_begin(&f,
              "f: () -> Int = 1\n"
              "main: () -> Int = f()\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);
}

static void local_bindings(void)
{
    rfx f;

    rfx_begin(&f,
              "f: () -> Int =\n"
              "    x := 1\n"
              "    2\n"
              "_use: Int = f()\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 2));
    rfx_end(&f);

    rfx_begin(&f,
              "f: () -> Int =\n"
              "    x := 1\n"
              "    x\n"
              "_use: Int = f()\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);

    rfx_begin(&f,
              "f: () -> Int =\n"
              "    _x := 1\n"
              "    2\n"
              "_use: Int = f()\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);
}

static void local_shadowing(void)
{
    rfx f;

    /* The inner binding is used; the shadowed outer one is not. */
    rfx_begin(&f,
              "f: () -> Int =\n"
              "    x := 1\n"
              "    x := 2\n"
              "    x\n"
              "_use: Int = f()\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 2));
    rfx_end(&f);
}

static void unused_import(void)
{
    rfx f;

    put_fixtures();
    run_in(&f,
           "import empty\n"
           "_x: Int = 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 1));
    EK_CHECK(f.dn == 1 && f.ds[0].message != NULL
             && strstr(f.ds[0].message, "`empty`") != NULL);
    rfx_end(&f);
}

static void used_imports(void)
{
    rfx f;

    put_fixtures();
    /* A qualified member uses the import. */
    run_in(&f,
           "import only\n"
           "_use: Int = only.pub\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);

    /* An opened selective name uses it. */
    run_in(&f,
           "import only.{pub}\n"
           "_use: Int = pub\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);

    /* A receiver module segment uses it. */
    run_in(&f,
           "import geo\n"
           "_use: Float =\n"
           "    s: Shape = Circle 1.0\n"
           "    s.geo.area()\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);

    /* A test root uses it. */
    run_in(&f,
           "import only\n"
           "t: test = only.pub\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);
}

static void unused_selective(void)
{
    rfx f;

    /* The target's own test uses `pub`; the importing file does not. */
    put_fixtures();
    run_in(&f,
           "import sel.{pub}\n"
           "_x: Int = 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 1));
    rfx_end(&f);
}

static void target_internal_use(void)
{
    rfx f;

    /* `a` is used inside `inner`, but that is not a use of this
       file's import. */
    put_fixtures();
    run_in(&f,
           "import inner\n"
           "_x: Int = 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 1));
    rfx_end(&f);
}

static void explicit_std(void)
{
    rfx f;

    /* The explicit line is W-redundant-import, never W-unused. */
    put_fixtures();
    run_in(&f,
           "import std\n"
           "_x: Int = 0\n");
    EK_CHECK(rfx_has_code(&f, "W-redundant-import"));
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);
}

static void pattern_bindings(void)
{
    rfx f;

    /* A destructuring binding has one site per name. */
    rfx_begin(&f,
              "f: (p: [Int, Int]) -> Int =\n"
              "    {a, b} := p\n"
              "    a\n"
              "_use: Int = f({1, 2})\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 2));
    rfx_end(&f);

    /* The alternatives of an or-pattern are one binding; a use of the
       standing alternative uses it. */
    rfx_begin(&f,
              "T: type = A Int | B Int\n"
              "f: (t: T) -> Int =\n"
              "    case t:\n"
              "        A x | B x -> x\n"
              "_use: Int = f(A 1)\n");
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    rfx_end(&f);
}

int main(void)
{
    top_binding();
    test_root_is_a_use();
    exemptions();
    local_bindings();
    local_shadowing();
    unused_import();
    used_imports();
    unused_selective();
    target_internal_use();
    explicit_std();
    pattern_bindings();
    return ek_test_report("resolve/unused");
}
