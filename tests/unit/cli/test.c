#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The `ekoc test` gate: the built ekoc drives test fixtures written to
   build/ under the repository root as the working directory. Each run
   reports one module header per test root, then one line per test, on
   standard output; the refusal and the compile reports land on
   standard error. */

static const char *t_passing =
    "adds: test =\n"
    "    assert_eq(1 + 1, 2)\n"
    "\n"
    "holds: test =\n"
    "    assert(2 > 1)\n";

static const char *t_failing =
    "bad_div: test =\n"
    "    x = 1 / 0\n"
    "    _\n";

static const char *t_mixed =
    "good_one: test =\n"
    "    assert(1 == 1)\n"
    "\n"
    "broken: test =\n"
    "    x = 1 / 0\n"
    "    _\n";

static const char *t_no_tests =
    "main: () -> _ =\n"
    "    _\n";

static const char *t_broken_code =
    "main: () -> _ =\n"
    "    q = nothing_here\n"
    "    _\n";

static const char *t_crashing =
    "loop: (x: Int) -> Int = 1 + loop(x)\n"
    "\n"
    "explodes: test =\n"
    "    loop(1)\n";

/* A two-module unit: the imported module holds two tests beside the
   entry's own one. */
static const char *t_mmod_mod =
    "three: () -> Int = 3\n"
    "\n"
    "alpha: test =\n"
    "    assert_eq(three(), 3)\n"
    "\n"
    "beta: test =\n"
    "    assert(1 < 2)\n";

static const char *t_mmod_entry =
    "import mod\n"
    "\n"
    "gamma: test =\n"
    "    assert_eq(mod.three(), 3)\n";

/* An entry without test bindings over an imported module with them:
   the import's tests are the whole run. */
static const char *t_quiet_entry =
    "import mod\n"
    "\n"
    "main: () -> _ =\n"
    "    t = mod.three()\n"
    "    assert_eq(t, 3)\n"
    "    _\n";

/* A failing test inside the imported module fails the whole run. */
static const char *t_badmod =
    "half: () -> Int = 0\n"
    "\n"
    "bad_div: test =\n"
    "    _x = 1 / half()\n"
    "    _\n";

static const char *t_badmod_entry =
    "import mod_bad\n"
    "\n"
    "e_case: test =\n"
    "    assert_eq(mod_bad.half(), 0)\n";

/* Two modules declaring one same-named test: each runs in its own
   driver, and the report tells the modules apart. */
static const char *t_coll_ca =
    "one: () -> Int = 1\n"
    "\n"
    "same_name: test =\n"
    "    assert_eq(one(), 1)\n";

static const char *t_coll_cb =
    "two: () -> Int = 2\n"
    "\n"
    "same_name: test =\n"
    "    assert_eq(two(), 2)\n";

static const char *t_coll_entry =
    "import ca\n"
    "import cb\n"
    "\n"
    "glue: test =\n"
    "    assert_eq(ca.one() + cb.two(), 3)\n";

/* A program root without `main` is refused before anything compiles. */
static const char *t_no_main =
    "x: Int = 3\n";

/* A clean program whose local binding is unused: the build succeeds and
   the warning still reports. */
static const char *t_unused =
    "main: () -> _ =\n"
    "    a = 1\n"
    "    _\n";

/* A lambda-valued module member and a module-path receiver call: the
   member runs the lambda, and `n.foo.add()` passes n first. */
static const char *t_member_foo =
    "bar: (String) -> String = (s): \"hello: {s}\"\n"
    "add: (num: Int) -> Int = num + 2\n";

static const char *t_member_main =
    "import foo\n"
    "\n"
    "first: test =\n"
    "    assert_eq(foo.bar(\"Erxat\"), \"hello: Erxat\")\n"
    "    n := 4\n"
    "    assert_eq(foo.add(n), 6)\n"
    "    assert_eq(n.foo.add(), 6)\n";

/* Runs one ekoc command line; standard output and standard error land
   in the two files. Returns the wait status. */
static int run_cli(const char *args, const char *out_path,
                   const char *err_path)
{
    char cmd[512];
    int n;

    n = snprintf(cmd, sizeof cmd, "./build/ekoc %s > %s 2> %s",
                 args, out_path, err_path);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return -1;
    }
    return system(cmd);
}

static int write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    size_t len = strlen(text);

    if (fp == NULL) {
        return 0;
    }
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

/* The file's length, or -1 when it cannot be read. */
static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    size_t got;
    int err;

    if (fp == NULL) {
        return -1;
    }
    got = fread(buf, 1, cap - 1, fp);
    err = ferror(fp);
    buf[got] = '\0';
    fclose(fp);
    return err != 0 ? -1 : (int)got;
}

/* Exited with the given status. */
static int exited(int status, int code)
{
    return status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == code;
}

/* One module header, then one line per test in declaration order, then
   the summary. */
