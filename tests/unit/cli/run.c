#define _POSIX_C_SOURCE 200809L

#include "harness.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The CLI gate: the built ekoc drives tiny fixtures written to build/
   under the repository root as the working directory. */

static const char *prog_good =
    "main: () -> _ =\n"
    "    a = 6\n"
    "    b = 7\n"
    "    r = a * b + 1\n"
    "    _\n";

static const char *prog_print =
    "main: (out: Out) ! = out.println(\"greetings\")\n";

static const char *prog_bad =
    "main: () -> _ =\n"
    "    q = nothing_here\n"
    "    _\n";

static const char *prog_time =
    "import std/time\n"
    "\n"
    "main: (out: Out) ! =\n"
    "    case time.Time.parse(\"2026-10-22T01:29:00Z\"):\n"
    "        Ok t -> out.println(time.Time.show(t))\n"
    "        Err _ -> out.println(\"parse failed\")\n";

static const char *prog_clock =
    "main: (clk: Clock, out: Out) ! =\n"
    "    out.println(show(clk.mono()))\n";

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

/* Runs one built program; returns the wait status. */
static int run_prog(const char *path)
{
    return system(path);
}

/* Runs one built program with its standard output in the file; returns
   the wait status. */
static int run_prog_out(const char *path, const char *out_path)
{
    char cmd[512];
    int n;

    n = snprintf(cmd, sizeof cmd, "%s > %s", path, out_path);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return -1;
    }
    return system(cmd);
}

/* The printing program: the manifest's Out reaches the executable, and
   the printed line lands on the process's standard output. */
static void prints_through_the_capability(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_print.eko", prog_print));
    status = run_cli("build build/cli_print.eko -o build/cli_print",
                     "build/cli_print.out", "build/cli_print.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_print.err", buf, sizeof buf) == 0);
    status = run_prog_out("./build/cli_print", "build/cli_print.run");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_print.run", buf, sizeof buf) == 10);
    EK_CHECK_STR(buf, "greetings\n");
}

/* The time companion through the build path: the parse family and the
   clock family link against a unit that names only the families it
   uses, and the built program runs. The test driver's own emission
   cannot see a missing family typeinfo, so this coverage lives on the
   build path. */
static void time_family_links_and_runs(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_time.eko", prog_time));
    status = run_cli("build build/cli_time.eko -o build/cli_time",
                     "build/cli_time.out", "build/cli_time.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_time.err", buf, sizeof buf) == 0);
    status = run_prog_out("./build/cli_time", "build/cli_time.run");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_time.run", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "2026-10-22T01:29:00Z\n");

    EK_CHECK(write_file("build/cli_clock.eko", prog_clock));
    status = run_cli("build build/cli_clock.eko -o build/cli_clock",
                     "build/cli_clock.out", "build/cli_clock.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_clock.err", buf, sizeof buf) == 0);
    status = run_prog_out("./build/cli_clock", "build/cli_clock.run");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_clock.run", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, "Mono ") == buf);
}

static void good_build(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_good.eko", prog_good));
    status = run_cli("build build/cli_good.eko -o build/cli_good",
                     "build/cli_good.out", "build/cli_good.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_good.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "wrote build/cli_good\n");
    /* a warning reports over a clean build, not only beside errors */
    EK_CHECK(read_file("build/cli_good.err", buf, sizeof buf) > 0);
    EK_CHECK(strstr(buf, ": W-unused: the binding `r` is never used")
                 != NULL);
    EK_CHECK(exited(run_prog("./build/cli_good"), 0));
}

/* The default output name is the input's stem in the current
   directory. */
static void default_output_name(void)
{
    char buf[256];
    int status;

    EK_CHECK(write_file("build/cli_stem.eko", prog_good));
    status = run_cli("build build/cli_stem.eko", "build/cli_stem.out",
                     "build/cli_stem.err");
    EK_CHECK(exited(status, 0));
    EK_CHECK(read_file("build/cli_stem.out", buf, sizeof buf) > 0);
    EK_CHECK_STR(buf, "wrote cli_stem\n");
    EK_CHECK(access("cli_stem", X_OK) == 0);
    EK_CHECK(access("cli_stem.c", R_OK) == 0);
    EK_CHECK(exited(run_prog("./cli_stem"), 0));
    (void)remove("cli_stem");
    (void)remove("cli_stem.c");
}

static void compile_error(void)
{
    char buf[512];
    int status;

    EK_CHECK(write_file("build/cli_bad.eko", prog_bad));
    (void)remove("build/cli_bad");
    status = run_cli("build build/cli_bad.eko -o build/cli_bad",
                     "build/cli_bad.out", "build/cli_bad.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_bad.err", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "build/cli_bad.eko:", 18) == 0);
    EK_CHECK(strstr(buf, ": E-") != NULL);
    EK_CHECK(read_file("build/cli_bad.out", buf, sizeof buf) == 0);
    EK_CHECK(access("build/cli_bad", X_OK) != 0);
}

