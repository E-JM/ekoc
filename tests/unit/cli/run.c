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
    EK_CHECK(read_file("build/cli_good.err", buf, sizeof buf) == 0);
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

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "cli/run: cannot create build/\n");
        return 1;
    }
    good_build();
    default_output_name();
    prints_through_the_capability();
    compile_error();
    missing_file();
    bad_usage();
    return ek_test_report("cli/run");
}
