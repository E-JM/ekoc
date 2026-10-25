#include "common.h"

/* The nine negative conformance entries of the M6a batch (Annex D.3):
   each snippet materializes the manifest program, runs the full
   pipeline, and asserts the exact code at the correct position plus
   the rejection itself. N11 has two halves: the manifest declaration
   (diagnosed in parse) and the check-side type-position reference. */

#define CONF_DIR "docs/tests/conformance"

/* N01 — nominal value into another nominal type (Clause 7.5.2). */
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

/* N02 — wrong shape into an anonymous parameter (Clause 7.5.5). */
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

/* N03 — missing constructor arm (Clause 7.7.2). */
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

/* N09 — result-only generic variable (Clause 7.6.5). */
static void n09(void)
{
    cfx f;

    cfx_begin(&f, "empty: () -> []t = {}\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-phantom", 1));
    cfx_end(&f);
}

/* N10 — phantom type binder (Clause 7.6.2). */
static void n10(void)
{
    cfx f;

    cfx_begin(&f, "Value: type t u = Second t\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-phantom", 1));
    cfx_end(&f);
}

/* N11, first half — the manifest declaration; the name case of a type
   declaration is diagnosed in parse (Clause 5.5.2). */
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
   qualified type reference (Clauses 5.5.2, 7.4.1). The fixture is the
   suite's own geolib.eko on the module path; nothing is written. */
static void n11_use(void)
{
    static const char *mpath[1] = {CONF_DIR};
    cfx f;

    if (!cfx_read_file(CONF_DIR "/geolib.eko")) {
        EK_CHECK(0);
        return;
    }
    cfx_run(&f,
            "import geolib\n"
            "\n"
            "bad: geolib.shape = 1\n",
            mpath, 1, "n11use.eko");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-name-case", 3));
    cfx_end(&f);
}

/* N14 — integer literal above Int.max (Clause 5.7.4, 5.7.1). */
static void n14(void)
{
    cfx f;

    cfx_begin(&f, "big: Int = 9223372036854775808\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-literal-overflow", 1));
    cfx_end(&f);
}

/* N18 — mixed numeric operands; no implicit conversion (Clause
   7.8.4). The int literal never lands at Float, so the expression is
   rejected. */
static void n18(void)
{
    cfx f;

    cfx_begin(&f, "ratio: Float = 1 / 2.0\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 1));
    cfx_end(&f);
}

/* N19 — `:=` on an unannotated lambda (Clauses 6.3.3, 7.6.3). */
static void n19(void)
{
    cfx f;

    cfx_begin(&f, "id := (x): x\n");
    EK_CHECK(cfx_errors(&f) >= 1);
    EK_CHECK(cfx_code_at_line(&f, "E-lambda-check-only", 1));
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
    return ek_test_report("check/negative");
}
