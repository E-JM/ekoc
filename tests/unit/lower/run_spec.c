#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gates for the generic specialization: a generic's
   instantiated body is compiled and run against the runtime, the
   applied instantiations answer their own types at runtime, and the
   stress gate runs the full std container surface over a
   two-hundred-thousand-element list under a capped child stack. The
   suite assumes the repository root as its working directory. */

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

/* Drains one pipe into the buffer, NUL-terminated and capped. */
static void read_pipe(int fd, char *buf, size_t cap)
{
    size_t len = 0;
    ssize_t got;

    while (len + 1 < cap) {
        got = read(fd, buf + len, cap - 1 - len);
        if (got <= 0) {
            break;
        }
        len += (size_t)got;
    }
    buf[len] = '\0';
}

/* Runs the binary in a child whose standard output and standard error
   are the two pipes and whose stack is capped when stack_kib is
   nonzero; returns the wait status with the captured text. The gates
   write at most a couple of megabytes, read after the child exits. */
static int run_capture(const char *path, char *out, size_t outcap,
                       char *err, size_t errcap, size_t stack_kib)
{
    int fds[2], efs[2];
    pid_t pid;
    int status = -1;

    out[0] = '\0';
    err[0] = '\0';
    if (pipe(fds) != 0 || pipe(efs) != 0) {
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        close(efs[0]);
        close(efs[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        close(efs[0]);
        if (stack_kib) {
            struct rlimit rl;

            rl.rlim_cur = stack_kib * 1024;
            rl.rlim_max = stack_kib * 1024;
            if (setrlimit(RLIMIT_STACK, &rl) != 0) {
                _exit(126);
            }
        }
        if (dup2(fds[1], STDOUT_FILENO) < 0
            || dup2(efs[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);
        close(efs[1]);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    close(efs[1]);
    read_pipe(fds[0], out, outcap);
    close(fds[0]);
    read_pipe(efs[0], err, errcap);
    close(efs[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Lowers, writes, compiles, and runs one printing program; returns
   the wait status with the captured standard output and error. */
static int gate_print(const char *src, const char *name, size_t fns,
                      char *out, size_t outcap, char *err, size_t errcap)
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
    status = run_capture(path, out, outcap, err, errcap, 0);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    return status;
}

/* The same gate with the child's stack capped at stack_kib. */
static int gate_stack_print(const char *src, const char *name, size_t fns,
                            char *out, size_t outcap, char *err,
                            size_t errcap, size_t stack_kib)
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
    status = run_capture(path, out, outcap, err, errcap, stack_kib);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    return status;
}

/* The failing scalar assertion: the panic names the two values the
   specialized body inspected. */
static void scalar_assert_fails(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    assert_eq(41, 42)\n"
        "    _\n";
    char out[8], err[256];
    int status = gate_print(src, "spec_run_int", 2, out, sizeof out, err,
                            sizeof err);
    const char *nl;

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 1);
    }
    EK_CHECK_STR(out, "");
    nl = strchr(err, '\n');
    if (nl) {
        EK_CHECK((size_t)(nl - err)
                     == strlen("panic [P-panic]: assert_eq failed: "
                               "41 != 42")
                 && strncmp(err, "panic [P-panic]: assert_eq failed: "
                                "41 != 42",
                            (size_t)(nl - err)) == 0);
    } else {
        EK_CHECK(0);
    }
}

/* The failing record assertion: the specialized body renders both
   values through the record's own inspect service. */
static void record_assert_fails(void)
{
    static const char *src =
        "Person: type = Person [name: String, age: Int]\n"
        "main: () -> _ =\n"
        "    a: Person = Person {name: \"ada\", age: 36}\n"
        "    b: Person = Person {name: \"ada\", age: 37}\n"
        "    assert_eq(a, b)\n"
        "    _\n";
    char out[8], err[256];
    int status = gate_print(src, "spec_run_rec", 2, out, sizeof out, err,
                            sizeof err);
    const char *nl;

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 1);
    }
    nl = strchr(err, '\n');
    if (nl) {
        EK_CHECK((size_t)(nl - err)
                     == strlen("panic [P-panic]: assert_eq failed: "
                               "Person {name: \"ada\", age: 36} != "
                               "Person {name: \"ada\", age: 37}")
                 && strncmp(err,
                            "panic [P-panic]: assert_eq failed: "
                            "Person {name: \"ada\", age: 36} != "
                            "Person {name: \"ada\", age: 37}",
                            (size_t)(nl - err)) == 0);
    } else {
        EK_CHECK(0);
    }
}

/* The stress gate: a two-hundred-thousand-element list runs through
   length, map, filter, fold, and join, the probe divisions pin every
   result, and the joined digits print exactly -- the whole traversal
   under a child stack of half a megabyte, the recursion the
   specialized bodies keep at constant depth. */
static void container_stress(void)
{
    static char src[512];
    static char want[1 << 21];
    static char out[1 << 21], err[8];
    size_t used = 0;
    int i;
    int status;

    snprintf(src, sizeof src,
        "main: (o: Out) ! =\n"
        "    xs = (1..=200000).to_list()\n"
        "    a = xs.map((x): x * 2)\n"
        "    b = a.filter((x): x > 4)\n"
        "    s = b.fold(0, (acc, x): acc + x)\n"
        "    n = xs.length()\n"
        "    w = 1 / (n - 199999)\n"
        "    w := 1 / (b.length() - 199997)\n"
        "    w := 1 / (s - 40000199993)\n"
        "    _ := w\n"
        "    o.println(xs.join(\",\"))\n"
        "    _\n");
    for (i = 1; i <= 200000; i++) {
        int wrote = snprintf(want + used, sizeof want - used, "%s%d",
                             i > 1 ? "," : "", i);

        if (wrote < 0 || (size_t)wrote >= sizeof want - used) {
            break;
        }
        used += (size_t)wrote;
    }
    want[used++] = '\n';
    status = gate_stack_print(src, "spec_run_stress", 13, out,
                              sizeof out, err, sizeof err, 512);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, want);
    EK_CHECK(err[0] == '\0');
}

/* Two instantiations of one generic in one program: the Int and the
   String join each render their own elements. */
static void two_instantiations(void)
{
    static const char *src =
        "main: (o: Out) ! =\n"
        "    ns: []Int = {1, 2, 3}\n"
        "    ss: []String = {\"x\", \"y\"}\n"
        "    o.println(ns.join(\"+\"))\n"
        "    o.println(ss.join(\"-\"))\n"
        "    _\n";
    char out[64], err[8];
    int status = gate_print(src, "spec_run_two", 10, out, sizeof out, err,
                            sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "1+2+3\nx-y\n");
    EK_CHECK(err[0] == '\0');
}

/* Nested composition: the wrapper is itself generic, and both inner
   maps instantiate from the wrapper's substituted body; a second
   applied type of the same wrapper keys its own instantiation. */
static void nested_composition(void)
{
    static const char *src =
        "twice: (xs: []t, f: (t) ?-> t) ?-> []t =\n"
        "    List.map(List.map(xs, f), f)\n"
        "main: (o: Out) ! =\n"
        "    ns: []Int = {1, 2}\n"
        "    ys = twice(ns, (x): x + 1)\n"
        "    ss: []String = {\"a\", \"b\"}\n"
        "    zs = twice(ss, (s): s ++ \"!\")\n"
        "    o.println(ys.join(\",\"))\n"
        "    o.println(zs.join(\",\"))\n"
        "    _\n";
    char out[64], err[8];
    int status = gate_print(src, "spec_run_nest", 16, out, sizeof out, err,
                            sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "3,4\na!!,b!!\n");
    EK_CHECK(err[0] == '\0');
}

/* A stable sort through the derived ordering of the element type: the
   merge walks with an accumulator, so deep inputs stay at constant
   stack, and the explicit comparator drives the same ordering
   service. */
static void sort_stable(void)
{
    static const char *src =
        "main: (o: Out) ! =\n"
        "    seed: []Int = {5, 3, 8, 1, 9, 2, 7}\n"
        "    sorted = seed.sort()\n"
        "    o.println(sorted.show())\n"
        "    down = seed.sort_by((a, b): a > b)\n"
        "    o.println(down.show())\n"
        "    _\n";
    char out[64], err[8];
    int status = gate_print(src, "spec_run_sort", 13, out, sizeof out, err,
                            sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "{1, 2, 3, 5, 7, 8, 9}\n{9, 8, 7, 5, 3, 2, 1}\n");
    EK_CHECK(err[0] == '\0');
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_spec: cannot create build/\n");
        return 1;
    }
    scalar_assert_fails();
    record_assert_fails();
    container_stress();
    two_instantiations();
    nested_composition();
    sort_stable();
    return ek_test_report("lower/run_spec");
}
