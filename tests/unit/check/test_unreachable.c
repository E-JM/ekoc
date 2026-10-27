#include "common.h"

/* Code after an expression of type `Never` is unreachable. The warning
   sits on the first following member of a block or inline suite; a
   `Never` arm of a case or an if is an alternative, not a sequence. */

static size_t warn_count(const cfx *f)
{
    size_t i, n = 0;

    for (i = 0; i < f->cdn; i++) {
        if (f->cds[i].code
            && strcmp(f->cds[i].code, "W-unreachable") == 0) {
            n++;
        }
    }
    return n;
}

static void after_divergence(void)
{
    cfx f;
    size_t i;

    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: () -> _ =\n"
              "    die(\"stop\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(warn_count(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "W-unreachable", 4));
    for (i = 0; i < f.cdn; i++) {
        if (f.cds[i].code
            && strcmp(f.cds[i].code, "W-unreachable") == 0) {
            EK_CHECK(f.cds[i].severity == EK_SEV_WARNING);
            EK_CHECK(f.cds[i].message != NULL
                     && strstr(f.cds[i].message,
                               "expression of type `Never`") != NULL);
        }
    }
    cfx_end(&f);

    /* The warning is one per unreachable run, on its first member. */
    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: () -> _ =\n"
              "    die(\"a\")\n"
              "    _\n"
              "    _\n");
    EK_CHECK_U64(warn_count(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "W-unreachable", 4));
    cfx_end(&f);

    /* A block whose last member diverges has no following code. */
    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: () -> _ =\n"
              "    _\n"
              "    die(\"x\")\n");
    EK_CHECK_U64(warn_count(&f), 0);
    cfx_end(&f);
}

static void inline_suite(void)
{
    cfx f;

    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: () -> _ =\n"
              "    g: () -> _ = (): die(\"x\"); _\n"
              "    g()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(warn_count(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "W-unreachable", 3));
    cfx_end(&f);
}

static void never_arms_are_alternatives(void)
{
    cfx f;

    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: (b: Bool) -> Int =\n"
              "    case b:\n"
              "        True -> die(\"x\")\n"
              "        False -> 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(warn_count(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "v: Int = if True: die(\"x\") else: 1\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(warn_count(&f), 0);
    cfx_end(&f);
}

static void whole_expression_diverges(void)
{
    cfx f;

    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: () -> _ =\n"
              "    case True:\n"
              "        True -> die(\"a\")\n"
              "        False -> die(\"b\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(warn_count(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "W-unreachable", 6));
    cfx_end(&f);

    cfx_begin(&f,
              "die: (m: String) -> Never = panic(m)\n"
              "f: () -> _ =\n"
              "    if True: die(\"a\") else: die(\"b\")\n"
              "    _\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    EK_CHECK_U64(warn_count(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "W-unreachable", 4));
    cfx_end(&f);
}

int main(void)
{
    after_divergence();
    inline_suite();
    never_arms_are_alternatives();
    whole_expression_diverges();
    return ek_test_report("check/unreachable");
}
