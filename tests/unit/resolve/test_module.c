#include "common.h"

#include <sys/stat.h>

/* Imports and modules: resolution on a real module path, privacy,
   cycles, stem collisions, and module-path receiver chains. */

#define MOD_DIR "build/tmp-resolve/mod"

static void put_fixtures(void)
{
    mkdir("build/tmp-resolve", 0755);
    mkdir(MOD_DIR, 0755);
    mkdir(MOD_DIR "/x", 0755);
    mkdir(MOD_DIR "/y", 0755);
    mkdir(MOD_DIR "/sub", 0755);
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
    EK_CHECK(rfx_put(MOD_DIR, "s.eko", "v: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "sub/s.eko",
        "import s\n"
        "\n"
        "w: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "self.eko",
        "import self\n"
        "\n"
        "v: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "math.eko", "n: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "chain1.eko",
        "import chain2\n"
        "\n"
        "v: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "chain2.eko",
        "import chain3\n"
        "\n"
        "v: Int = 2\n"));
    EK_CHECK(rfx_put(MOD_DIR, "chain3.eko",
        "import chain4\n"
        "\n"
        "v: Int = 3\n"));
    EK_CHECK(rfx_put(MOD_DIR, "chain4.eko",
        "import chain5\n"
        "\n"
        "v: Int = 4\n"));
    EK_CHECK(rfx_put(MOD_DIR, "chain5.eko",
        "import chain1\n"
        "\n"
        "v: Int = 5\n"));
    EK_CHECK(rfx_put(MOD_DIR, "ages.eko",
        "Age: type = Age UInt\n"
        "label: (a: Age) -> UInt = 0\n"));
    EK_CHECK(rfx_put(MOD_DIR, "sub/std.eko", "zz: Int = 7\n"));
}

static void run_in(rfx *f, const char *src, const char *name)
{
    const char *path[1] = { MOD_DIR };

    rfx_run(f, src, path, 1, name);
}

/* The implicit surface of an import: types and constructors open
   unqualified, functions stay module-qualified. */
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
    EK_CHECK(f.res.module_count == 3); /* use + geolib + std */
    rfx_end(&f);
}

/* x.Mod.f(args) names an imported module through the module-path
   reading. */
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

/* Privacy across modules. */
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

/* An import path that lands on a directory resolves to no file. */
static void directory_import(void)
{
    rfx f;

    mkdir(MOD_DIR "/dirmod.eko", 0755);
    run_in(&f, "import dirmod\n\nv: Int = 1\n", "dir.eko");
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

/* Two imported modules with the same stem, judged over the closure. */
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

/* An explicit `import std` is redundant. */
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

/* Module names are snake_case and never begin with `_`. */
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

/* A module never enters value position. */
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

/* A repeated import line opens the target once; a duplicated open
   would fake same-named type and constructor clashes. */
static void duplicate_import(void)
{
    rfx f;

    run_in(&f,
        "import geolib\n"
        "import geolib\n"
        "\n"
        "mk: Shape = Circle 2.0\n"
        "approx: Float = geolib.pi\n", "dup.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(f.res.module_count == 3); /* use + geolib + std */
    rfx_end(&f);
}

/* s.eko and sub/s.eko share a stem; one importing the other is a stem
   collision over the closure, not a dependency cycle. */
static void same_stem_import_pair(void)
{
    rfx f;

    run_in(&f, "import sub/s\n\nv: Int = 1\n", "subroot.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_has_code(&f, "E-name-conflict"));
    EK_CHECK(!rfx_has_code(&f, "E-cycle-import"));
    rfx_end(&f);
}

/* A non-root module importing its own stem closes a cycle. */
static void self_import_module(void)
{
    rfx f;

    run_in(&f, "import self\n\nv: Int = 1\n", "selfroot.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-cycle-import", 1));
    rfx_end(&f);
}

/* A cycle reached through five imports is reported once. */
static void deep_chain_cycle(void)
{
    rfx f;

    run_in(&f, "import chain1\n\nv: Int = 1\n", "chainroot.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_has_code(&f, "E-cycle-import"));
    rfx_end(&f);
}

/* The std/ path is reserved: a user module holding a companion's stem
   collides whichever import comes first. */
static void std_companion_collision(void)
{
    rfx f;

    run_in(&f, "import math\nimport std/math\n", "comp1.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 2));
    rfx_end(&f);

    run_in(&f, "import std/math\nimport math\n", "comp2.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-conflict", 2));
    rfx_end(&f);
}

/* A user module is never named `std`, whatever the path spelling; the
   reserved stem registers nothing. */
static void std_stem_import(void)
{
    rfx f;

    run_in(&f, "import sub/std\n\nv: Int = 1\n", "stdstem.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-module-name", 1));
    EK_CHECK(f.res.module_count == 2); /* the root and std */
    rfx_end(&f);
}

/* The compilation root obeys the same stem rules as an import. */
static void root_name_rules(void)
{
    rfx f;

    rfx_run(&f, "v: Int = 1\n", NULL, 0, "std.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-module-name", 1));
    rfx_end(&f);

    rfx_run(&f, "v: Int = 1\n", NULL, 0, "Geolib.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-name-case", 1));
    rfx_end(&f);

    rfx_run(&f, "v: Int = 1\n", NULL, 0, "_geo.eko");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-module-name", 1));
    rfx_end(&f);
}

