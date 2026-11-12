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

static const char *prog_frames =
    "inner: (n: Int) -> Int =\n"
    "    case n:\n"
    "        0 -> panic(\"bottom\")\n"
    "        _ -> inner(n - 1)\n"
    "outer: (n: Int) -> Int = inner(n) * 2\n"
    "main: (out: Out) ! = out.println(outer(3).show())\n";

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

/* Runs the binary in a child whose standard error is the pipe; returns
   the wait status with the captured text in err. stack_kib caps the
   child's stack when nonzero. */
static int run_binary(const char *path, char *err, size_t cap,
                      size_t stack_kib)
{
    int fds[2];
    pid_t pid;
    int status = -1;

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
        if (stack_kib) {
            struct rlimit rl;

            rl.rlim_cur = stack_kib * 1024;
            rl.rlim_max = stack_kib * 1024;
            if (setrlimit(RLIMIT_STACK, &rl) != 0) {
                _exit(126);
            }
        }
        if (dup2(fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(fds[1]);
        execl(path, path, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    read_pipe(fds[0], err, cap);
    close(fds[0]);
    waitpid(pid, &status, 0);
    return status;
}

/* Runs the binary in a child whose standard output and standard error
   are the two pipes; returns the wait status with the captured text.
   The child of a printing gate writes a line or two, so reading the
   pipes in turn cannot deadlock. */
static int run_binary_capture(const char *path, char *out, size_t outcap,
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

/* Lowers, writes, compiles, and runs one program of fns functions
   with a capped child stack; returns the wait status and the captured
   standard error. */
static int gate_stack(const char *src, const char *name, size_t fns,
                      char *err, size_t cap, size_t stack_kib)
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
    status = run_binary(path, err, cap, stack_kib);
    EK_CHECK(status >= 0 && WIFEXITED(status));
    return status;
}

/* Lowers, writes, compiles, and runs one program of fns functions;
   returns the wait status and the captured standard error. */
static int gate(const char *src, const char *name, size_t fns, char *err,
                size_t cap)
{
    return gate_stack(src, name, fns, err, cap, 0);
}

/* Lowers, writes, compiles, and runs one printing program; returns the
   wait status with the captured standard output and error. */
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
    status = run_binary_capture(path, out, outcap, err, errcap);
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

/* The panic backtrace: innermost frame first, source-level names, one
   frame for the self tail caller (Clause 8.8.3), and the frame text of
   Annex B.6. */
static void panic_backtrace(void)
{
    char err[512];
    int status = gate(prog_frames, "lower_run_frames", 4, err, sizeof err);

    if (status < 0 || !WIFEXITED(status)) {
        return;
    }
    EK_CHECK_U64(WEXITSTATUS(status), 1);
    EK_CHECK(strstr(err, "panic [P-panic]: bottom\n") != NULL);
    /* the innermost frame reads the panic expression, the caller
       frames the call sites -- none reads the declaration */
    EK_CHECK(strstr(err, "  at test.inner (test.eko:3:14)\n") != NULL);
    EK_CHECK(strstr(err, "  at test.outer (test.eko:5:26)\n") != NULL);
    EK_CHECK(strstr(err, "  at test.main (test.eko:6:34)\n") != NULL);
    EK_CHECK(strstr(err, "at test.inner\n  at test.inner") == NULL);
    EK_CHECK(strstr(err, "ek_frame") == NULL);
}

/* An if or case whose every arm is a lambda binds the selected
   closure: calling it runs the arm, never a raw closure value. */
static void lambda_arms_value(void)
{
    char out[64], err[8];
    int status = gate_print("flag: Bool = True\n"
                            "pick: (n: Int) -> Int =\n"
                            "    case flag:\n"
                            "        True -> (m): m * 2\n"
                            "        False -> (m): m + 1\n"
                            "main: (out: Out) ! =\n"
                            "    g: () -> Int = if flag: ((): 5) "
                            "else: ((): 6)\n"
                            "    out.println(pick(21).show())\n"
                            "    out.println(g().show())\n"
                            "    _\n",
                            "lower_arms_value", 3, out, sizeof out, err,
                            sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "42\n5\n");
    EK_CHECK(err[0] == '\0');
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

/* A ten-thousand-element list folded by a recursive sum over the
   {h, ..t} arm: the self tail call runs at constant stack, so the
   recursion survives a child stack far smaller than ten thousand
   frames would need. */
static void deep_list(void)
{
    static char src[256 + 10000 * 7];
    char err[8];
    size_t used = 0;
    int i;
    int status;

    used += (size_t)snprintf(src, sizeof src,
        "sum: (xs: []Int, acc: Int) -> Int =\n"
        "    case xs:\n"
        "        {} -> acc\n"
        "        {h, ..t} -> sum(t, acc + h)\n"
        "main: () -> _ =\n"
        "    xs: []Int = {");
    for (i = 1; i <= 10000; i++) {
        used += (size_t)snprintf(src + used, sizeof src - used, "%s%d",
                                 i > 1 ? ", " : "", i);
    }
    snprintf(src + used, sizeof src - used,
             "}\n"
             "    r = sum(xs, 0)\n"
             "    d = 1 / (r - 50004999)\n"
             "    _ := d\n"
             "    _\n");
    status = gate_stack(src, "lower_run_deep", 2, err, sizeof err, 1024);
    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A hundred-thousand-deep self tail call: constant stack is the
   contract, so the recursion survives a child stack of a few hundred
   kilobytes where the eliminated frames would not. */
static void tail_recursion(void)
{
    static char src[256];
    char err[8];
    int status;

    snprintf(src, sizeof src,
        "count: (n: Int, acc: Int) -> Int =\n"
        "    case n:\n"
        "        0 -> acc\n"
        "        _ -> count(n - 1, acc + 1)\n"
        "main: () -> _ =\n"
        "    r = count(100000, 0)\n"
        "    d = 1 / (r - 99999)\n"
        "    _ := d\n"
        "    _\n");
    status = gate_stack(src, "lower_run_tail", 2, err, sizeof err, 512);
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

/* A lambda capturing two bindings is called through a function-typed
   parameter: the environment struct, the closure ABI, and the
   snapshot-at-creation discipline all hold up at runtime. */
static void lambda_capture(void)
{
    static const char *src =
        "twice: (f: (Int) -> Int, x: Int) -> Int = f(f(x))\n"
        "main: () -> _ =\n"
        "    a = 10\n"
        "    b = 3\n"
        "    h: (Int) -> Int = (x): x * a + b\n"
        "    r = twice(h, 2)\n"
        "    d = 1 / (r - 232)\n"
        "    _ := d\n"
        "    _\n";
    char err[8];
    int status = gate(src, "lower_run_lambda", 2, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A top-level function referenced as a value travels as a closure over
   its trampoline and is called through a function-typed parameter. */
static void function_value(void)
{
    static const char *src =
        "double: (x: Int) -> Int = x * 2\n"
        "apply: (f: (Int) -> Int, x: Int) -> Int = f(x)\n"
        "main: () -> _ =\n"
        "    r = apply(double, 21)\n"
        "    d = 1 / (r - 41)\n"
        "    _ := d\n"
        "    _\n";
    char err[8];
    int status = gate(src, "lower_run_value", 3, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A function value held by a top-level binding takes the closure call
   through its getter: the alias and the selected arm apply like any
   value, and the same binding passed as an argument keeps the value
   read. */
static void value_slot_call(void)
{
    char out[64], err[8];
    int status = gate_print("double: (x: Int) -> Int = x * 2\n"
                            "twice = double\n"
                            "pick = case 1:\n"
                            "    0 -> (m): m * 2\n"
                            "    _ -> (m): m + 1\n"
                            "apply: (f: (Int) -> Int, x: Int) -> Int = f(x)\n"
                            "main: (out: Out) ! =\n"
                            "    out.println(twice(21).show())\n"
                            "    out.println(pick(20).show())\n"
                            "    out.println(apply(twice, 5).show())\n"
                            "    _\n",
                            "lower_slot_call", 6, out, sizeof out, err,
                            sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "42\n21\n10\n");
    EK_CHECK(err[0] == '\0');
}

/* A guarded arm and an or-pattern discriminate at runtime: the probes
   panic unless every arm chose exactly. */
static void guarded_and_or(void)
{
    static const char *src =
        "Key: type = Up | Down | Left | Right\n"
        "axis: (k: Key) -> Bool =\n"
        "    case k:\n"
        "        Up | Down -> True\n"
        "        Left | Right -> False\n"
        "sign: (n: Int) -> Bool =\n"
        "    case n:\n"
        "        n if n < 0 -> True\n"
        "        n if n == 0 -> False\n"
        "        _ -> True\n"
        "pick: (k: Key, n: Int) -> Int =\n"
        "    case axis(k), sign(n):\n"
        "        True, True -> 1\n"
        "        True, False -> 2\n"
        "        False, True -> 3\n"
        "        False, False -> 4\n"
        "main: () -> _ =\n"
        "    w = 0\n"
        "    if pick(Up, 0 - 1) == 1:\n"
        "        if pick(Down, 5) == 1:\n"
        "            if pick(Left, 0) == 4:\n"
        "                if pick(Right, 9) == 3:\n"
        "                    w := 1\n"
        "                    _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char err[8];
    int status = gate(src, "lower_run_guard", 4, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A record brace pattern destructures through the generated struct:
   the probe division pins the fields the arm bound. */
static void record_pattern(void)
{
    static const char *src =
        "Human: type = [name: String, age: Int, city: String]\n"
        "Holder: type = Holder Human\n"
        "age_of: (h: Holder) -> Int =\n"
        "    case h:\n"
        "        Holder {name, age: a} -> a\n"
        "main: () -> _ =\n"
        "    h: Holder = Holder {name: \"ada\", age: 36, city: \"x\"}\n"
        "    d = 1 / (age_of(h) - 35)\n"
        "    _ := d\n"
        "    _\n";
    char err[8];
    int status = gate(src, "lower_run_record_pat", 2, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The inverted-binding chain: each `<-` step feeds the std Result
   combinators, the short-circuit paths return the error, and the
   successful chain sums. */
static void invert_chain(void)
{
    static const char *src =
        "chain: (xs: []Int) -> Result Int String =\n"
        "    a <- xs.get(0).to_result(\"missing a\").try()\n"
        "    b <- xs.get(1).to_result(\"missing b\").try()\n"
        "    Ok (a + b)\n"
        "probe: (xs: []Int) -> Int =\n"
        "    case chain(xs):\n"
        "        Ok n -> n\n"
        "        Err e -> 0 - 1\n"
        "main: () -> _ =\n"
        "    w = 0\n"
        "    if probe({7, 8}) == 15:\n"
        "        if probe({}) == 0 - 1:\n"
        "            if probe({7}) == 0 - 1:\n"
        "                w := 1\n"
        "                _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char err[8];
    int status = gate(src, "lower_run_invert", 6, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The logical operators are lazy: a right operand whose evaluation
   would panic never runs when the left operand decides the result. */
static void short_circuit(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    x = 1\n"
        "    r = x == 1 || 1 / (1 - x) == 0\n"
        "    q = x == 2 && 1 / (x - 2) == 0\n"
        "    w = 0\n"
        "    if r:\n"
        "        if q == False:\n"
        "            w := 1\n"
        "            _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char err[256];
    int status = gate(src, "lower_run_shortcut", 1, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* A self tail call in the right operand of `&&` and of `||` is a tail
   position of the function: both recursions run at constant stack. */
static void tail_through_logic(void)
{
    static const char *src =
        "down: (n: Int) -> Bool =\n"
        "    case n:\n"
        "        0 -> True\n"
        "        _ -> n > 0 && down(n - 1)\n"
        "up: (n: Int) -> Bool =\n"
        "    case n:\n"
        "        0 -> True\n"
        "        _ -> n == 100000 || up(n + 1)\n"
        "main: () -> _ =\n"
        "    w = 0\n"
        "    if down(100000):\n"
        "        if up(0):\n"
        "            w := 1\n"
        "            _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char err[8];
    int status = gate_stack(src, "lower_run_logic_tail", 3, err, sizeof err,
                            512);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The printing gate: `main` receives the Out capability by position,
   and the printed bytes reach the redirected standard output. */
static const char *prog_print =
    "main: (out: Out) ! = out.println(\"hello\")\n";

/* The `++` join feeding println: both operands evaluate to temps and
   the joined string travels to the bundle's write closure. */
static const char *prog_concat =
    "main: (out: Out) ! = out.println(\"foo\" ++ \"bar\")\n";

/* The zero-grant derivation: Out.null's own write lands nowhere, so
   the redirected standard output stays empty while the program exits
   cleanly. */
static const char *prog_null =
    "main: (out: Out) ! =\n"
    "    quiet = Out.null()\n"
    "    quiet.println(\"hush\")\n"
    "    _\n";

/* The observer derivation: the tee forwards every write through the
   bundle's own closure and then hands the written line to the
   observer, the terminator staying with the transport, so the line
   lands twice. */
static const char *prog_tee =
    "main: (out: Out) ! =\n"
    "    obs: (String) !-> _ = (s): out.println(s)\n"
    "    q = out.tee(obs)\n"
    "    q.println(\"both\")\n"
    "    _\n";

static void hello_world(void)
{
    char out[64], err[8];
    int status = gate_print(prog_print, "lower_run_print", 2, out,
                            sizeof out, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "hello\n");
    EK_CHECK(err[0] == '\0');
}

static void concat_feeds_print(void)
{
    char out[64], err[8];
    int status = gate_print(prog_concat, "lower_run_concat", 2, out,
                            sizeof out, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "foobar\n");
    EK_CHECK(err[0] == '\0');
}

static void null_prints_nothing(void)
{
    char out[64], err[8];
    int status = gate_print(prog_null, "lower_run_null", 3, out,
                            sizeof out, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "");
    EK_CHECK(err[0] == '\0');
}

static void tee_prints_twice(void)
{
    char out[64], err[8];
    int status = gate_print(prog_tee, "lower_run_tee", 12, out,
                            sizeof out, err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "both\nboth\n");
    EK_CHECK(err[0] == '\0');
}

/* The concurrency capability is supplied by the shim like any other:
   the manifest builds the Conc value and the four members lower. */
static void conc_manifest(void)
{
    lfx f;

    lfx_begin(&f,
              "main: (c: Conc) -> _ =\n"
              "    hold = c\n"
              "    _ := hold\n"
              "    _\n");
    EK_CHECK(lfx_check_clean(&f));
    EK_CHECK(lfx_lower_clean(&f));
    EK_CHECK(lfx_has(&f, "ek_cap_conc_os"));
    EK_CHECK(lfx_has(&f, "int main"));
    lfx_end(&f);
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
    lambda_arms_value();
        fprintf(stderr, "lower/run: cannot create build/\n");
        return 1;
    }
    arithmetic_program();
    panic_backtrace();
    division_by_zero();
    block_scoping();
    string_ownership();
    record_field();
    adt_construction();
    bind_forms();
    deep_list();
    tail_recursion();
    lambda_capture();
    function_value();
    value_slot_call();
    guarded_and_or();
    record_pattern();
    invert_chain();
    short_circuit();
    tail_through_logic();
    hello_world();
    concat_feeds_print();
    null_prints_nothing();
    tee_prints_twice();
    conc_manifest();
    return ek_test_report("lower/run");
}
