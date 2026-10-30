#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* The executable gate for the intrinsic dispatch: the routed rows run
   against the runtime, the wrapped Option and Result values travel
   through the case matches, and the probe divisions pin every value.
   The rig mirrors the one in run.c. */

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

/* Lowers, writes, compiles, and runs one program of fns functions;
   returns the wait status with the captured output and error. */
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

/* The successful parse takes the Ok arm and the payload computes. */
static void parse_ok(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    r = case Int.parse(\"42\"):\n"
        "        Ok n -> n\n"
        "        Err _ -> 0 - 1\n"
        "    w = 0\n"
        "    if r == 42:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_parse", 1, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The malformed input takes the Err arm: the runtime reports the
   kind, the wrap builds the ParseError, and the match never reads a
   payload. */
static void parse_err(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    r = case Int.parse(\"4x2\"):\n"
        "        Ok n -> n\n"
        "        Err _ -> 0 - 1\n"
        "    w = 0\n"
        "    if r == 0 - 1:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_parse_err", 1, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The scalar count over multibyte text and the accessor at one
   scalar index: the probes pin five scalars and the code of the
   second one. */
static void string_members(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    n = \"héllo\".length()\n"
        "    c = case \"héllo\".at(1):\n"
        "        Some ch -> ch.to_code()\n"
        "        None -> 0 - 1\n"
        "    w = 0\n"
        "    if n == 5:\n"
        "        if c == 233:\n"
        "            w := 1\n"
        "            _\n"
        "    d = 1 / w\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_string", 3, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The dictionary literal builds through the set chain, the count and
   the lookup route to the runtime, and the miss feeds the std
   default. */
static void dict_members(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    d: [Int]Int = {7: 30, 8: 12}\n"
        "    n = d.length()\n"
        "    v = case d.get(8):\n"
        "        Some x -> x\n"
        "        None -> 0 - 1\n"
        "    w = d.get(9).unwrap_or(5)\n"
        "    g = 0\n"
        "    if n == 2:\n"
        "        if v == 12:\n"
        "            if w == 5:\n"
        "                g := 1\n"
        "                _\n"
        "    a = 1 / g\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_dict", 2, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The narrowing boundaries: the fit reports Some, the overflow None,
   the float truncates toward zero, and the unsigned crossing comes
   back as the signed domain. */
static void narrowing_bounds(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    hi = 127\n"
        "    over = 300\n"
        "    a = case hi.to_i8():\n"
        "        Some _ -> 1\n"
        "        None -> 0\n"
        "    b = case over.to_i8():\n"
        "        Some _ -> 1\n"
        "        None -> 0\n"
        "    u: UInt = 40\n"
        "    c = case u.to_int():\n"
        "        Some n -> n\n"
        "        None -> 0 - 1\n"
        "    f = 3.7\n"
        "    g = case f.to_int():\n"
        "        Some n -> n\n"
        "        None -> 0 - 1\n"
        "    v = a * 1000 + (1 - b) * 100 + c * 10 + g\n"
        "    w = 0\n"
        "    if v == 1503:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_narrow", 1, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The byte bridge feeds the display channel: the chained rows print
   the hexadecimal form through the Out bundle. */
static void bytes_print(void)
{
    static const char *src =
        "main: (out: Out) ! =\n"
        "    out.println(\"hex = \" ++ \"AB\".to_bytes().to_hex())\n";
    char out[64], err[8];
    int status = gate(src, "lower_intr_print", 2, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
    EK_CHECK_STR(out, "hex = 4142\n");
    EK_CHECK(err[0] == '\0');
}

/* The radix parser reads the hexadecimal spelling back and the
   failing one misses. */
static void radix_parse(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    r = case Int.parse_hex(\"2a\"):\n"
        "        Ok n -> n\n"
        "        Err _ -> 0 - 1\n"
        "    s = case Int.parse_hex(\"2g\"):\n"
        "        Ok n -> n\n"
        "        Err _ -> 0 - 1\n"
        "    w = 0\n"
        "    if r == 42:\n"
        "        if s == 0 - 1:\n"
        "            w := 1\n"
        "            _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_radix", 1, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

/* The list append routes to the runtime spine: the appended tail
   shares and the count reads through the cells. */
static void list_append(void)
{
    static const char *src =
        "main: () -> _ =\n"
        "    xs: []Int = {1, 2}\n"
        "    ys = xs.append({3})\n"
        "    n = case ys.get(2):\n"
        "        Some a -> a\n"
        "        None -> 0 - 1\n"
        "    w = 0\n"
        "    if n == 3:\n"
        "        w := 1\n"
        "        _\n"
        "    d = 1 / w\n"
        "    _ := d\n"
        "    _\n";
    char out[8], err[8];
    int status = gate(src, "lower_intr_list", 2, out, sizeof out,
                      err, sizeof err);

    if (status >= 0 && WIFEXITED(status)) {
        EK_CHECK_U64(WEXITSTATUS(status), 0);
    }
}

int main(void)
{
    if (mkdir("build", 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "lower/run_intrinsic: cannot create build/\n");
        return 1;
    }
    parse_ok();
    parse_err();
    string_members();
    dict_members();
    narrowing_bounds();
    bytes_print();
    radix_parse();
    list_append();
    return ek_test_report("lower/run_intrinsic");
}
