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

/* The executable gate for interpolation and the range family: the
   emitted unit is written under build/, compiled by cc against the
   runtime archive, and runs in a forked child whose standard output
   and error are captured. The stack-capped runner pins the native
   iteration of the traversals. The suite assumes the repository root
   as its working directory. */

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

/* Runs the binary in a child with both pipes captured; stack_kib caps
   the child's stack when nonzero. */
static int run_binary(const char *path, char *out, size_t outcap,
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
    read_pipe(efs[0], err, errcap);
    close(fds[0]);
    close(efs[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Lowers, writes, compiles, and runs one program of fns functions
   with a capped child stack; the program must exit zero. */
static void gate_stack(const char *src, const char *name, size_t fns,
                       size_t stack_kib)
{
    lfx f;
    char path[64], out[8], err[512];
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
    status = run_binary(path, out, sizeof out, err, sizeof err, stack_kib);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(err, "");
}

/* Lowers, writes, compiles, and runs one program of fns functions;
   the captured standard output must equal want exactly and the exit
   must be clean. */
static void gate_print(const char *src, const char *name, size_t fns,
                       const char *want)
{
    lfx f;
    char path[64], out[4096], err[512];
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
    status = run_binary(path, out, sizeof out, err, sizeof err, 0);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, want);
    EK_CHECK_STR(err, "");
}

/* Lowers, writes, compiles, and runs one program expected to panic:
   the wait status and the panic line pin the failing element. */
static void gate_panic(const char *src, const char *name, size_t fns,
                       const char *code)
{
    lfx f;
    char path[64], out[8], err[512];
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
    status = run_binary(path, out, sizeof out, err, sizeof err, 0);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 1);
        EK_CHECK(strstr(err, code) != NULL);
    }
}

/* The slots render through the show of their row types: the Int and
   String bare, the Char bare, the Bool as True, the composite in its
   derived form, and the escaped braces verbatim. */
static void slot_rendering(void)
{
    gate_print("Money: type = [cents: Int, cur: String]\n"
               "main: (o: Out) ! =\n"
               "    n = 3\n"
               "    s = \"ada\"\n"
               "    c = 'x'\n"
               "    m: Money = {cents: 12500, cur: \"USD\"}\n"
               "    o.println(\"n = {n} and s = {s}\")\n"
               "    o.println(\"char {c}, bool {n == 3}\")\n"
               "    o.println(\"m = {m}\")\n"
               "    o.println(\"set \\{ is literal\")\n"
               "    o.println(\"outer {\"inner {n}\"}\")\n"
               "    _\n",
               "interp_print", 2,
               "n = 3 and s = ada\n"
               "char x, bool True\n"
               "m = {cents: 12500, cur: \"USD\"}\n"
               "set { is literal\n"
               "outer inner 3\n");
}

/* The million-element fold runs in constant C stack: the recursion
   the callback would cost stays one frame deep, so the traversal
   survives a child stack far smaller than a million frames. */
static void fold_native_iteration(void)
{
    gate_stack("main: () -> _ =\n"
               "    r = (1..=1000000).fold(0, (acc, i): acc + i)\n"
               "    w = 0\n"
               "    if r == 500000500000:\n"
               "        w := 1\n"
               "        _\n"
               "    d = 1 / w\n"
               "    _ := d\n"
               "    _\n",
               "interp_fold_big", 1, 512);
}

/* A Char range maps through the code-point bridge and the list
   filter keeps the order: the show pins the exact list form. */
static void map_filter_char(void)
{
    gate_print("main: (o: Out) ! =\n"
               "    r = ('a'..='f').map((c): c.to_code())\n"
               "    e = r.filter((n): n % 2 == 0)\n"
               "    o.println(e.show())\n"
               "    _\n",
               "interp_map_filter", 6,
               "{98, 100, 102}\n");
}

/* The short-circuit find_by stops at the deciding element: the
   predicate would panic with a division by zero on the next one. */
