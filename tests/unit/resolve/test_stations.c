#include "common.h"

/* Resolution stations and classifications: the expression-head order,
   the receiver form, and constructor positions. */

/* The receiver position never consults lexical bindings. */
static void receiver_not_lexical(void)
{
    rfx f;

    rfx_begin(&f,
        "length: Int = 3\n"
        "f: (s: String) -> Int = s.length()\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(rfx_ref_named(&f, 2, "length") == NULL);
    EK_CHECK(rfx_has_member_ref(&f, 2, EK_ST_RECEIVER));
    rfx_end(&f);
}

/* A namespace member precedes the current-module function reading: an
   equally named member and function are ordered resolution, not a
   conflict. */
static void namespace_vs_current_module(void)
{
    rfx f;

    rfx_begin(&f,
        "Money: type = [cents: Int]\n"
        "Money.describe: (m: Money) -> String = \"m\"\n"
        "describe: (m: Money) -> String = \"free\"\n"
        "f: String =\n"
        "    m: Money = {cents: 1}\n"
        "    m.describe()\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(rfx_has_member_ref(&f, 6, EK_ST_RECEIVER));
    rfx_end(&f);
}

/* A bare `x.f` that is not a call is a field access. */
static void bare_field_access(void)
{
    rfx f;

    rfx_begin(&f,
        "r: [name: String] = {name: \"x\"}\n"
        "f: String =\n"
        "    q := r.name\n"
        "    q\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(rfx_has_member_ref(&f, 3, EK_ST_FIELD));
    rfx_end(&f);
}

/* A single receiver link spelled like a module is still a field
   reading: only a non-last link takes the module-path reading. */
static void last_link_is_not_a_module_segment(void)
{
    rfx f;

    rfx_begin(&f,
        "geolib: Int = 1\n"
        "f: Int =\n"
        "    p := geolib\n"
        "    p.geolib\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    EK_CHECK(rfx_has_member_ref(&f, 4, EK_ST_FIELD));
    rfx_end(&f);
}

static void constructors(void)
{
    rfx f;
    const ek_ref *r;

    /* A declared constructor at value position. */
    rfx_begin(&f,
        "Color: type = Red | Green\n"
        "f: Color = Red\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "Red");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_CONSTRUCTOR);
    EK_CHECK(r->station == EK_ST_CONS);
    rfx_end(&f);

    /* A type name that doubles as constructor: value position denotes
       the constructor, type position the type. */
    rfx_begin(&f,
        "Age: type = Age UInt\n"
        "f: Age = Age 36\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named_kind(&f, 2, "Age", EK_SYM_CONSTRUCTOR);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_CONS);
    r = rfx_ref_named_kind(&f, 2, "Age", EK_SYM_TYPE);
    EK_CHECK(r != NULL);
    EK_CHECK(r->station == EK_ST_TYPE);
    rfx_end(&f);
}

/* Type position: a bare lowercase name is a type variable of the
   annotation; a bare PascalCase name a declared type. */
static void type_positions(void)
{
    rfx f;

    rfx_begin(&f,
        "Box: type t = [v: t]\n"
        "f: (b: Box Int) -> Int = 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);

    rfx_begin(&f,
        "f: (x: t) -> t = x\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

int main(void)
{
    receiver_not_lexical();
    namespace_vs_current_module();
    bare_field_access();
    last_link_is_not_a_module_segment();
    constructors();
    type_positions();
    return ek_test_report("resolve/stations");
}
