/* The undetermined-variable rule: an instantiation variable that
   survives the whole module was never determined and is rejected,
   while the bridge's opaque variables rest undetermined as their
   documented state. */

#include "common.h"

static void case_undetermined_arg(void)
{
    cfx f;

    cfx_begin(&f,
              "len: (l: []t) -> Int =\n"
              "    case l:\n"
              "        {} -> 0\n"
              "        {h, ..r} -> 1 + len(r)\n"
              "\n"
              "n := len({})\n");
    EK_CHECK_U64(cfx_errors(&f), 1);
    EK_CHECK(cfx_code_at_line(&f, "E-undetermined", 6));
    cfx_end(&f);
}

static void case_determined_controls(void)
{
    cfx f;

    /* The literal's element determines the variable. */
    cfx_begin(&f,
              "len: (l: []t) -> Int =\n"
              "    case l:\n"
              "        {} -> 0\n"
              "        {h, ..r} -> 1 + len(r)\n"
              "\n"
              "k: Int = len({1})\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    /* The expected type determines the variable after the call. */
    cfx_begin(&f,
              "id: (x: t) -> t = x\n"
              "\n"
              "w: []Int = id({})\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* The bridge answers for its own opacity: a std call site and an
   unforced monomorphic chain rest undetermined without a word. */
static void case_bridge_rests(void)
{
    cfx f;

    cfx_begin(&f, "n: Int = \"abc\".length()\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_begin(&f,
              "x := 3\n"
              "y := x\n");
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

int main(void)
{
    case_undetermined_arg();
    case_determined_controls();
    case_bridge_rests();
    return ek_test_report("check/undetermined");
}