static void short_circuit(void)
{
    gate_stack("main: () -> _ =\n"
               "    xs = (1..=10).to_list()\n"
               "    v = case xs.find_by((i): i / (5 - i) == 4):\n"
               "        Some x -> x\n"
               "        None -> 0 - 1\n"
               "    w = 0\n"
               "    if v == 4:\n"
               "        w := 1\n"
               "        _\n"
               "    d = 1 / w\n"
               "    _ := d\n"
               "    _\n",
               "interp_shortcircuit", 2, 0);
}

/* The two operator forms and the empty span at the boundary, over
   the three endpoint domains, pinned by probe divisions. */
static void boundary_forms(void)
{
    gate_stack("main: () -> _ =\n"
               "    a = (1..5).fold(0, (acc, i): acc + i)\n"
               "    b = (1..=5).fold(0, (acc, i): acc + i)\n"
               "    n0 = (5..5).length()\n"
               "    n1 = (5..=5).length()\n"
               "    n2 = (3..1).length()\n"
               "    ch = ('a'..'c').length() * 10 + ('a'..='c').length()\n"
               "    u0: UInt = 1\n"
               "    u1: UInt = 4\n"
               "    us = (u0..=u1).fold(u0, (acc, i): acc + i)\n"
               "    w = 0\n"
               "    if a == 10:\n"
               "        if b == 15:\n"
               "            if n0 == 0:\n"
               "                if n1 == 1:\n"
               "                    if n2 == 0:\n"
               "                        if ch == 23:\n"
               "                            if us == 11:\n"
               "                                w := 1\n"
               "                                _\n"
               "    d = 1 / w\n"
               "    _ := d\n"
               "    _\n",
               "interp_bounds", 1, 0);
}

/* The traversal bounds: an empty range invokes nothing, a closed
   single element range invokes once, and the index visitor reaches
   the last position. A callback that divides by zero pins the
   invocation that happens. */
static void each_traversal(void)
{
    gate_stack("die: (v: Int) -> _ =\n"
               "    q = 1 / (v - v)\n"
               "    _\n"
               "main: () -> _ =\n"
               "    (5..5).each((v): die(v))\n"
               "    _\n",
               "interp_each_empty", 2, 0);
    gate_panic("die: (v: Int) -> _ =\n"
               "    q = 1 / (v - v)\n"
               "    _\n"
               "main: () -> _ =\n"
               "    (5..=5).each((v): die(v))\n"
               "    _\n",
               "interp_each_single", 2, "P-div");
    gate_panic("die_at: (v: Int, i: Int) -> _ =\n"
               "    if i == 2:\n"
               "        q = 1 / (i - 2)\n"
               "        _\n"
               "    else:\n"
               "        _\n"
               "main: () -> _ =\n"
               "    (1..=4).each_with_index((v, i): die_at(v, i))\n"
               "    _\n",
               "interp_each_index", 2, "P-div");
}

/* The range's own display is the operator form over both channels. */
static void range_display(void)
{
    gate_print("main: (o: Out) ! =\n"
               "    a = 1..9\n"
               "    b = 1..=9\n"
               "    c = 'a'..'c'\n"
               "    o.println(a.show())\n"
               "    o.println(b.show())\n"
               "    o.println(c.show())\n"
               "    o.println(a.inspect())\n"
               "    _\n",
               "interp_range_show", 2,
               "1..9\n"
               "1..=9\n"
               "'a'..'c'\n"
               "1..9\n");
    gate_stack("main: () -> _ =\n"
               "    a = 1..9\n"
               "    b = 1..=9\n"
               "    w = 0\n"
               "    if a.lt(b):\n"
               "        if b.gt(a):\n"
               "            if a.le(a):\n"
               "                if b.ge(b):\n"
               "                    w := 1\n"
               "                    _\n"
               "    d = 1 / w\n"
               "    _ := d\n"
               "    _\n",
               "interp_range_ord", 1, 0);
}

int main(void)
{
    slot_rendering();
    fold_native_iteration();
    map_filter_char();
    short_circuit();
    boundary_forms();
    each_traversal();
    range_display();
    return ek_test_report("lower/run_interp");
}
