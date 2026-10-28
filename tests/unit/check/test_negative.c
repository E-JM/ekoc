#include "common.h"

/* The negative conformance entries outside the world-bit and native
   batches: each snippet materializes the manifest program, runs the
   full pipeline, and asserts the exact code at the correct position
   plus the rejection itself. N11 has two halves: the manifest
   declaration (diagnosed in parse) and the check-side type-position
   reference. N25 and N26 also carry the legal boundary of their
   checks: the companion programs that must keep compiling. */

#define CONF_DIR "tests/conformance"

/* A resolve-stage rejection at a source line: the check fixture keeps
   the resolve drain apart from check's. */
static int resolve_code_at(const cfx *f, const char *code, int line)
{
    size_t i;

    for (i = 0; i < f->dn; i++) {
        if (f->ds[i].code && strcmp(f->ds[i].code, code) == 0
            && f->ds[i].line == line) {
            return 1;
        }
    }
    return 0;
}

/* The geolib fixture on the module path is present and readable. */
static int have_geolib(void)
{
    char *geo = cfx_read_file(CONF_DIR "/geolib.eko");

    free(geo);
    return geo != NULL;
}

/* N01 — nominal value into another nominal type. */
static void n01(void)
{
    cfx f;

    cfx_begin(&f,
              "Celsius: type = [deg: Float]\n"
              "Fahrenheit: type = [deg: Float]\n"
              "\n"
              "c: Celsius = {deg: 0.0}\n"
              "f: Fahrenheit = c\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 5));
    cfx_end(&f);
}

/* N02 — wrong shape into an anonymous parameter. */
static void n02(void)
{
    cfx f;

    cfx_begin(&f,
              "Wide: type = [name: String, age: Int, city: String]\n"
              "\n"
              "take: (p: [name: String, age: Int]) -> String = p.name\n"
              "w: Wide = {name: \"ada\", age: 36, city: \"x\"}\n"
              "\n"
              "use: () -> String = take(w)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-shape-mismatch", 6));
    cfx_end(&f);
}

/* N03 — missing constructor arm. */
static void n03(void)
{
    cfx f;

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "\n"
              "label: (s: Sex) -> String =\n"
              "    case s:\n"
              "        Man -> \"m\"\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-exhaust", 4));
    cfx_end(&f);
}

/* N09 — result-only generic variable. */
static void n09(void)
{
    cfx f;

    cfx_begin(&f, "empty: () -> []t = {}\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-phantom", 1));
    cfx_end(&f);
}

/* N10 — phantom type binder. */
static void n10(void)
{
    cfx f;

    cfx_begin(&f, "Value: type t u = Second t\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-phantom", 1));
    cfx_end(&f);
}

/* N11, first half — the manifest declaration; the name case of a type
   declaration is diagnosed in parse. */
static void n11_decl(void)
{
    cfx f;
    size_t i;
    int hit = 0;

    cfx_begin(&f, "person: type = [name: String]\n");
    for (i = 0; i < f.dn; i++) {
        if (f.ds[i].code && strcmp(f.ds[i].code, "E-name-case") == 0
            && f.ds[i].line == 1) {
            hit = 1;
        }
    }
    EK_CHECK(hit);
    EK_CHECK(cfx_stage_errors(f.ds, f.dn) >= 1);
    cfx_end(&f);
}

/* N11, second half — the check side: a lowercase final segment of a
   qualified type reference. The fixture is the suite's own geolib.eko
   on the module path; nothing is written. */
static void n11_use(void)
{
    static const char *mpath[1] = {CONF_DIR};
    char *geo;
    cfx f;

    geo = cfx_read_file(CONF_DIR "/geolib.eko");
    if (!geo) {
        EK_CHECK(0);
        return;
    }
    free(geo);
    cfx_run(&f,
            "import geolib\n"
            "\n"
            "bad: geolib.shape = 1\n",
            mpath, 1, "n11use.eko");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-name-case", 3));
    cfx_end(&f);
}