static void passing_root(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_t_pass.eko", t_passing));
    status = run_cli("test build/cli_t_pass.eko", "build/cli_t_pass.out",
                     "build/cli_t_pass.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_pass.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_pass\nok adds\nok holds\n2/2 passed\n");
    EK_CHECK(read_file("build/cli_t_pass.err", buf, sizeof buf) == 0);
}

/* A panicking test reports the panic block's first line. */
static void failing_root(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_t_fail.eko", t_failing));
    status = run_cli("test build/cli_t_fail.eko", "build/cli_t_fail.out",
                     "build/cli_t_fail.err");
    EK_CHECK(exited(status, 1));
    EK_CHECK(read_file("build/cli_t_fail.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_fail\n"
                      "FAIL bad_div: [P-div] division by zero\n"
                      "0/1 passed\n");
}

/* The passing tests still report, and the run fails as a whole. */
static void mixed_root(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_t_mixed.eko", t_mixed));
    status = run_cli("test build/cli_t_mixed.eko", "build/cli_t_mixed.out",
                     "build/cli_t_mixed.err");
    EK_CHECK(exited(status, 1));
    EK_CHECK(read_file("build/cli_t_mixed.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_mixed\n"
                      "ok good_one\nFAIL broken: [P-div] division by zero\n"
                      "1/2 passed\n");
}

/* A unit without test bindings in any module is refused before
   anything runs. */
static void refusal_no_tests(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_t_none.eko", t_no_tests));
    status = run_cli("test build/cli_t_none.eko", "build/cli_t_none.out",
                     "build/cli_t_none.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_none.out", buf, sizeof buf) == 0);
    EK_CHECK(read_file("build/cli_t_none.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, "declares no test bindings") != NULL);
}

static void compile_error(void)
{
    char buf[512];
    int status;

    EK_CHECK(write_file("build/cli_t_bad.eko", t_broken_code));
    status = run_cli("test build/cli_t_bad.eko", "build/cli_t_bad.out",
                     "build/cli_t_bad.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_bad.out", buf, sizeof buf) == 0);
    EK_CHECK(read_file("build/cli_t_bad.err", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "build/cli_t_bad.eko:", 20) == 0);
    EK_CHECK(strstr(buf, ": E-") != NULL);
}

/* A native fault in the child is a crash, not a panic. */
static void crashed_test(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_t_crash.eko", t_crashing));
    status = run_cli("test build/cli_t_crash.eko", "build/cli_t_crash.out",
                     "build/cli_t_crash.err");
    EK_CHECK(exited(status, 1));
    EK_CHECK(read_file("build/cli_t_crash.out", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "module cli_t_crash\n"
                          "FAIL explodes: crashed (signal ", 48) == 0);
}

/* A program root that declares no `main` is refused with the entry
   diagnostic, before anything compiles. */
static void build_missing_main(void)
{
    char buf[512];
    int status;

    EK_CHECK(write_file("build/cli_t_nomain.eko", t_no_main));
    status = run_cli("build build/cli_t_nomain.eko -o build/cli_t_nomain",
                     "build/cli_t_nomain.out", "build/cli_t_nomain.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_nomain.out", buf, sizeof buf) == 0);
    EK_CHECK(read_file("build/cli_t_nomain.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, ": E-entry-point: the entry module declares no "
                         "`main`") != NULL);
}

/* A warning over a clean build still reports; the build succeeds. */
static void build_reports_warnings(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_t_unused.eko", t_unused));
    status = run_cli("build build/cli_t_unused.eko -o build/cli_t_unused",
                     "build/cli_t_unused.out", "build/cli_t_unused.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_unused.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "wrote build/cli_t_unused\n");
    EK_CHECK(read_file("build/cli_t_unused.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, ": W-unused: the binding `a` is never used")
                 != NULL);
}

/* The check command: every diagnostic reports and nothing compiles --
   no binary is written; a test root is exempt from `main`. */
static void check_command(void)
{
    char buf[512];
    int status;

    EK_CHECK(write_file("build/cli_t_chk_unused.eko", t_unused));
    status = run_cli("check build/cli_t_chk_unused.eko",
                     "build/cli_t_chk.out", "build/cli_t_chk.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_chk.out", buf, sizeof buf) == 0);
    EK_CHECK(read_file("build/cli_t_chk.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, ": W-unused:") != NULL);

    EK_CHECK(write_file("build/cli_t_chk_bad.eko", t_broken_code));
    status = run_cli("check build/cli_t_chk_bad.eko",
                     "build/cli_t_chk.out", "build/cli_t_chk.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_chk.out", buf, sizeof buf) == 0);
    EK_CHECK(read_file("build/cli_t_chk.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, ": E-") != NULL);

    EK_CHECK(write_file("build/cli_t_chk_nomain.eko", t_no_main));
    status = run_cli("check build/cli_t_chk_nomain.eko",
                     "build/cli_t_chk.out", "build/cli_t_chk.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_chk.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, "E-entry-point") != NULL);

    EK_CHECK(write_file("build/cli_t_chk_root.eko", t_passing));
    status = run_cli("check build/cli_t_chk_root.eko",
                     "build/cli_t_chk.out", "build/cli_t_chk.err");
    EK_CHECK(exited(status, 0));

    status = run_cli("check", "build/cli_t_chk.out",
                     "build/cli_t_chk.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_chk.err", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "usage:", 6) == 0);
}

/* A lambda-valued module member called cross-module runs the lambda, and
   the receiver-form module path passes its receiver as the first
   argument. The imported module declares no tests, so the entry's test
   is the whole run. */
static void module_member_calls(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/foo.eko", t_member_foo));
    EK_CHECK(write_file("build/cli_t_member.eko", t_member_main));
    status = run_cli("test build/cli_t_member.eko",
                     "build/cli_t_member.out", "build/cli_t_member.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_member.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_member\nok first\n1/1 passed\n");
    EK_CHECK(read_file("build/cli_t_member.err", buf, sizeof buf) == 0);
}

/* The tests of an imported module run beside the entry's own: one
   driver per test root, the summary over all of them. */
static void imported_module_tests(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/mod.eko", t_mmod_mod));
    EK_CHECK(write_file("build/cli_t_mm.eko", t_mmod_entry));
    status = run_cli("test build/cli_t_mm.eko", "build/cli_t_mm.out",
                     "build/cli_t_mm.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_mm.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_mm\nok gamma\n"
                      "module mod\nok alpha\nok beta\n"
                      "3/3 passed\n");
    EK_CHECK(read_file("build/cli_t_mm.err", buf, sizeof buf) == 0);
}

/* An entry without test bindings runs the imported module's tests
   instead of being refused. */
static void entry_without_tests(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/mod.eko", t_mmod_mod));
    EK_CHECK(write_file("build/cli_t_quiet.eko", t_quiet_entry));
    status = run_cli("test build/cli_t_quiet.eko", "build/cli_t_quiet.out",
                     "build/cli_t_quiet.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_quiet.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module mod\nok alpha\nok beta\n2/2 passed\n");
    EK_CHECK(read_file("build/cli_t_quiet.err", buf, sizeof buf) == 0);
}

/* A failing test inside an imported module fails the whole run after
   the entry's own tests passed. */
static void imported_module_failure(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/mod_bad.eko", t_badmod));
    EK_CHECK(write_file("build/cli_t_xe.eko", t_badmod_entry));
    status = run_cli("test build/cli_t_xe.eko", "build/cli_t_xe.out",
                     "build/cli_t_xe.err");
    EK_CHECK(exited(status, 1));
    EK_CHECK(read_file("build/cli_t_xe.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_xe\nok e_case\n"
                      "module mod_bad\n"
                      "FAIL bad_div: [P-div] division by zero\n"
                      "1/2 passed\n");
    EK_CHECK(read_file("build/cli_t_xe.err", buf, sizeof buf) == 0);
}

/* Two modules declaring one same-named test: each runs in its own
   driver and both report under their module headers. */
static void same_named_tests(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/ca.eko", t_coll_ca));
    EK_CHECK(write_file("build/cb.eko", t_coll_cb));
    EK_CHECK(write_file("build/cli_t_coll.eko", t_coll_entry));
    status = run_cli("test build/cli_t_coll.eko", "build/cli_t_coll.out",
                     "build/cli_t_coll.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_t_coll.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "module cli_t_coll\nok glue\n"
                      "module ca\nok same_name\n"
                      "module cb\nok same_name\n"
                      "3/3 passed\n");
    EK_CHECK(read_file("build/cli_t_coll.err", buf, sizeof buf) == 0);
}

static void bad_usage(void){
    char buf[64];
    int status;

    status = run_cli("test", "build/cli_t_usage.out",
                     "build/cli_t_usage.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_t_usage.err", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "usage:", 6) == 0);

    status = run_cli("test build/cli_t_pass.eko extra.eko",
                     "build/cli_t_usage.out", "build/cli_t_usage.err");
    EK_CHECK(exited(status, 2));

    status = run_cli("test build/cli_t_pass.eko --frobnicate",
                     "build/cli_t_usage.out", "build/cli_t_usage.err");
    EK_CHECK(exited(status, 2));

    status = run_cli("frobnicate build/cli_t_pass.eko",
                     "build/cli_t_usage.out", "build/cli_t_usage.err");
    EK_CHECK(exited(status, 2));
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "cli/test: cannot create build/\n");
        return 1;
    }
    passing_root();
    failing_root();
    mixed_root();
    crashed_test();
    refusal_no_tests();
    compile_error();
    build_missing_main();
    build_reports_warnings();
    check_command();
    module_member_calls();
    imported_module_tests();
    entry_without_tests();
    imported_module_failure();
    same_named_tests();
    bad_usage();
    return ek_test_report("cli/test");
}
