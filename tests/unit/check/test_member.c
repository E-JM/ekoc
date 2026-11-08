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

/* The phantom rule reaches a namespace member's annotation the way it
   reaches an ordinary annotated binding: a variable only in the result
   is reported at the member, while a variable occurring inside a
   parameter type stays legal. */
static void station_namespace_phantom(void)
{
    cfx f;

    cfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "Money.bad: () -> Option t = None\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-phantom", 2));
    cfx_end(&f);

    cfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "Money.wrap: (g: () -> t) -> t = g()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f, "bad: () -> Option t = None\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-phantom", 1));
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

    /* The dictionary namespace has a complete row set: a name no row
       carries is absent, not bridged. */
    cfx_begin(&f,
              "d: [String]Int = {\"a\": 1}\n"
              "v: Int = d.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    /* A landed row makes a receiver known: Bytes has a namespace and
       its length row types as Int. */
    cfx_begin(&f,
              "w: Bytes = \"ab\".to_bytes()\n"
              "n: Int = w.length()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 2, EK_RD_NAMESPACE));
    cfx_end(&f);

    /* A capability namespace is complete once its rows land: a name
       no station answers is absent, like any other declared
       namespace. */
    cfx_begin(&f,
              "c: Clock = Clock.frozen(Time 0)\n"
              "w := c.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);

    /* A String receiver has a complete row set plus its derived
       services: a name no station answers is absent. */
    cfx_begin(&f,
              "s: String = \"x\"\n"
              "t: String = s.bogus()\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-unbound-member", 2));
    cfx_end(&f);
}

/* A declared type's namespace is closed: a name the type does not
   carry is reported, in a call and in a bare field read alike. */
static void declared_ns_nominal(void)
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
}

/* A tuple has no namespace, and a record has no order. */
static void declared_ns_structural(void)
{
    cfx f;

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
}

/* The std namespace rows answer alongside the closed namespaces. */
static void declared_ns_std_rows(void)
{
    cfx f;

    cfx_begin(&f,
              "j: Json = Json.parse(\"null\").unwrap()\n"
              "d: Duration = Time.sub(Time 5, Time 2)\n"
              "i: Int = Int.add(1, 2)\n"
              "xs: []Int = {1}\n"
              "ys: []Int = xs.map((n): n + 1)\n"
              "ds: [String]Int = {\"a\": 1}\n"
              "got: Option Int = ds.get(\"a\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A declared module's namespace is complete: a name the type does not
   carry is reported, in a call and in a bare field read alike. A std
   or container namespace is closed once its row set is complete. */
static void declared_namespace_is_complete(void)
{
    declared_ns_nominal();
    declared_ns_structural();
    declared_ns_std_rows();
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

/* A nominal record receiver: the update reads as the derived service
   and types at the receiver's own type. */
static void with_nominal_reads(void)
{
    cfx f;

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
}

/* An unknown field is named once, at its layer. */
static void with_unknown_fields(void)
{
    cfx f;

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
}

/* A field value keeps its own type, shorthand braces are the comma
   family, and a value in hand is not a partial literal. */
static void with_field_type_errors(void)
{
    cfx f;

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age: \"37\"})\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age})\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with(37)\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);
}

/* The partial literal is the one argument, and a receiver whose known
   type is not a record is rejected. */
static void with_arity_and_receiver(void)
{
    cfx f;

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

    cfx_begin(&f,
              "Sex: type = Man | Woman\n"
              "s: Sex = Man\n"
              "q := s.with({a: 1})\n");
    EK_CHECK(cfx_has_code(&f, "E-type-mismatch"));
    cfx_end(&f);

    /* A type-variable receiver names no record: `with` is rejected at
       check time, not handed to a later stage (Clause 9.8.6). */
    cfx_begin(&f, "g: (r: t) -> t = r.with({})\n");
    EK_CHECK(cfx_has_code(&f, "E-no-member"));
    cfx_end(&f);
}

/* A duplicate field inside the update literal. */
static void with_duplicate_field(void)
{
    cfx f;

    cfx_begin(&f,
              "Human: type = [name: String, age: Int]\n"
              "p: Human = {name: \"ada\", age: 36}\n"
              "q := p.with({age: 37, age: 38})\n");
    EK_CHECK(cfx_has_code(&f, "E-field-duplicate"));
    cfx_end(&f);
}

/* The derived with service: the receiver's record type comes back, the
   argument is a partial record literal checked field by field, layer
   by layer, and the service is pure (Clauses 6.6.3, 9.8.6, 9.8.7). */
static void derived_with(void)
{
    with_nominal_reads();
    with_unknown_fields();
    with_field_type_errors();
    with_arity_and_receiver();
    with_duplicate_field();
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

/* Station 3 resolves a current-module function over any receiver
   shape, not only a nominal one: the structural composites and a
   function type reach the module reading (Clause 7.4.3). */
static void receiver_station_on_composites(void)
{
    cfx f;

    cfx_begin(&f,
              "sum_all: (xs: []Int) -> Int = xs.fold(0, (a, n): a + n)\n"
              "apply1: (fn: (Int) -> Int, x: Int) -> Int = fn(x)\n"
              "xs: []Int = {1, 2}\n"
              "a: Int = xs.sum_all()\n"
              "fn: (Int) -> Int = (n): n + 1\n"
              "b: Int = fn.apply1(7)\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_MODULE_FN));
    EK_CHECK(cfx_reading_at(&f, 6, EK_RD_MODULE_FN));
    cfx_end(&f);
}

/* The receiver of a member call conforms before the arguments, so an
   argument checked under a parameter that shares the receiver's
   variables sees them determined. */
static void receiver_binds_first(void)
{
    cfx f;

    cfx_begin(&f,
              "apply2: (i: Int, cb: (Int) -> Bool) -> Bool = cb(i)\n"
              "v: Bool = 1.apply2((x): x.eq(2))\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "apply2: (i: Int, cb: (Int) -> Bool) -> Bool = cb(i)\n"
              "v: Bool = 1.apply2((x): x.eq(\"no\"))\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-nominal-mismatch", 2));
    cfx_end(&f);
}

int main(void)
{
    station_field();
    station_namespace();
    station_namespace_phantom();
    station_module_fn();
    derived_services();
    derived_with();
    opaque_members();
    declared_namespace_is_complete();
    function_receiver();
    qualified_calls();
    type_qualified_calls();
    receiver_station_on_composites();
    receiver_binds_first();
    return ek_test_report("check/member");
}
