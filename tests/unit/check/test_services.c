#include "common.h"

/* The free-call form of a compiler service is the identity of the
   receiver form: show(t) is t.show(), and panic(m) is m.panic(). The
   first argument is the receiver, so the receiver's type selects the
   same namespace member; a receiver type the placeholder leaves opaque
   types the call opaquely, and a service used as a bare value stays
   opaque too. */

static void show_on_string(void)
{
    cfx f;

    cfx_begin(&f,
              "v: String = \"hi\"\n"
              "a: String = show(v)\n"
              "b: String = v.show()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A user record whose namespace declares show: both spellings select
   the declaration, so the free call types at its result. */
static void show_on_record(void)
{
    cfx f;

    cfx_begin(&f,
              "Money: type = [cents: Int, cur: String]\n"
              "Money.show: (m: Money) -> String = \"{m.cur} {m.cents}\"\n"
              "m: Money = {cents: 12500, cur: \"USD\"}\n"
              "a: String = show(m)\n"
              "b: String = m.show()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_NAMESPACE));
    cfx_end(&f);
}

static void panic_both_forms(void)
{
    cfx f;

    cfx_begin(&f,
              "die: (msg: String) -> Never = panic(msg)\n"
              "gone: (msg: String) -> Never = msg.panic()\n"
              "f: () -> Never = panic(\"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* No argument supplies a receiver; the call must not crash and must
   not invent a diagnostic out of the missing receiver. */
static void zero_argument_service(void)
{
    cfx f;

    cfx_begin(&f,
              "f: () -> _ =\n"
              "    show()\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A service outside call position stays opaque. */
static void bare_service_value(void)
{
    cfx f;

    cfx_begin(&f, "x := show\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A record field holding a function is station 1 for both spellings:
   the free call is the receiver call, and the receiver is not an
   argument. */
static void field_service(void)
{
    cfx f;

    cfx_begin(&f,
              "Money: type = [cents: Int, show: (String) -> String]\n"
              "fmt: (s: String) -> String = s\n"
              "m: Money = {cents: 1, show: fmt}\n"
              "a: String = m.show(\"x\")\n"
              "b: String = show(m, \"x\")\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK(cfx_reading_at(&f, 4, EK_RD_FIELD));
    EK_CHECK(cfx_reading_at(&f, 5, EK_RD_FIELD));
    cfx_end(&f);

    /* The arity is the field's arity in both spellings. */
    cfx_begin(&f,
              "Money: type = [cents: Int, show: (String) -> String]\n"
              "fmt: (s: String) -> String = s\n"
              "m: Money = {cents: 1, show: fmt}\n"
              "a: String = m.show()\n"
              "b: String = show(m)\n");
    EK_CHECK_U64(cfx_errors(&f), 2);
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 4));
    EK_CHECK(cfx_code_at_line(&f, "E-call-arity", 5));
    cfx_end(&f);
}

int main(void)
{
    show_on_string();
    show_on_record();
    field_service();
    panic_both_forms();
    zero_argument_service();
    bare_service_value();
    return ek_test_report("check/services");
}
