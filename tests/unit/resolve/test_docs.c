#include "common.h"

/* The document ride: the /// lines before a binding join into one
   string on the binding's symbol, so the semantic index carries the
   document together with the span of the binding it documents. A
   binding with no document carries none. */

static void value_binding(void)
{
    rfx f;
    const ek_symbol *s;

    rfx_begin(&f,
              "/// Adds one.\n"
              "_add1: (n: Int) -> Int = n\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    s = rfx_surface_sym(&f, "test", "_add1");
    EK_CHECK(s != NULL);
    if (s) {
        EK_CHECK(s->doc != NULL);
        if (s->doc) {
            EK_CHECK_STR(s->doc, "Adds one.");
        }
    }
    rfx_end(&f);
}

static void two_lines_join(void)
{
    rfx f;
    const ek_symbol *s;

    rfx_begin(&f,
              "/// Adds one.\n"
              "/// A second line.\n"
              "_add1: (n: Int) -> Int = n\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    s = rfx_surface_sym(&f, "test", "_add1");
    EK_CHECK(s != NULL);
    if (s) {
        EK_CHECK(s->doc != NULL);
        if (s->doc) {
            EK_CHECK_STR(s->doc, "Adds one.\nA second line.");
        }
    }
    rfx_end(&f);
}

static void no_document(void)
{
    rfx f;
    const ek_symbol *s;

    rfx_begin(&f, "_plain: Int = 1\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    s = rfx_surface_sym(&f, "test", "_plain");
    EK_CHECK(s != NULL);
    if (s) {
        EK_CHECK(s->doc == NULL);
    }
    rfx_end(&f);
}

static void blank_line_detaches(void)
{
    rfx f;
    const ek_symbol *s;

    /* A blank line between the document and the binding detaches it:
       a document sits directly above the line it describes. */
    rfx_begin(&f,
              "/// Adds one.\n"
              "\n"
              "_add1: (n: Int) -> Int = n\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    s = rfx_surface_sym(&f, "test", "_add1");
    EK_CHECK(s != NULL);
    if (s) {
        EK_CHECK(s->doc == NULL);
        EK_CHECK_U64(s->line, 3);
    }
    rfx_end(&f);
}

static void namespace_member(void)
{
    rfx f;
    const ek_symbol *s;

    rfx_begin(&f,
              "Money: type = [cents: Int]\n"
              "/// The decimal form.\n"
              "Money.describe: (m: Money) -> String = \"m\"\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    s = rfx_surface_sym(&f, "test", "describe");
    EK_CHECK(s != NULL);
    if (s) {
        EK_CHECK(s->doc != NULL);
        if (s->doc) {
            EK_CHECK_STR(s->doc, "The decimal form.");
        }
    }
    rfx_end(&f);
}

static void type_declaration(void)
{
    rfx f;
    const ek_symbol *s;

    rfx_begin(&f,
              "/// A color.\n"
              "Color: type = Red | Green\n");
    EK_CHECK_U64(rfx_errors(&f), 0);
    s = rfx_surface_sym(&f, "test", "Color");
    EK_CHECK(s != NULL);
    if (s) {
        EK_CHECK(s->doc != NULL);
        if (s->doc) {
            EK_CHECK_STR(s->doc, "A color.");
        }
    }
    rfx_end(&f);
}

int main(void)
{
    value_binding();
    two_lines_join();
    no_document();
    blank_line_detaches();
    namespace_member();
    type_declaration();
    return ek_test_report("resolve/docs");
}