/* N14 — integer literal above Int.max. */
static void n14(void)
{
    cfx f;

    cfx_begin(&f, "big: Int = 9223372036854775808\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);
}

/* N18 — mixed numeric operands; no implicit conversion. The int
   literal never lands at Float, so the expression is rejected. */
static void n18(void)
{
    cfx f;

    cfx_begin(&f, "ratio: Float = 1 / 2.0\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);
}

/* N19 — `:=` on an unannotated lambda. */
static void n19(void)
{
    cfx f;

    cfx_begin(&f, "id := (x): x\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-lambda-check-only", 1));
    cfx_end(&f);
}

/* N25 — a name bound by two of one arm's patterns. */
static void n25(void)
{
    cfx f;

    cfx_begin(&f,
              "pair: (p: [Int, Int], q: [Int, Int]) -> Int =\n"
              "    case p, q:\n"
              "        {x, _}, {x, _} -> x\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-name-conflict", 3));
    cfx_end(&f);
}

/* N26 — a floating literal whose converted value is not finite. The
   boundary stays legal: a magnitude inside the finite range, a plain
   fraction, and an underflow to zero are all finite; the integer
   overflow path is untouched. */
static void n26(void)
{
    cfx f;

    cfx_begin(&f, "big: Float = 1e999\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);

    cfx_begin(&f, "big: Float = 1.8e308\n");
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);

    cfx_begin(&f,
              "a: Float = 1e308\n"
              "b: Float = 1.5\n"
              "c: Float = 1e-999\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "big: Int = 9223372036854775808\n");
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);
}

/* N27 — two visible constructors and no anchor. The same program's
   annotated uses stay legal. */
static void n27(void)
{
    cfx f;

    cfx_begin(&f,
              "MyBool: type = True | False\n"
              "\n"
              "x: Bool = True\n"
              "y: MyBool = True\n"
              "z = True\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-undetermined", 5));
    cfx_end(&f);
}

/* N28 — an opened free name and a file top-level binding share one
   layer, so the binding is a conflict at its declaration. */
static void n28(void)
{
    static const char *mpath[1] = {CONF_DIR};
    cfx f;

    if (!have_geolib()) {
        EK_CHECK(0);
        return;
    }
    cfx_run(&f,
            "import geolib.{area}\n"
            "\n"
            "area: (s: Shape) -> Float = 0.0\n",
            mpath, 1, "n28.eko");
    EK_CHECK(resolve_code_at(&f, "E-name-conflict", 3));
    EK_CHECK(cfx_errors(&f) == 0);
    cfx_end(&f);
}

/* N29 — two imports that open one name conflict at the second
   declaration. */
static void n29(void)
{
    static const char *mpath[1] = {CONF_DIR};
    cfx f;

    if (!have_geolib()) {
        EK_CHECK(0);
        return;
    }
    cfx_run(&f,
            "import geolib.{area}\n"
            "import geolib.{move, area}\n"
            "\n"
            "r: Shape = Rect {2.0, 3.0}\n"
            "a: Float = area(r)\n",
            mpath, 1, "n29.eko");
    EK_CHECK(resolve_code_at(&f, "E-name-conflict", 2));
    EK_CHECK(cfx_errors(&f) == 0);
    cfx_end(&f);
}


/* N30 — non-terminating generic instantiation. */
static void n30(void)
{
    cfx f;

    cfx_begin(&f,
              "W: type t = W t\n"
              "wrap: (x: t) -> W t = W x\n"
              "f: (x: t) -> Int = f(wrap(x))\n"
              "\n"
              "n: Int = f(0)\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-instantiation-depth", 3));
    cfx_end(&f);
}

/* A malformed type declaration is one parse diagnostic. The alias has
   no representation, so dependent check claims about its literals are
   not emitted; the legal shapes around it stay clean. */
static void alias_cascade(void)
{
    cfx f;

    cfx_begin(&f,
              "Scope: type = [vars: [String]Int]\n"
              "Env: type = []Scope\n"
              "f: (s: Scope) -> Env = (s): {s}\n");
    EK_CHECK(resolve_code_at(&f, "E-type-decl-form", 2));
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "Scope: type = [vars: [String]Int]\n"
              "Env: type = [items: []Scope]\n"
              "s: Scope = {vars: {\"a\": 1}}\n"
              "e: Env = {items: {s}}\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    n01();
    n02();
    n03();
    n09();
    n10();
    n11_decl();
    n11_use();
    n14();
    n18();
    n19();
    n25();
    n26();
    n27();
    n28();
    n29();
    n30();
    alias_cascade();
    return ek_test_report("check/negative");
}
