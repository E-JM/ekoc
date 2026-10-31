#include "common.h"

#include <sys/stat.h>

/* Module-qualified namespace members: `Mod.Type.f(args)` and the
   receiver chain `x.Mod.Type.f()`. The type segment is part of the
   callee's namespace path; the receiver is the value before the module
   segment. */

#define MEMBER_DIR "build/tmp-check-member"

/* A module fixture for the module-qualified namespace-member calls. */
static int put_module(const char *name, const char *content)
{
    char path[512];
    FILE *fp;
    int ok;

    mkdir("build", 0755);
    mkdir(MEMBER_DIR, 0755);
    snprintf(path, sizeof path, "%s/%s", MEMBER_DIR, name);
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    ok = fwrite(content, 1, strlen(content), fp) == strlen(content);
    fclose(fp);
    return ok;
}

static void module_qualified_members(void)
{
    static const char *const mpath[1] = {MEMBER_DIR};
    cfx f;

    EK_CHECK(put_module("mtypes.eko",
        "Number: type = One | Two\n"
        "Number.hi: (n: Number) -> String = \"hi\"\n"));
    cfx_run(&f,
        "import mtypes\n"
        "\n"
        "check: () -> String =\n"
        "    a: Number = One\n"
        "    b: mtypes.Number = Two\n"
        "    a.hi()\n"
        "    b.hi()\n"
        "    mtypes.Number.hi(b)\n"
        "    a.mtypes.Number.hi()\n"
        "    a.mtypes.Number.show()\n"
        "    mtypes.Number.show(b)\n", mpath, 1, "mtypes_use.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);
}

/* A head-qualified member read as a value: the member's scheme types
   the chain, so a module function of known signature fills an
   open-arrow parameter and its result determines the callee's
   variable. A receiver-form module path stays a field walk. */
static void module_member_values(void)
{
    static const char *const mpath[1] = {MEMBER_DIR};
    cfx f;

    EK_CHECK(put_module("mfns.eko",
        "double: (x: Int) -> Int = x * 2\n"
        "shout: (s: String) -> String = s\n"));
    cfx_run(&f,
        "import mfns\n"
        "\n"
        "use: () -> _ =\n"
        "    xs: []Int = {1, 2, 3}\n"
        "    ys := xs.map(mfns.double)\n"
        "    g := mfns.double\n"
        "    zs := xs.map(g)\n"
        "    _\n", mpath, 1, "mfns_use.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK_U64(cfx_errors(&f), 0);
    cfx_end(&f);

    cfx_run(&f,
        "import mfns\n"
        "\n"
        "use: () -> _ =\n"
        "    xs: []Int = {1, 2, 3}\n"
        "    ys := xs.map(mfns.shout)\n"
        "    _\n", mpath, 1, "mfns_bad.eko");
    EK_CHECK_U64(cfx_stage_errors(f.ds, f.dn), 0);
    EK_CHECK(cfx_code_at_line(&f, "E-type-mismatch", 5));
    cfx_end(&f);
}

int main(void)
{
    module_qualified_members();
    module_member_values();
    return ek_test_report("check/module-members");
}
