#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gate for the lower stage's contract with ekort: the
   emitted unit is written under build/, compiled by cc against the
   runtime archive, and the binary runs in a forked child. The suite
   assumes the repository root as its working directory. */

static const char *prog_arith =
    "main: () -> _ =\n"
    "    a = 6\n"
    "    b = 7\n"
    "    r = a * b + 1\n"
    "    if r == 43:\n"
    "        r := r - 1\n"
    "        _\n"
    "    _\n";

static const char *prog_div =
    "main: () -> _ =\n"
    "    a = 6\n"
    "    b = 0\n"
    "    q = a / b\n"
    "    _\n";

/* The branch's binding dies with the branch: the outer `a` stays 1 and
   the division does not panic. */
static const char *prog_shadow =
    "main: () -> _ =\n"
    "    a = 1\n"
    "    if 1 == 1:\n"
    "        a = 0\n"
    "        _\n"
    "    q = 5 / a\n"
    "    _\n";

/* A heap value travels through a call and one binding: a broken
   retain/release balance would double-free and crash. */
static const char *prog_string =
    "greet: (n: Int) -> String = \"hi\"\n"
    "main: () -> _ =\n"
    "    s = greet(1)\n"
    "    _\n";

/* A declared record is constructed and its fields are read: the sum
   pins the layout, and a wrong offset reads garbage and panics at the
   probe division instead of exiting cleanly. */
static const char *prog_record =
    "Point: type = [x: Int, y: Int]\n"
    "main: () -> _ =\n"
    "    p: Point = {x: 3, y: 4}\n"
    "    r = p.x + p.y\n"
    "    w = 0\n"
    "    if r == 7:\n"
    "        w := 1\n"
    "        _\n"
    "    q = 1 / w\n"
    "    _ := q\n"
    "    _\n";

/* An algebraic type constructs through the std source bridge: both
   constructors are born, travel through a call, and die owned; a
   broken constructor or drop crashes instead of exiting cleanly. */
static const char *prog_adt =
    "sink: (o: Option Int) -> Int = 0\n"
    "main: () -> _ =\n"
    "    a = sink(Some 36)\n"
    "    b = sink(None)\n"
    "    c = sink(Some a)\n"
    "    _ := a\n"
    "    _ := b\n"
    "    _ := c\n"
    "    _\n";

/* The discard binding, the fresh-name rebind, and the rebind whose
   right-hand side is an if: the probe division pins the branch value. */
static const char *prog_bind =
    "main: () -> _ =\n"
    "    _ := 1 + 2\n"
    "    x = 5\n"
    "    x := if x == 5: 6 else: 7\n"
    "    y := 9\n"
    "    _ := x\n"
    "    _ := y\n"
    "    w = 0\n"
    "    if x == 6:\n"
    "        w := 1\n"
    "        _\n"
    "    q = 1 / w\n"
    "    _ := q\n"
    "    _\n";

static int write_unit(const char *path, const char *text, size_t len)
{
    FILE *fp = fopen(path, "wb");

    if (!fp) {
        return 0;
    }
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int compile_unit(const char *name)
{
    char cmd[256];
    int n;

    n = snprintf(cmd, sizeof cmd,
                 "cc -std=c99 -O2 -Irt build/%s.c build/libekort.a"
                 " -lm -o build/%s", name, name);
    if (n < 0 || (size_t)n >= sizeof cmd) {
        return 0;
    }
    return system(cmd) == 0;
}

/* Runs the binary in a child whose standard error is the pipe; returns
   the wait status with the captured text in err. */
static int run_binary(const char *path, char *err, size_t cap)
{
    int fds[2];
    pid_t pid;
    int status = -1;
    size_t len = 0;
    ssize_t got;

    err[0] = '\0';
    if (pipe(fds) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    while (len + 1 < cap) {
        got = read(fds[0], err + len, cap - 1 - len);
        if (got <= 0) {
            break;
        }
        len += (size_t)got;
    }
    err[len] = '\0';
    close(fds[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Lowers, writes, compiles, and runs one program of fns functions;
   returns the wait status and the captured standard error. */
static int gate(const char *src, const char *name, size_t fns, char *err,
                size_t cap)
{
    lfx f;
    char path[64];
    int status;

    lfx_begin(&f, src);
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(f.low.text != NULL);
    EK_CHECK_U64(f.low.fn_count, fns);
    snprintf(path, sizeof path, "build/%s.c", name);
    EK_CHECK(write_unit(path, f.low.text, f.low.len));
    lfx_end(&f);
    EK_CHECK(compile_unit(name));
    snprintf(path, sizeof path, "build/%s", name);
    status = run_binary(path, err, cap);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    return status;
}

static void arithmetic_program(void)
{
    char err[8];
    int status = gate(prog_arith, "lower_run_arith", 1, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

static void division_by_zero(void)
{
    char err[256];
    int status = gate(prog_div, "lower_run_div", 1, err, sizeof err);
    const char *line_end;

    if (status < 0 || !WIFEXITED(status)) {
        return;
    }
    EK_CHECK_U64(WEXITSTATUS(status), 1);
    line_end = strchr(err, '\n');
    if (line_end) {
        size_t n = (size_t)(line_end - err);

        EK_CHECK(n == strlen("panic [P-div]: division by zero")
                 && strncmp(err, "panic [P-div]: division by zero", n) == 0);
    } else {
        EK_CHECK(0);
    }
}

static void block_scoping(void)
{
    char err[8];
    int status = gate(prog_shadow, "lower_run_shadow", 1, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A declared record constructs, its fields read, and the probe
   division pins the field layout. */
static void record_field(void)
{
    char err[8];
    int status = gate(prog_record, "lower_run_record", 1, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* Both constructors of the std Option type construct, travel, and die
   owned; a broken constructor or drop crashes the child. */
static void adt_construction(void)
{
    char err[8];
    int status = gate(prog_adt, "lower_run_adt", 2, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The discard binding, the fresh rebind, and the if-valued rebind: the
   probe division pins the branch value the rebind produced. */
static void bind_forms(void)
{
    char err[8];
    int status = gate(prog_bind, "lower_run_bind", 1, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A ten-thousand-element list literal: construction folds from the nil
   end and the release consumes the spine, both in constant C stack. */
static void deep_list(void)
{
    static char src[256 + 10000 * 7];
    char err[8];
    size_t used = 0;
    int i;
    int status;

    used += (size_t)snprintf(src, sizeof src,
        "sum: (xs: []Int) -> Int = 0\n"
        "main: () -> _ =\n"
        "    xs: []Int = {");
    for (i = 1; i <= 10000; i++) {
        used += (size_t)snprintf(src + used, sizeof src - used, "%s%d",
                                 i > 1 ? ", " : "", i);
    }
    snprintf(src + used, sizeof src - used,
             "}\n"
             "    n = sum(xs)\n"
             "    d = 1 / (n + 1)\n"
             "    _ := d\n"
             "    _\n");
    status = gate(src, "lower_run_deep", 2, err, sizeof err);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

static void string_ownership(void)
{
    char err[8];
    int status = gate(prog_string, "lower_run_string", 2, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run: cannot create build/\n");
        return 1;
    }
    arithmetic_program();
    division_by_zero();
    block_scoping();
    string_ownership();
    record_field();
    adt_construction();
    bind_forms();
    deep_list();
    return ek_test_report("lower/run");
}