/* A constructor and its type MAY share a spelling; the qualified value
   and pattern readings denote the constructor. */
static void qualified_ctor_shared_spelling(void)
{
    rfx f;
    const ek_ref *r;

    run_in(&f,
        "import ages\n"
        "\n"
        "v: ages.Age = ages.Age(36)\n"
        "w: UInt = ages.Age\n", "qage.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named_kind(&f, 3, "Age", EK_SYM_CONSTRUCTOR);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    r = rfx_ref_named_kind(&f, 4, "Age", EK_SYM_CONSTRUCTOR);
    EK_CHECK(r != NULL);
    rfx_end(&f);

    run_in(&f,
        "import ages\n"
        "\n"
        "f: (a: ages.Age) -> UInt =\n"
        "    case a:\n"
        "        ages.Age n -> n\n"
        "        _ -> 0\n", "qpat.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named_kind(&f, 5, "Age", EK_SYM_CONSTRUCTOR);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_QUALIFIED);
    rfx_end(&f);
}

/* A diamond holds the shared module once and reports no cycle. */
static void diamond_share(void)
{
    rfx f;

    EK_CHECK(rfx_put(MOD_DIR, "shared.eko", "s: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "dleft.eko",
        "import shared\n\nl: Int = 1\n"));
    EK_CHECK(rfx_put(MOD_DIR, "dright.eko",
        "import shared\n\nr: Int = 2\n"));
    run_in(&f,
        "import dleft\n"
        "import dright\n"
        "import shared\n"
        "\n"
        "v: Int = 1\n", "droot.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(f.res.module_count == 5); /* root, std, and three imports */
    rfx_end(&f);
}

/* An empty module file imports and contributes no names; the stage
   chain yields a whole empty module. */
static void empty_module(void)
{
    rfx f;

    EK_CHECK(rfx_put(MOD_DIR, "emptymod.eko", ""));
    run_in(&f, "import emptymod\n\nv: Int = 1\n", "useempty.eko");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(f.res.module_count == 3);
    rfx_end(&f);
}

/* A file that fails to parse still yields its partial AST; the import
   enters the closure and the walk recovers. */
static void broken_import_file(void)
{
    rfx f;

    EK_CHECK(rfx_put(MOD_DIR, "broken.eko", "this is not : : valid (((\n"));
    run_in(&f, "import broken\n\nv: Int = 1\n", "usebroken.eko");
    EK_CHECK(f.res.module_count == 3);
    EK_CHECK(!rfx_has_code(&f, "E-import-missing"));
    EK_CHECK(!rfx_has_code(&f, "E-cycle-import"));
    rfx_end(&f);
}

int main(void)
{
    put_fixtures();
    import_opens();
    ufcs_station();
    privacy();
    missing_file();
    directory_import();
    cycle();
    stem_collision();
    explicit_std();
    bad_names();
    module_value();
    duplicate_import();
    same_stem_import_pair();
    self_import_module();
    deep_chain_cycle();
    std_companion_collision();
    std_stem_import();
    root_name_rules();
    qualified_ctor_shared_spelling();
    diamond_share();
    empty_module();
    broken_import_file();
    return ek_test_report("resolve/module");
}
