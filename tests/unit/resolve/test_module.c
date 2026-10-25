#include "common.h"

#include <sys/stat.h>

/* Imports and modules (Clause 7.2): resolution on a real module path,
   privacy of Clause 7.3.1, cycles, stem collisions, and the receiver
   form of Clause 7.4.3 station 4. */

#define MOD_DIR "build/tmp-resolve-m5/mod"

static void put_fixtures(void)
{
    mkdir("build/tmp-resolve-m5", 0755);
    mkdir(MOD_DIR, 0755);
    mkdir(MOD_DIR "/x", 0755);
    mkdir(MOD_DIR "/y", 0755);
    EK_CHECK(rfx_put(MOD_DIR, "geolib.eko",
        "Shape: type = Circle Float | Rect [Float, Float]\n"
        "Point: type = [x: Float, y: Float]\n"
        "_Hide: type = [v: Int]\n"
        "pi: Float = 3.0\n"
        "area: (s: Shape) -> Float =\n"
        "    case s:\n"
        "        Circle r -> r\n"
        "        Rect {w, h} -> w\n"
        "_secret: (s: Shape) -> Float = 0.0\n"));
    EK_CHECK(rfx_put(MOD_DIR, "a.eko",
        "import b\n"
        "\n"
        "v: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "b.eko",
        "import a\n"
        "\n"
        "w: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "x/a.eko", "u: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "y/a.eko", "d: Int = 2\n"));
}

static void run_in(rfx *f, const char *src, const char *name)
{
    const char *path[1] = { MOD_DIR };

    rfx_run(f, src, path, 1, name);
}

/* The implicit surface of an import: types and constructors open
   unqualified, functions stay module-qualified (Clause 7.2.3). */
static void import_opens(void)
{
    rfx f;
    const ek_ref *r;

    run_in(&f,
        "import geolib\n"
        "\n"
        "area_of: (s: geolib.Shape) -> Float = geolib.area(s)\n"
        "mk: Shape = Circle 2.0\n"
        "ctor_ref: (s: Shape) -> Shape = geolib.Circle\n"
        "approx: Float = geolib.pi\n", "use.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "area");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "geolib");
    r = rfx_ref_named(&f, 4, "Circle");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_CONS);
    r = rfx_ref_named(&f, 5, "Circle");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    EK_CHECK(rfx_surface_sym(&f, "geolib", "_secret") != NULL);
    EK_CHECK(f.res.module_count == 2); /* use + geolib */
    rfx_end(&f);
}

/* Station 4: x.Mod.f(args) names an imported module (Clause 7.4.3). */
static void ufcs_station(void)
{
    rfx f;
    const ek_ref *r;

    run_in(&f,
        "import geolib\n"
        "\n"
        "ufcs: (s: geolib.Shape) -> Float = s.geolib.area()\n",
        "ufcs.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "area");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_FUNC);
    EK_CHECK(r->station == EK_ST_MODULE_PATH);
    rfx_end(&f);
}

/* Privacy of Clause 7.3.1 across modules. */
static void privacy(void)
{
    rfx f;

    run_in(&f,
        "import geolib\n"
        "\n"
        "p: (s: geolib.Shape) -> Float = geolib._secret(s)\n",
        "priv1.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-private-name", 3));
    rfx_end(&f);

    run_in(&f,
        "import geolib\n"
        "\n"
        "t: (h: geolib._Hide) -> Int = 0\n", "priv2.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-private-name", 3));
    rfx_end(&f);
}

static void missing_file(void)
{
    rfx f;

    run_in(&f,
        "import nosuch\n"
        "\n"
        "v: Int = 1\n", "miss.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-import-missing", 1));
    rfx_end(&f);
}

static void cycle(void)
{
    rfx f;
    char *src = rfx_read_file(MOD_DIR "/a.eko");

    EK_CHECK(src != NULL);
    if (src) {
        run_in(&f, src, "a.eko");
        EK_CHECK(rfx_has_code(&f, "E-cycle-import"));
        EK_CHECK(rfx_errors(&f) == 1); /* the walk reports the cycle once */
        rfx_end(&f);
    }
    free(src);
}

/* Two imported modules with the same stem, judged over the closure
   (Clauses 7.2.1, 7.4.6). */
static void stem_collision(void)
{
    rfx f;

    run_in(&f,
        "import x/a\n"
        "import y/a\n", "coll.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 2));
    rfx_end(&f);
}

/* An explicit `import std` is redundant (Clause 7.2.5). */
static void explicit_std(void)
{
    rfx f;

    run_in(&f,
        "import std\n"
        "\n"
        "v: Int = 1\n", "stdimp.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(rfx_has_code(&f, "W-redundant-import"));
    rfx_end(&f);
}

/* Module names are snake_case and never begin with `_`
   (Clauses 5.5.2, 7.2.1). */
static void bad_names(void)
{
    rfx f;

    run_in(&f, "import Geolib\n\nv: Int = 1\n", "case1.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-case", 1));
    rfx_end(&f);

    run_in(&f, "import _geo\n\nv: Int = 1\n", "case2.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-module-name", 1));
    rfx_end(&f);
}

/* A module never enters value position (Clause 7.2.6). */
static void module_value(void)
{
    rfx f;

    run_in(&f,
        "import geolib\n"
        "\n"
        "v: Int = geolib\n", "mval.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-module-value", 3));
    rfx_end(&f);
}

int main(void)
{
    put_fixtures();
    import_opens();
    ufcs_station();
    privacy();
    missing_file();
    cycle();
    stem_collision();
    explicit_std();
    bad_names();
    module_value();
    return ek_test_report("resolve/module");
}
