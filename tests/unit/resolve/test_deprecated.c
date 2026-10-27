#include "common.h"

#include <sys/stat.h>

/* The deprecation warning: a use of a symbol whose declaration document
   carries the convention warns once at the use site. The declaration
   itself, an unmarked document, and a module that never uses the
   binding do not warn. */

#define MOD_DIR "build/tmp-resolve/deprecated"

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

static const ek_diagnostic *first_code(const rfx *f, const char *code)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].code && strcmp(f->ds[i].code, code) == 0) {
            return &f->ds[i];
        }
    }
    return NULL;
}

static void same_module_use(void)
{
    rfx f;
    const ek_diagnostic *d;

    rfx_begin(&f,
              "/// @deprecated\n"
              "old: Int = 1\n"
              "_use: Int = old\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    EK_CHECK_U64(code_count(&f, "W-unused"), 0);
    d = first_code(&f, "W-deprecated");
    EK_CHECK(d != NULL);
    if (d) {
        EK_CHECK_U64(d->severity, EK_SEV_WARNING);
        EK_CHECK_U64(d->line, 3);
        EK_CHECK_STR(d->message, "the binding `old` is deprecated");
    }
    rfx_end(&f);
}

static void unmarked_document(void)
{
    rfx f;

    rfx_begin(&f,
              "/// ordinary text\n"
              "old: Int = 1\n"
              "_use: Int = old\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 0);
    rfx_end(&f);
}

static void declaration_is_not_a_use(void)
{
    rfx f;

    rfx_begin(&f, "/// @deprecated\nold: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 0);
    EK_CHECK_U64(code_count(&f, "W-unused"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-unused", 2));
    rfx_end(&f);
}

static void repeated_uses(void)
{
    rfx f;

    rfx_begin(&f,
              "/// @deprecated\n"
              "old: Int = 1\n"
              "_a: Int = old\n"
              "_b: Int = old + old\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 3);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 3));
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 4));
    rfx_end(&f);
}

static void call_use(void)
{
    rfx f;

    /* The callee name and the call node carry the same symbol; the use
       site reports once. */
    rfx_begin(&f,
              "/// @deprecated\n"
              "old: (x: Int) -> Int = x\n"
              "_use: Int = old(1)\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 3));
    rfx_end(&f);
}

static void block_binding(void)
{
    rfx f;
    const ek_diagnostic *d;

    rfx_begin(&f,
              "f =\n"
              "    /// @deprecated\n"
              "    x = 1\n"
              "    x\n"
              "_use: Int = f\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    d = first_code(&f, "W-deprecated");
    EK_CHECK(d != NULL);
    if (d) {
        EK_CHECK_U64(d->line, 4);
        EK_CHECK_STR(d->message, "the binding `x` is deprecated");
    }
    rfx_end(&f);
}

static void namespace_member(void)
{
    rfx f;

    rfx_begin(&f,
              "Money: type = [cents: Int]\n"
              "/// @deprecated\n"
              "Money.describe: (m: Money) -> String = \"m\"\n"
              "_use: (m: Money) -> String = Money.describe\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 4));
    rfx_end(&f);
}

static void constructor_use(void)
{
    rfx f;

    /* The constructor's declaration is the type declaration, so the
       type's document marks its constructors too. */
    rfx_begin(&f,
              "/// @deprecated\n"
              "Color: type = Red | Green\n"
              "_use: Color = Red\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 2);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 3));
    rfx_end(&f);
}

static void warning_with_error(void)
{
    rfx f;

    rfx_begin(&f,
              "/// @deprecated\n"
              "old: Int = 1\n"
              "_use: Int = old + missing\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 3));
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 3));
    rfx_end(&f);
}

static void put_fixtures(void)
{
    mkdir("build/tmp-resolve", 0755);
    mkdir(MOD_DIR, 0755);
    EK_CHECK(rfx_put(MOD_DIR, "dep.eko",
                     "/// @deprecated\n"
                     "old: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "geo.eko",
                     "Shape: type = Circle Float\n"
                     "/// @deprecated\n"
                     "area: (s: Shape) -> Float = 1.0\n"));
}

static void across_import(void)
{
    rfx f;
    const char *path[1] = { MOD_DIR };

    put_fixtures();
    rfx_run(&f,
            "import dep\n"
            "_use: Int = dep.old\n", path, 1, "use.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 2));
    EK_CHECK(f.ds[0].file != NULL && strstr(f.ds[0].file, "use.eko"));
    rfx_end(&f);
}

static void receiver_module_path(void)
{
    rfx f;
    const char *path[1] = { MOD_DIR };

    put_fixtures();
    rfx_run(&f,
            "import geo\n"
            "_use: Float =\n"
            "    s: geo.Shape = Circle 1.0\n"
            "    s.geo.area()\n", path, 1, "geo_use.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK_U64(code_count(&f, "W-deprecated"), 1);
    EK_CHECK(rfx_code_at_line(&f, "W-deprecated", 4));
    rfx_end(&f);
}

int main(void)
{
    same_module_use();
    unmarked_document();
    declaration_is_not_a_use();
    repeated_uses();
    call_use();
    block_binding();
    namespace_member();
    constructor_use();
    warning_with_error();
    across_import();
    receiver_module_path();
    return ek_test_report("resolve/deprecated");
}
