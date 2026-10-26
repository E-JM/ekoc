#include "common.h"

/* Selective imports: a listed free name enters the file's top layer,
   collisions are judged at the declaration point, and a local binding
   shadows the layer. The fixture module is the conformance suite's
   geolib.eko on the module path. */

#define CONF_DIR "tests/conformance"

static void run_in(rfx *f, const char *src, const char *name)
{
    const char *path[1] = { CONF_DIR };

    rfx_run(f, src, path, 1, name);
}

/* A listed free function answers a bare call at the top layer. */
static void opened_lookup(void)
{
    rfx f;
    const ek_ref *r;

    run_in(&f,
        "import geolib.{area}\n"
        "\n"
        "f: (s: Shape) -> Float = area(s)\n", "open1.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named_kind(&f, 3, "area", EK_SYM_FUNC);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_HEAD_TOP);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "geolib");
    rfx_end(&f);
}

/* An opened name and a file top-level binding share the layer. */
static void opened_vs_top(void)
{
    rfx f;

    run_in(&f,
        "import geolib.{area}\n"
        "\n"
        "area: (s: Shape) -> Float = 1.0\n", "open2.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 3));
    rfx_end(&f);
}

/* A repeated name inside one list is a conflict at the import. */
static void duplicate_in_list(void)
{
    rfx f;

    run_in(&f,
        "import geolib.{area, area}\n"
        "\n"
        "v: Float = area(Circle 1.0)\n", "open3.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 1));
    rfx_end(&f);
}

/* Two imports opening one name conflict at the second declaration. */
static void two_opens(void)
{
    rfx f;

    run_in(&f,
        "import geolib.{area}\n"
        "import geolib.{move, area}\n"
        "\n"
        "v: Float = area(Circle 1.0)\n", "open4.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 2));
    rfx_end(&f);
}

/* A local binding shadows an opened name legally. */
static void local_shadow(void)
{
    rfx f;
    const ek_ref *r;

    run_in(&f,
        "import geolib.{area}\n"
        "\n"
        "f: () -> Int =\n"
        "    area: Int = 1\n"
        "    area\n", "open5.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 5, "area");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    EK_CHECK(r->station == EK_ST_HEAD_LOCAL);
    rfx_end(&f);
}

/* Listed names are validated against the target surface: a private
   function, an unknown name, and a type spelling are all rejected. */
static void listed_names_checked(void)
{
    rfx f;

    run_in(&f, "import geolib.{_sum}\n\nv: Float = 1.0\n", "open6.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-private-name", 1));
    rfx_end(&f);

    run_in(&f, "import geolib.{nope}\n\nv: Float = 1.0\n", "open7.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-member", 1));
    rfx_end(&f);

    run_in(&f, "import geolib.{Shape}\n\nv: Float = 1.0\n", "open8.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-member", 1));
    rfx_end(&f);
}

/* An import without a list opens no free names. */
static void plain_import_opens_no_names(void)
{
    rfx f;

    run_in(&f,
        "import geolib\n"
        "\n"
        "v: Float = area(Circle 1.0)\n", "open9.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 3));
    rfx_end(&f);
}

int main(void)
{
    opened_lookup();
    opened_vs_top();
    duplicate_in_list();
    two_opens();
    local_shadow();
    listed_names_checked();
    plain_import_opens_no_names();
    return ek_test_report("resolve/selective");
}
