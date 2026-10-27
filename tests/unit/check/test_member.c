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

    /* A placeholder nominal receiver keeps the bridge. */
    cfx_begin(&f,
              "s: String = \"x\"\n"
              "t: String = s.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_EXTERNAL));
    cfx_end(&f);
}

/* A declared module's namespace is complete: a name the type does not
   carry is reported, in a call and in a bare field read alike. The std
   and container namespaces keep the bridge, because their members are
   supplied by the compiler. */
static void declared_namespace_is_complete(void)
{
    cfx f;

    cfx_begin(&f,
              "A: type = Man\n"
              "a: A = Man\n"
              "v: Int = a.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "M: type = [n: Int]\n"
              "m: M = {n: 1}\n"
              "v: Int = m.nope\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 3));
    cfx_end(&f);

    /* A tuple has no namespace, and a record has no order. */
    cfx_begin(&f,
              "t: [Int, Bool] = {1, True}\n"
              "v: Int = t.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "R: type = [n: Int]\n"
              "r: R = {n: 1}\n"
              "b: Bool = r.lt(r)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 3));
    cfx_end(&f);

    cfx_begin(&f,
              "a: Bool = True\n"
              "b: Bool = False\n"
              "c: Bool = a.lt(b)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 3));
    cfx_end(&f);

    /* The std namespaces still bridge their members. */
    cfx_begin(&f,
              "j: Json = Json.parse(\"{}\").unwrap()\n"
              "d: Duration = Time.sub(Time 5, Time 2)\n"
              "i: Int = Int.add(1, 2)\n"
              "xs: []Int = {1}\n"
              "ys: []Int = xs.map((n): n + 1)\n"
              "ds: [String]Int = {\"a\": 1}\n"
              "got: Option Int = ds.get(\"a\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A member link on a known function-typed receiver is rejected: a
   function type has no fields and no namespace. */
static void function_receiver(void)
{
    cfx f;

    cfx_begin(&f,
              "area: (n: Int) -> Float = 1.0\n"
              "v: Float = area.shape(1)\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "area: (n: Int) -> Float = 1.0\n"
              "v: Float = area.bogus\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
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

/* Any row on the line typed at the named nominal: the type an update
   lands at. */
static int nominal_at_line(const cfx *f, int line, const char *name)
{
    size_t i;

    for (i = 0; i < f->chk.count; i++) {
        const ek_type *t = f->chk.rows[i].type;

        if (f->chk.rows[i].node && f->chk.rows[i].node->line == line
            && t && t->kind == EK_T_NOMINAL && t->u.nominal.sym
            && strcmp(t->u.nominal.sym->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The derived with service: the receiver's record type comes back, the
   argument is a partial record literal checked field by field, layer
   by layer, and the service is pure (Clauses 6.6.3, 9.8.6, 9.8.7). */
static void derived_with(void)
{
    cfx f;

    /* A nominal record receiver: the update reads as the derived
       service and types at the receiver's own type. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age: 37})\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_DERIVED));
    EK_CHECK(cfx_bit_at(&f, 3, EK_BIT_PURE));
    EK_CHECK(nominal_at_line(&f, 3, "Human"));
    cfx_end(&f);

    /* An anonymous record receiver. */
    cfx_begin(&f,
              "p: [name: String, age: Int] = {name: \"ada\", age: 36}\n"
              "q := p.with({age: 37})\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_DERIVED));
    cfx_end(&f);

    /* The partial reading extends layer by layer through the field
       values: the inner literal may name a subset too. */
    cfx_begin(&f,
              "Address: type = [city: String, zip: String]\n"
              "Person: type = [name: String, age: Int, address: Address]\n"
              "p: Person = {name: \"ada\", age: 36, "
              "address: {city: \"x\", zip: \"1\"}}\n"
              "m := p.with({address: {city: \"y\"}})\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    EK_CHECK(nominal_at_line(&f, 4, "Person"));
    cfx_end(&f);

    /* An unknown field is named once, at its layer. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({ag: 37})\n");
    EK_CHECK(cfx_has_code(&f, "E-field-missing"));
    cfx_end(&f);

    cfx_begin(&f,
              "Address: type = [city: String, zip: String]\n"
              "Person: type = [name: String, age: Int, address: Address]\n"
              "p: Person = {name: \"ada\", age: 36, "
              "address: {city: \"x\", zip: \"1\"}}\n"
              "m := p.with({address: {cityy: \"y\"}})\n");
    EK_CHECK(cfx_has_code(&f, "E-field-missing"));
    cfx_end(&f);

    /* A field value keeps its own type. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age: \"37\"})\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* Shorthand braces are the comma family, not a record reading. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age})\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* The literal argument alone: a value in hand is not a partial
       literal. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with(37)\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* One argument: the partial literal. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with()\n");
    EK_CHECK(cfx_has_code(&f, "E-call-arity"));
    cfx_end(&f);

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age: 37}, 1)\n");
    EK_CHECK(cfx_has_code(&f, "E-call-arity"));
    cfx_end(&f);

    /* A receiver whose known type is not a record is rejected. */
    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "s: Sex = Man\n"
              "q := s.with({a: 1})\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* A duplicate field inside the update literal. */
    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age: 37, age: 38})\n");
    EK_CHECK(cfx_has_code(&f, "E-field-duplicate"));
    cfx_end(&f);
}


/* A call whose head is a type namespace takes no receiver: the
   explicit arguments are the whole argument list. A module-qualified
   head keeps its own reading. */
static void type_qualified_calls(void)
{
    cfx f;

    cfx_begin(&f,
              "T: type = [n: Int]\n"
              "v: T = {n: 1}\n"
              "b: Bool = T.eq(v, v)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_DERIVED));
    cfx_end(&f);

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "a: Sex = Man\n"
              "b: Sex = Woman\n"
              "v: Bool = Sex.lt(a, b)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_DERIVED));
    cfx_end(&f);

    /* A declared namespace member called through its type. */
    cfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "Money.show: (m: Money) -> String = m.cur\n"
              "s: String = Money.show({cents: 1, cur: \"USD\"})\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 3, EK_RD_NAMESPACE));
    cfx_end(&f);

    /* The std placeholder stays opaque through a type head. */
    cfx_begin(&f,
              "x: String = \"a\"\n"
              "s: String = String.show(x)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    station_field();
    station_namespace();
    station_module_fn();
    derived_services();
    derived_with();
    opaque_members();
    declared_namespace_is_complete();
    function_receiver();
    qualified_calls();
    type_qualified_calls();
    return ek_test_report("check/member");
}
