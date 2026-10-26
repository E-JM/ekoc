#include "common.h"

/* The placeholder std surface of Clause 7.2.5: the free names and the
   opened types resolve without diagnostics; everything else a file
   uses from the implicit import is the unresolved-external bridge
   (resolve.h; the placeholder stands in for lib/std.eko). */

static void free_names(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    assert_eq(1, 1)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 2, "assert_eq");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_EXTERNAL);
    EK_CHECK(r->station == EK_ST_EXTERNAL);
    EK_CHECK_STR(r->symbol ? r->symbol->name : NULL, "assert_eq");
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    rfx_end(&f);
}

static void opened_types(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: String = \"x\"\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_at(&f, 1); /* the annotation resolves first */
    EK_CHECK(r != NULL);
    EK_CHECK_STR(r->symbol ? r->symbol->name : NULL, "String");
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    rfx_end(&f);
}

static void std_constructors(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: Result Int String = Ok 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 1, "Ok");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_EXTERNAL);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    rfx_end(&f);
}

/* A name the placeholder does not know: an unknown bare name is
   unbound; an unknown member of a std type is the bridge
   (Clause 7.4.2 and the resolve.h ruling). */
static void unknown_names(void)
{
    rfx f;

    rfx_begin(&f,
        "f: Int = nothere\n");
    EK_CHECK_U64(rfx_errors(&f), 1);
    EK_CHECK(rfx_code_at_line(&f, "E-unbound-name", 1));
    rfx_end(&f);

    rfx_begin(&f,
        "f: () -> Int = String.unknown_thing()\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    /* A qualified Pascal head is the type-namespace reading; the
       placeholder knows the type, check owns its members. */
    {
        const ek_ref *r = rfx_ref_named(&f, 1, "String");

        EK_CHECK(r != NULL);
        EK_CHECK(r->kind == EK_SYM_TYPE);
    }
    rfx_end(&f);
}

static void qualified_std(void)
{
    rfx f;

    /* `std.show` is the qualified spelling of the implicit import
       (Clause 7.2.5). */
    rfx_begin(&f,
        "f: () -> _ =\n"
        "    std.show(1)\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    rfx_end(&f);
}

/* The qualified spelling is legal at every name position, including
   the pattern constructor position of the bridge (Clauses 7.2.5,
   7.4.4). */
static void qualified_std_constructors(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: (b: Bool) -> Int =\n"
        "    case b:\n"
        "        std.True -> 1\n"
        "        std.False -> 0\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "True");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_EXTERNAL);
    EK_CHECK(r->station == EK_ST_EXTERNAL);
    EK_CHECK_STR(r->symbol ? r->symbol->module : NULL, "std");
    rfx_end(&f);
}

/* Local shadowing of an opened free name is legal; only a top-level
   binding collides with the layer it occupies (Clause 7.2.5,
   7.4.6). */
static void local_shadow_free_name(void)
{
    rfx f;
    const ek_ref *r;

    rfx_begin(&f,
        "f: () -> _ =\n"
        "    show: Int = 1\n"
        "    show\n"
        "    _\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    r = rfx_ref_named(&f, 3, "show");
    EK_CHECK(r != NULL);
    EK_CHECK(r->kind == EK_SYM_LOCAL);
    EK_CHECK(r->station == EK_ST_HEAD_LOCAL);
    rfx_end(&f);
}

int main(void)
{
    free_names();
    opened_types();
    std_constructors();
    unknown_names();
    qualified_std();
    qualified_std_constructors();
    local_shadow_free_name();
    return ek_test_report("resolve/std");
}
