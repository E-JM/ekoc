#include "common.h"

/* Receiver-station selection: the field, namespace, derived-service,
   and current-module readings, the qualified and module-path calls,
   and the derived eq and lt members. Geolib provides the cross-module
   fixture. */

#define CONF_DIR "tests/conformance"

static const char *const geo_path[1] = {CONF_DIR};

static void station_field(void)
{
    cfx f;

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "n: String = p.name\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_FIELD));
    cfx_end(&f);

    /* A field of a record-shaped nominal reads the same way. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "h: Human = {name: \"ada\", age: 36}\n"
              "v: Int = h.age\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_FIELD));
    cfx_end(&f);
}

static void station_namespace(void)
{
    cfx f;

    /* A declared namespace member. */
    cfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "Money.show: (m: Money) -> String = \"m\"\n"
              "m: Money = {cents: 1, cur: \"USD\"}\n"
              "s: String = m.show()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_NAMESPACE));
    cfx_end(&f);
}

static void station_module_fn(void)
{
    cfx f;

    /* A current-module function whose first parameter is the
       receiver's type. */
    cfx_begin(&f,
              "double: (n: Int) -> Int = n * 2\n"
              "v: Int = 4.double()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_MODULE_FN));
    cfx_end(&f);
}

static void derived_services(void)
{
    cfx f;

    /* Derived eq on a declared ADT. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "a: Sex = Man\n"
              "b: Sex = Woman\n"
              "v: Bool = a.eq(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    cfx_end(&f);

    /* Derived lt by declaration order. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "a: Sex = Man\n"
              "b: Sex = Woman\n"
              "v: Bool = a.lt(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    cfx_end(&f);
}

static void opaque_members(void)
{
    cfx f;

    /* The std namespace members are unknown under the placeholder:
       the reading is opaque and nothing is diagnosed. */
    cfx_begin(&f,
              "xs: []Int = {1, 2}\n"
              "ys: []Int = xs.map((n): n + 1)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_EXTERNAL));
    cfx_end(&f);

    /* A receiver of unknown type reads as unknown. */
    cfx_begin(&f,
              "w := \"ab\".to_bytes()\n"
              "n := w.length()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_UNKNOWN));
    cfx_end(&f);
}

static void qualified_calls(void)
{
    char *geo;
    cfx f;

    geo = cfx_read_file(CONF_DIR "/geolib.eko");
    EK_CHECK(geo != NULL);
    free(geo);
    if (!geo) {
        return;
    }
    /* A module-path call through a module segment, plus the qualified
       head. */
    cfx_run(&f,
            "import geolib\n"
            "\n"
            "p: geolib.Point = {x: 3.0, y: 4.0}\n"
            "a: Float = p.geolib.coord_sum()\n"
            "b: Float = geolib.coord_sum(p)\n",
            geo_path, 1, "qualified.eko");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    cfx_end(&f);
}

int main(void)
{
    station_field();
    station_namespace();
    station_module_fn();
    derived_services();
    opaque_members();
    qualified_calls();
    return ek_test_report("check/member");
}
