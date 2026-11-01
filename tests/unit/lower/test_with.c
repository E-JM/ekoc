#include "common.h"

/* The with service in the emitted text: the unnamed fields of the
   partial literal read the receiver's own slots, a nested partial
   literal rebuilds the field's record with the receiver's field value
   as its base, and a full nested literal lowers as a plain record
   literal with no receiver reads. */

/* A nested partial literal names fewer fields than the field's record
   holds: the ones it does not name read the receiver's field value at
   that slot, and none of the build's arguments is a unit pad. */
static void nested_partial(void)
{
    lfx f;

    lfx_begin(&f,
              "Address: type = [city: String, zip: String]\n"
              "Person: type = [name: String, age: Int, "
              "address: Address]\n"
              "main: () -> _ =\n"
              "    p: Person = {name: \"ada\", age: 36, "
              "address: {city: \"x\", zip: \"1\"}}\n"
              "    moved = p.with({address: {city: \"y\"}})\n"
              "    _ := moved.address.zip\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    /* the nested base is the receiver's own address field, and the
       zip the build takes is read off that base */
    EK_CHECK(lfx_has(&f, "((ekS_test_Address *)(((ekS_test_Person *)(t"));
    EK_CHECK(lfx_has(&f, "->f_address).as.ref)->f_zip)"));
    EK_CHECK(lfx_lacks(&f, ", ek_val_unit())"));
    lfx_end(&f);
}

/* Partiality runs at every layer: a with argument two records deep
   rebuilds each layer over the receiver's field chain down to the
   innermost unnamed slot. */
static void deep_partial(void)
{
    lfx f;

    lfx_begin(&f,
              "A: type = [x: Int, y: Int]\n"
              "B: type = [a: A, z: Int]\n"
              "C: type = [b: B, w: Int]\n"
              "main: () -> _ =\n"
              "    c: C = {b: {a: {x: 1, y: 2}, z: 3}, w: 4}\n"
              "    d = c.with({b: {a: {y: 20}}})\n"
              "    _ := d.b.a.x\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "->f_b).as.ref)->f_a)"));
    EK_CHECK(lfx_has(&f, "->f_a).as.ref)->f_x)"));
    EK_CHECK(lfx_lacks(&f, ", ek_val_unit())"));
    lfx_end(&f);
}

/* A nested literal naming every field of the field's record is a full
   literal: it lowers as a plain record literal and reads nothing off
   the receiver at that layer. */
static void full_nested_value(void)
{
    lfx f;

    lfx_begin(&f,
              "Address: type = [city: String, zip: String]\n"
              "Person: type = [name: String, age: Int, "
              "address: Address]\n"
              "main: () -> _ =\n"
              "    p: Person = {name: \"ada\", age: 36, "
              "address: {city: \"x\", zip: \"1\"}}\n"
              "    moved = p.with({address: {city: \"y\", "
              "zip: \"9\"}})\n"
              "    _ := moved.address.zip\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ekn_test_Address_rec(t"));
    EK_CHECK(lfx_lacks(&f, "->f_address).as.ref)->f_zip)"));
    lfx_end(&f);
}

/* The receiver may be an anonymous record: the rebuild runs through
   the anonymous family and the unnamed fields read its slots. */
static void anon_receiver(void)
{
    lfx f;

    lfx_begin(&f,
              "main: () -> _ =\n"
              "    r: [x: Int, s: String] = {x: 1, s: \"a\"}\n"
              "    t = r.with({s: \"b\"})\n"
              "    _ := t.x\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "((ekS_rec0 *)(t"));
    EK_CHECK(lfx_has(&f, ")->f_x)"));
    lfx_end(&f);
}

int main(void)
{
    nested_partial();
    deep_partial();
    full_nested_value();
    anon_receiver();
    return ek_test_report("lower/with");
}
