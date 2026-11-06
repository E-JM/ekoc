#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gate for the structural keying: a dictionary keyed by
   constructed values hits and overwrites on structural equality, the
   record domains key the same way, the set vocabulary dedups over
   composite elements, and the Float rules of the key domain hold. The
   rig mirrors the one in run_rows.c, and the suite assumes the
   repository root as its working directory. */

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
                 "cc -std=c99 -O2 -pthread -Irt build/%s.c build/libekort.a"
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
   are the two pipes; returns the wait status with the captured text. */
static int run_binary(const char *path, char *out, size_t outcap,
                      char *err, size_t errcap)
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
   the wait status with the captured output and error. */
static int gate(const char *src, const char *name, size_t fns, char *out,
                size_t outcap, char *err, size_t errcap)
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
    status = run_binary(path, out, outcap, err, errcap);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    return status;
}

/* The `[Value]Value` shape: two structurally equal keys hit and
   overwrite, the distinct ones coexist, and the lookups answer through
   the members. */
static void value_keyed_dict_run(void)
{
    static const char *src =
        "Val: type = N | S String | P Int\n"
        "main: (out: Out) ! =\n"
        "    d: [Val]Int = {S \"insert\": 1, S \"len\": 2}\n"
        "    k: Val = P 1\n"
        "    d := d.insert(k, 3)\n"
        "    d := d.insert(P 1, 9)\n"
        "    out.println(d.length().show())\n"
        "    out.println(d.get(P 1).unwrap_or(0).show())\n"
        "    out.println(d.get(N).unwrap_or(0).show())\n"
        "    out.println(d.get(S \"insert\").unwrap_or(0).show())\n"
        "    out.println(d.get(S \"b\").unwrap_or(0).show())\n";
    char out[64], err[8];
    int status = gate(src, "lower_keys_values", 3, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "3\n9\n0\n1\n0\n");
    EK_CHECK(err[0] == '\0');
}

/* Declared and anonymous record keys: two equal-but-distinct objects
   are one key, and unequal ones coexist. */
static void record_keyed_dict_run(void)
{
    static const char *src =
        "Pt: type = [x: Int]\n"
        "main: (out: Out) ! =\n"
        "    d: [Pt]Int = {}\n"
        "    d := d.insert({x: 1}, 5)\n"
        "    d := d.insert({x: 1}, 6)\n"
        "    d := d.insert({x: 2}, 7)\n"
        "    e: [[x: Int, y: Int]]Int = {}\n"
        "    e := e.insert({x: 1, y: 2}, 8)\n"
        "    out.println(d.length().show())\n"
        "    out.println(d.get({x: 1}).unwrap_or(0).show())\n"
        "    out.println(d.get({x: 2}).unwrap_or(0).show())\n"
        "    out.println(e.get({x: 1, y: 2}).unwrap_or(0).show())\n"
        "    out.println(e.get({x: 2, y: 1}).unwrap_or(0).show())\n";
    char out[64], err[8];
    int status = gate(src, "lower_keys_records", 3, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "2\n6\n7\n8\n0\n");
    EK_CHECK(err[0] == '\0');
}

/* The set vocabulary over composite elements: of deduplicates keeping
   the first occurrences, contains is the domain check, remove is
   copy-on-insert. */
static void composite_set_run(void)
{
    static const char *src =
        "V: type = N | S String | P Int\n"
        "main: (out: Out) ! =\n"
        "    s := Set.of({S \"a\", S \"a\", N, P 1})\n"
        "    out.println(s.length().show())\n"
        "    out.println(show(s.contains(S \"a\")))\n"
        "    out.println(show(s.contains(P 1)))\n"
        "    out.println(show(s.contains(P 5)))\n"
        "    s2 := s.insert(P 1)\n"
        "    out.println(s2.length().show())\n"
        "    s3 := s2.remove(S \"a\")\n"
        "    out.println(s3.length().show())\n"
        "    out.println(show(s3.contains(S \"a\")))\n";
    char out[64], err[8];
    int status = gate(src, "lower_keys_set", 2, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "3\nTrue\nTrue\nFalse\n3\n2\nFalse\n");
    EK_CHECK(err[0] == '\0');
}

/* The Float rules of the key domain: a set holds 0.0 and -0.0 as one
   element, a NaN element never matches after its insert, and a number
   keyed by an ADT wrapper reads the same rules through the hooks. */
static void float_key_rules_run(void)
{
    static const char *src =
        "V: type = M Float | U\n"
        "main: (out: Out) ! =\n"
        "    f := Set.of({0.0, -0.0, 1.5})\n"
        "    n := Set.of({0.0 / 0.0})\n"
        "    out.println(f.length().show())\n"
        "    out.println(show(f.contains(-0.0)))\n"
        "    out.println(n.length().show())\n"
        "    out.println(show(n.contains(0.0 / 0.0)))\n"
        "    d: [V]Int = {}\n"
        "    d := d.insert(M 0.0, 1)\n"
        "    d := d.insert(M (-0.0), 2)\n"
        "    d := d.insert(M 2.5, 3)\n"
        "    out.println(d.length().show())\n"
        "    out.println(d.get(M (-0.0)).unwrap_or(0).show())\n";
    char out[64], err[8];
    int status = gate(src, "lower_keys_float", 3, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "2\nTrue\n1\nFalse\n2\n2\n");
    EK_CHECK(err[0] == '\0');
}

/* The key domains the stamping reaches: a bare zero-payload
   constructor keys a dictionary literal, a tuple-payload variant keys
   deterministically through its payload's own hash, and the scalar
   Float domain reads its numeric equality -- the two zeros one key, a
   NaN insert changing nothing. */
static void stamped_key_domains_run(void)
{
    static const char *src =
        "Dir: type = North | South\n"
        "Shape: type = Circle Float | Rect [Float, Float]\n"
        "main: (out: Out) ! =\n"
        "    d: [Dir]Int = {North: 1, South: 2}\n"
        "    out.println(d.get(North).unwrap_or(0).show())\n"
        "    s: [Shape]Int = {Rect {2.0, 3.0}: 5}\n"
        "    out.println(s.get(Rect {2.0, 3.0}).unwrap_or(0).show())\n"
        "    out.println(s.get(Shape.Rect {2.0, 4.0}).unwrap_or(0).show())\n"
        "    out.println(s.get(Shape.Circle 2.0).unwrap_or(0).show())\n"
        "    f: [Float]Int = {}\n"
        "    f := f.insert(0.0, 1)\n"
        "    f := f.insert(-0.0, 2)\n"
        "    f := f.insert(0.0 / 0.0, 9)\n"
        "    out.println(f.length().show())\n"
        "    out.println(f.get(-0.0).unwrap_or(0).show())\n";
    char out[64], err[8];
    int status = gate(src, "lower_keys_stamped", 3, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "1\n5\n0\n0\n1\n2\n");
    EK_CHECK(err[0] == '\0');
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_keys: cannot create build/\n");
        return 1;
    }
    value_keyed_dict_run();
    record_keyed_dict_run();
    composite_set_run();
    float_key_rules_run();
    stamped_key_domains_run();
    return ek_test_report("lower/run_keys");
}