static void missing_file(void)
{
    char buf[512];
    int status;

    status = run_cli("build build/cli_absent.eko -o build/cli_absent",
                     "build/cli_absent.out", "build/cli_absent.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_absent.err", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "build/cli_absent.eko:", 21) == 0);
    EK_CHECK(strstr(buf, ": E-") != NULL);
}

static void bad_usage(void)
{
    char buf[64];
    int status;

    status = run_cli("", "build/cli_usage.out", "build/cli_usage.err");
    EK_CHECK(exited(status, 2));
    EK_CHECK(read_file("build/cli_usage.err", buf, sizeof buf) > 0);
    EK_CHECK(strncmp(buf, "usage:", 6) == 0);

    status = run_cli("frobnicate", "build/cli_usage.out",
                     "build/cli_usage.err");
    EK_CHECK(exited(status, 2));

    status = run_cli("build", "build/cli_usage.out", "build/cli_usage.err");
    EK_CHECK(exited(status, 2));

    status = run_cli("build build/cli_good.eko --frobnicate",
                     "build/cli_usage.out", "build/cli_usage.err");
    EK_CHECK(exited(status, 2));

    status = run_cli("build build/cli_good.eko -o",
                     "build/cli_usage.out", "build/cli_usage.err");
    EK_CHECK(exited(status, 2));
}

/* The compiler's own root, four directory levels above the suite
   binary; 0 when the path cannot be resolved. */
static int compiler_root(char *root, size_t cap)
{
    char self[512];
    ssize_t n;
    char *slash;
    int len;
    int i;

    n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) {
        EK_CHECK(0);
        return 0;
    }
    self[n] = '\0';
    for (i = 0; i < 4; i++) {
        slash = strrchr(self, '/');
        if (slash == NULL) {
            EK_CHECK(0);
            return 0;
        }
        *slash = '\0';
    }
    len = snprintf(root, cap, "%s", self);
    if (len < 0 || (size_t)len >= cap) {
        EK_CHECK(0);
        return 0;
    }
    return 1;
}

/* The foreign-cwd build: compile a module under / through the
   compiler's absolute path, run the binary, and expect an empty error
   file. */
static void run_foreign_build(const char *root)
{
    char cmd[1024];
    char path[512];
    char buf[64];
    int len;
    int status;

    len = snprintf(cmd, sizeof cmd,
                   "cd / && %s/build/ekoc build %s/build/cli_foreign.eko"
                   " -o /tmp/ekoc_cli_foreign > %s/build/cli_foreign.out"
                   " 2> %s/build/cli_foreign.err",
                   root, root, root, root);
    if (len < 0 || (size_t)len >= sizeof cmd) {
        EK_CHECK(0);
        return;
    }
    len = snprintf(path, sizeof path, "%s/build/cli_foreign.eko", root);
    if (len < 0 || (size_t)len >= sizeof path) {
        EK_CHECK(0);
        return;
    }
    write_file(path, "main: () -> _ =\n    _\n");
    status = system(cmd);
    EK_CHECK(exited(status, 0));
    status = system("/tmp/ekoc_cli_foreign");
    EK_CHECK(exited(status, 0));
    len = snprintf(path, sizeof path, "%s/build/cli_foreign.err", root);
    if (len < 0 || (size_t)len >= sizeof path) {
        EK_CHECK(0);
        return;
    }
    EK_CHECK(read_file(path, buf, sizeof buf) == 0);
    remove("/tmp/ekoc_cli_foreign");
    remove("/tmp/ekoc_cli_foreign.c");
}

/* Building from a directory outside the tree: the runtime root is the
   compiler's own location, never the caller's working directory. */
static void builds_from_foreign_cwd(void)
{
    char root[512];

    if (!compiler_root(root, sizeof root)) {
        return;
    }
    run_foreign_build(root);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "cli/run: cannot create build/\n");
        return 1;
    }
    good_build();
    default_output_name();
    prints_through_the_capability();
    time_family_links_and_runs();
    builds_from_foreign_cwd();
    compile_error();
    missing_file();
    bad_usage();
    return ek_test_report("cli/run");
}
